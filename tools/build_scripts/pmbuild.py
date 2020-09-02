import collections
import sys
import os.path
import json
import fnmatch
import util
import subprocess
import platform
import shutil
import time
import dependencies
import glob
import threading
import jsn.jsn as jsn
import cgu.cgu as cgu


# returns tool to run from cmdline with .exe
def tool_to_platform(tool):
    tool = util.sanitize_file_path(tool)
    tool = tool.replace("$platform", util.get_platform_name())
    if platform.system() == "Windows":
        tool += ".exe"
    return tool


# ensure running with python3 or py -3
def python_tool_to_platform(tool):
    tool = util.sanitize_file_path(tool)
    if platform.system() == "Windows":
        tool = "py -3 " + tool
    else:
        tool = "python3 " + tool
    return tool


# ches if file is excluded based on know files to ignore
def is_excluded(file):
    excluded_files = [".DS_Store"]
    for ex in excluded_files:
        if file.find(ex) != -1:
            return True
    return False


# writes a required value input by the user, into config.user.jsn
def update_user_config(k, v, config):
    config[k] = v
    user = dict()
    if os.path.exists("config.user.jsn"):
        user = jsn.loads(open("config.user.jsn", "r").read())
    user[k] = v
    bj = open("config.user.jsn", "w+")
    bj.write(json.dumps(user, indent=4))
    bj.close()


# locate latest version of the windows sdk
def locate_windows_sdk():
    pf_env = ["PROGRAMFILES", "PROGRAMFILES(X86)"]
    sdk = "Windows Kits"
    sdk_dir = None
    for v in pf_env:
        print(v)
        d = os.environ[v]
        if d:
            if sdk in os.listdir(d):
                print(sdk)
                print(d)
                sdk_dir = os.path.join(d, sdk)
                break
    if sdk_dir:
        versions = sorted(os.listdir(sdk_dir), reverse=False)
        if len(versions) > 0:
            if versions[0] == "10":
                # windows 10 has sub versions
                source = os.path.join(sdk_dir, versions[0], "Source")
                if os.path.exists(source):
                    sub_versions = sorted(os.listdir(source), reverse=False)
                    if len(sub_versions) > 0:
                        return str(sub_versions[0])
            else:
                # 8.1
                return str(versions[0])
    return None


# windows only, prompt user to supply their windows sdk version
def configure_windows_sdk(config):
    if "sdk_version" in config.keys():
        return
    # attempt to auto locate
    auto_sdk = locate_windows_sdk()
    if auto_sdk:
        update_user_config("sdk_version", auto_sdk, config)
        return
    print("Windows SDK version not set.")
    print("Please enter the windows sdk you want to use.")
    print("You can find available sdk versions in:")
    print("Visual Studio > Project Properties > General > Windows SDK Version.")
    input_sdk = str(input())
    update_user_config("sdk_version", input_sdk, config)
    return


# find visual studio installation directory
def locate_vs_root():
    pf_env = ["PROGRAMFILES", "PROGRAMFILES(X86)"]
    vs = "Microsoft Visual Studio"
    vs_dir = ""
    for v in pf_env:
        d = os.environ[v]
        if d:
            if vs in os.listdir(d):
                vs_dir = os.path.join(d, vs)
                break
    return vs_dir


# find latest visual studio version
def locate_vs_latest():
    vs_dir = locate_vs_root()
    if len(vs_dir) == 0:
        print("[warning]: could not auto locate visual studio, using vs2017 as default")
        return "vs2017"
    supported = ["2017", "2019"]
    versions = sorted(os.listdir(vs_dir), reverse=False)
    for v in versions:
        if v in supported:
            return "vs" + v


# attempt to locate vc vars all by lookin in prgoram files, and finding visual studio installations
def locate_vc_vars_all():
    vs_dir = locate_vs_root()
    if len(vs_dir) == 0:
        return None
    pattern = os.path.join(vs_dir, "**/vcvarsall.bat")
    # if we reverse sort then we get the latest vs version
    vc_vars = sorted(glob.glob(pattern, recursive=True), reverse=False)
    if len(vc_vars) > 0:
        return vc_vars[0]
    return None


# windows only, configure vcvarsall directory for commandline vc compilation
def configure_vc_vars_all(config):
    # already exists
    if "vcvarsall_dir" in config.keys():
        if os.path.exists(config["vcvarsall_dir"]):
            return
    # attempt to auto locate
    auto_vc_vars = locate_vc_vars_all()
    if auto_vc_vars:
        auto_vc_vars = os.path.dirname(auto_vc_vars)
        update_user_config("vcvarsall_dir", auto_vc_vars, config)
        return
    # user input
    while True:
        print("Cannot find 'vcvarsall.bat'")
        print("Please enter the full path to the vc2017/vc2019 installation directory containing vcvarsall.bat")
        input_dir = str(input())
        input_dir = input_dir.strip("\"")
        input_dir = os.path.normpath(input_dir)
        if os.path.isfile(input_dir):
            input_dir = os.path.dirname(input_dir)
        if os.path.exists(input_dir):
            update_user_config("vcvarsall_dir", input_dir, config)
            return
        else:
            time.sleep(1)


# apple only, ask user for their team id to insert into xcode projects
def configure_teamid(config):
    if "teamid" in config.keys():
        return
    print("Apple Developer Team ID not set.")
    print("Please enter your development team ID ie. (7C3Y44TX5K)")
    print("You can find team id's or personal team id on the Apple Developer website")
    print("Optionally leave this blank and you select a team later in xcode:")
    print("  Project > Signing & Capabilities > Team")
    input_sdk = str(input())
    update_user_config("teamid", input_sdk, config)
    return


# configure user settings for each platform
def configure_user(config, args):
    config_user = dict()
    if os.path.exists("config.user.jsn"):
        config_user = jsn.loads(open("config.user.jsn", "r").read())
    if util.get_platform_name() == "win32":
        if "-msbuild" not in sys.argv:
            configure_vc_vars_all(config_user)
            configure_windows_sdk(config_user)
    if os.path.exists("config.user.jsn"):
        config_user = jsn.loads(open("config.user.jsn", "r").read())
        util.merge_dicts(config, config_user)


# look for export.json in directory tree, combine and override exports by depth, override further by fnmatch
def export_config_for_directory(filedir, platform):
    filepath = util.sanitize_file_path(filedir)
    dirtree = filepath.split(os.sep)
    export_dict = dict()
    subdir = ""
    for i in range(0, len(dirtree)):
        subdir = os.path.join(subdir, dirtree[i])
        export = os.path.join(subdir, "export.jsn")
        if os.path.exists(export):
            dir_dict = jsn.loads(open(export, "r").read())
            util.merge_dicts(export_dict, dir_dict)
    if platform in export_dict.keys():
        util.merge_dicts(export_dict, export_dict[platform])
    return export_dict


# get file specific export config from the directory config checking for fnmatch on the basename
def export_config_for_file(filename):
    dir_config = export_config_for_directory(os.path.dirname(filename), "osx")
    bn = os.path.basename(filename)
    for k in dir_config.keys():
        if fnmatch.fnmatch(k, bn):
            file_dict = dir_config[k]
            util.merge_dicts(dir_config, file_dict)
    return dir_config


# get files for task, will iterate dirs, match wildcards or return single files, returned in tuple (src, dst)
def get_task_files(task):
    outputs = []
    if len(task) != 2:
        print("[error] file tasks must be an array of size 2 [src, dst]")
        exit(1)
    fn = task[0].find("*")
    if fn != -1:
        # wildcards
        fnroot = task[0][:fn - 1]
        for root, dirs, files in os.walk(fnroot):
            for file in files:
                src = util.sanitize_file_path(os.path.join(root, file))
                if is_excluded(src):
                    continue
                if fnmatch.fnmatch(src, task[0]):
                    dst = src.replace(util.sanitize_file_path(fnroot), util.sanitize_file_path(task[1]))
                    outputs.append((src, dst))
    elif os.path.isdir(task[0]):
        # dir
        for root, dirs, files in os.walk(task[0]):
            for file in files:
                src = util.sanitize_file_path(os.path.join(root, file))
                if is_excluded(src):
                    continue
                dst = src.replace(util.sanitize_file_path(task[0]), util.sanitize_file_path(task[1]))
                outputs.append((src, dst))
    else:
        # single file
        if not is_excluded(task[0]):
            outputs.append((task[0], task[1]))
    return outputs


# get files for a task sorted by directory
def get_task_files_containers(task):
    container_ext = ".cont"
    files = get_task_files(task)
    container_files = []
    skip = 0
    for fi in range(0, len(files)):
        if fi < skip:
            continue
        f = files[fi]
        cpos = f[0].find(container_ext)
        if cpos != -1:
            container_name = f[0][:cpos + len(container_ext)]
            export = export_config_for_directory(container_name, "osx")
            container_src = container_name + "/container.txt"
            container_dst = os.path.dirname(f[1])
            container_dir = os.path.dirname(f[0])
            cf = (container_src, container_dst)
            file_list = ""
            # list of files in json
            if "files" in export:
                for xf in export["files"]:
                    file_list += os.path.join(container_name, xf) + "\n"
            # otherwise take all files in the directory
            else:
                dir_files = sorted(os.listdir(container_dir))
                for xf in dir_files:
                    if xf.endswith(".jsn") or xf.endswith(".DS_Store") or xf.endswith(".txt"):
                        continue
                    file_list += os.path.join(container_name, xf) + "\n"
            update_container = False
            if os.path.exists(container_src):
                cur_container = open(container_src, "r").read()
                if cur_container != file_list:
                    update_container = True
            else:
                update_container = True
            if update_container:
                open(container_src, "w+").write(file_list)
            container_files.append(cf)
            for gi in range(fi+1, len(files)):
                ff = files[gi]
                cur_container_name = ff[0][:cpos + len(container_ext)]
                if cur_container_name != container_name:
                    skip = gi
                    break
        else:
            container_files.append(f)
    return container_files


# gets a list of files within container to track in dependencies
def get_container_dep_inputs(container_filepath, dep_inputs):
    cf = open(container_filepath, "r").read().split("\n")
    for cff in cf:
        dep_inputs.append(cff)
    return dep_inputs


# set visual studio version for building
def run_vs_version(config):
    supported_versions = [
        "vs2017",
        "vs2019"
    ]
    version = config["vs_version"]
    if version == "latest":
        config["vs_version"] = locate_vs_latest()
        print("setting vs_version to: " + config["vs_version"])
        return config
    else:
        if version not in supported_versions:
            print("[error]: unsupported visual studio version " + str(version))
            print("    supported versions are " + str(supported_versions))


# copy files, directories or wildcards
def run_copy(config):
    print("--------------------------------------------------------------------------------")
    print("copy ---------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    copy_tasks = config["copy"]
    for task in copy_tasks:
        files = get_task_files(task)
        for f in files:
            util.copy_file_create_dir_if_newer(f[0], f[1])


# single jsn job to run on a thread
def run_jsn_thread(f, ii, config, jsn_tasks):
    cmd = python_tool_to_platform(config["tools"]["jsn"])
    cmd += " -i " + f[0] + " -o " + f[1] + ii
    imports = jsn.get_import_file_list(f[0], jsn_tasks["import_dirs"])
    inputs = [f[0], config["tools"]["jsn"]]
    for im in imports:
        inputs.append(im)
    dep = dependencies.create_dependency_info(inputs, [f[1]], cmd)
    if not dependencies.check_up_to_date_single(f[1], dep):
        subprocess.call(cmd, shell=True)
        dependencies.write_to_file_single(dep, util.change_ext(f[1], ".dep"))


# convert jsn to json for use at runtime
def run_jsn(config):
    print("--------------------------------------------------------------------------------")
    print("jsn ----------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    threads = []
    jsn_tasks = config["jsn"]
    ii = " -I "
    for i in jsn_tasks["import_dirs"]:
        ii += i + " "
    for task in jsn_tasks["files"]:
        files = get_task_files(task)
        for f in files:
            if not os.path.exists(f[0]):
                print("[warning]: file or directory " + f[0] + " does not exist!")
                continue
            x = threading.Thread(target=run_jsn_thread, args=(f, ii, config, jsn_tasks))
            threads.append(x)
            x.start()
    for t in threads:
        t.join()


# premake
def run_premake(config):
    print("--------------------------------------------------------------------------------")
    print("premake ------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    cmd = tool_to_platform(config["tools"]["premake"])
    for c in config["premake"]:
        if c == "vs_version":
            c = config["vs_version"]
        cmd += " " + c
    # add pmtech dir
    cmd += " --pmtech_dir=\"" + config["env"]["pmtech_dir"] + "\""
    # add sdk version for windows
    if "sdk_version" in config.keys():
        cmd += " --sdk_version=\"" + str(config["sdk_version"]) + "\""
    # check for teamid
    if "require_teamid" in config:
        if config["require_teamid"]:
            configure_teamid(config)
            cmd += " --teamid=\"" + config["teamid"] + "\""
    subprocess.call(cmd, shell=True)


# pmfx
def run_pmfx(config):
    cmd = python_tool_to_platform(config["tools"]["pmfx"])
    for c in config["pmfx"]:
        cmd += " " + c
    subprocess.call(cmd, shell=True)


# single model build / optimise ran on a separate thread
def run_models_thread(cmd):
    p = subprocess.Popen(cmd, shell=True)
    p.wait()


# models
def run_models(config):
    print("--------------------------------------------------------------------------------")
    print("models -------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    tool_cmd = python_tool_to_platform(config["tools"]["models"])
    threads = []
    for task in config["models"]:
        task_files = get_task_files(task)
        mesh_opt = ""
        if os.path.exists(config["tools"]["mesh_opt"]):
            mesh_opt = config["tools"]["mesh_opt"]
        for f in task_files:
            cmd = " -i " + f[0] + " -o " + os.path.dirname(f[1])
            if len(mesh_opt) > 0:
                cmd += " -mesh_opt " + mesh_opt
            x = threading.Thread(target=run_models_thread, args=(tool_cmd + cmd,))
            threads.append(x)
            x.start()
    for t in threads:
        t.join()


# build third_party libs
def run_libs(config):
    print("--------------------------------------------------------------------------------")
    print("libs ---------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    shell = ["linux", "osx", "ios"]
    cmd = ""
    for arg in config["libs"]:
        cmd = arg
        if util.get_platform_name() in shell:
            pass
        else:
            args = ""
            args += config["env"]["pmtech_dir"] + "/" + " "
            args += config["sdk_version"] + " "
            if "vs_version" not in config:
                config["vs_version"] = "vs2017"
            args += config["vs_version"] + " "
            cmd += "\"" + config["vcvarsall_dir"] + "\"" + " " + args
        print(cmd)
        p = subprocess.Popen(cmd, shell=True)
        p.wait()


# textures
def run_textures(config):
    print("--------------------------------------------------------------------------------")
    print("textures -----------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    tool_cmd = tool_to_platform(config["tools"]["texturec"])
    for task in config["textures"]:
        files = get_task_files_containers(task)
        for f in files:
            copy_fmt = [".dds", ".pmv"]
            conv_fmt = [".png", ".jpg", ".tga", ".bmp", ".txt"]
            cont_fmt = [".txt"]
            fext = os.path.splitext(f[0])[1]
            if fext in copy_fmt:
                util.copy_file_create_dir_if_newer(f[0], f[1])
            if fext in conv_fmt:
                export = export_config_for_file(f[0])
                dep_inputs = [f[0], config["tools"]["texturec"]]
                if fext in cont_fmt:
                    export = export_config_for_directory(f[0], "osx")
                    dep_inputs = get_container_dep_inputs(f[0], dep_inputs)
                dst = util.change_ext(f[1], ".dds").lower()
                # to refactor
                if "format" not in export.keys():
                    export["format"] = "RGBA8"
                cmd = tool_cmd + " "
                cmd += "-f " + f[0] + " "
                cmd += "-t " + export["format"] + " "
                if "cubemap" in export.keys() and export["cubemap"]:
                    cmd += " --cubearray "
                if "mips" in export.keys() and export["mips"]:
                    cmd += " --mips "
                cmd += "-o " + dst
                dep = dependencies.create_dependency_info(dep_inputs, [dst], cmd)
                if not dependencies.check_up_to_date_single(dst, dep):
                    util.create_dir(dst)
                    subprocess.call(cmd, shell=True)
                    dependencies.write_to_file_single(dep, util.change_ext(dst, ".dep"))


# cleans directories specified in config["clean"]
def run_clean(config):
    print("--------------------------------------------------------------------------------")
    print("clean --------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    for clean_task in config["clean"]:
        if os.path.isfile(clean_task):
            print("file " + clean_task)
            os.remove(clean_task)
        elif os.path.isdir(clean_task):
            print("directory " + clean_task)
            shutil.rmtree(clean_task)


# generates metadata json to put in data root dir, for doing hot loading and other re-build tasks
def generate_pmbuild_config(config, profile):
    if "data_dir" not in config:
        print("[error]: did not generate pmbuild_config.json for live reloading")
        return
    wd = os.getcwd()
    pmd = util.sanitize_file_path(config["env"]["pmtech_dir"])
    md = {
        "profile": profile,
        "pmtech_dir": pmd,
        "pmbuild": "cd " + wd + " && " + pmd + "pmbuild " + profile + " "
    }
    util.create_dir(config["data_dir"])
    np = os.path.join(config["data_dir"], "pmbuild_config.json")
    np = os.path.normpath(np)
    f = open(np, "w+")
    f.write(json.dumps(md, indent=4))


# gets a commandline to setup vcvars for msbuild from command line
def setup_vcvars(config):
    return "pushd \ && cd \"" + config["vcvarsall_dir"] + "\" && vcvarsall.bat x86_amd64 && popd"


# run build commands.. build is now deprecated in favour of 'make'
def run_build(config):
    print("--------------------------------------------------------------------------------")
    print("build --------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    for build_task in config["build"]:
        if util.get_platform_name() == "win32":
            build_task = setup_vcvars(config) + " && " + build_task
        p = subprocess.Popen(build_task, shell=True)
        e = p.wait()
        if e != 0:
            exit(0)


# generate a cli command for building with different toolchains (make, gcc/clang, xcodebuild, msbuild)
def make_for_toolchain(jsn_config, options):
    make_config = jsn_config["make"]
    toolchain = make_config["toolchain"]
    exts = {
        "make": ".make",
        "emmake": ".make",
        "xcodebuild": ".xcodeproj",
        "msbuild": ".vcxproj"
    }
    ext = exts[toolchain]
    strip_ext = ["make", "emmake"]

    # first option is always target, it can be 'all' or a single build
    targets = []
    if options[0] == "all":
        for file in os.listdir(make_config["dir"]):
            if file.endswith(ext):
                targets.append(file)
    else:
        if toolchain not in strip_ext:
            targets.append(options[0] + ext)
        else:
            targets.append(options[0])

    # msbuild needs vcvars all
    setup_env = ""
    if toolchain == "msbuild":
        setup_env = setup_vcvars(jsn_config) + " &&"

    cmds = {
        "make": "make",
        "emmake": "emmake make",
        "xcodebuild": "xcodebuild",
        "msbuild": "msbuild"
    }
    cmd = cmds[toolchain]

    target_options = {
        "make": "",
        "emmake": "",
        "xcodebuild": "-project ",
        "msbuild": ""
    }
    target_option = target_options[toolchain]

    configs = {
        "make": "config=",
        "emmake": "config=",
        "xcodebuild": "-configuration ",
        "msbuild": "/p:Configuration="
    }
    config = ""

    # parse other options
    extra_args = ""
    for option in options[1:]:
        # config
        if option.find("config=") != -1:
            config = configs[toolchain]
            config += option.replace("config=", "")
        else:
            # pass through any additional platform specific args
            extra_args += option

    # build final cli command
    make_commands = []
    for target in targets:
        cmdline = setup_env + " " + cmd + " " + target_option + " " + target + " " + config + " " + extra_args
        make_commands.append(cmdline)

    return make_commands


# runs make, and compiles from makefiles, vs solution or xcode project.
def run_make(config, options):
    print("--------------------------------------------------------------------------------")
    print("make ---------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    cwd = os.getcwd()
    if "make" not in config.keys():
        print("[error] make config missing from config.jsn ")
        return
    if len(options) == 0:
        print("[error] no make target specified")
        return
    make_commands = make_for_toolchain(config, options)
    # cd to the build dir
    os.chdir(config["make"]["dir"])
    if len(options) == 0:
        print("[error] no make target specified")
    else:
        for mc in make_commands:
            subprocess.call(mc, shell=True)
    os.chdir(cwd)


# launches and exectuable program from the commandline
def run_exe(config, options):
    print("--------------------------------------------------------------------------------")
    print("run ----------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    cwd = os.getcwd()
    if "run" not in config.keys():
        print("[error] run config missing from config.jsn ")
        return
    run_config = config["run"]
    if len(options) == 0:
        print("[error] no run target specified")
        return
    targets = []
    if options[0] == "all":
        for file in os.listdir(run_config["dir"]):
            if file.endswith(run_config["ext"]):
                targets.append(os.path.splitext(file)[0])
    else:
        targets.append(options[0])
    # switch to bin dir
    os.chdir(run_config["dir"])
    for t in targets:
        cmd = run_config["cmd"]
        cmd = cmd.replace("%target%", t)
        for o in options[1:]:
            cmd += " " + o
        print(cmd)
        subprocess.call(cmd, shell=True)
    os.chdir(cwd)


# generates function pointer bindings to call pmtech from a live reloaded dll.
def run_cr(config):
        print("--------------------------------------------------------------------------------")
        print("cr -----------------------------------------------------------------------------")
        print("--------------------------------------------------------------------------------")
        print(config["cr"]["output"])
        files = config["cr"]["files"]
        free_funcs = []
        added = []
        for f in files:
            source = open(f, "r").read()
            source = cgu.remove_comments(source)
            strings, source = cgu.placeholder_string_literals(source)
            functions, function_names = cgu.find_functions(source)
            for func in functions:
                free = len(func["qualifier"]) == 0
                for s in func["scope"]:
                    if s["type"] == "struct":
                        free = False
                        break
                # cant add members
                if not free:
                    continue
                # cant add overloads
                if func["name"] in added:
                    continue
                func["file"] = os.path.basename(f)
                added.append(func["name"])
                free_funcs.append(func)

        # start writing code
        code = cgu.src_line("// codegen_2")
        code += cgu.src_line("#pragma once")
        for f in files:
            bn = os.path.basename(f)
            code += cgu.src_line('#include ' + cgu.in_quotes(bn))

        code += cgu.src_line("using namespace pen;")
        code += cgu.src_line("using namespace put;")
        code += cgu.src_line("using namespace pmfx;")
        code += cgu.src_line("using namespace ecs;")
        code += cgu.src_line("using namespace dbg;")

        # sort by immediate scope
        scope_funcs = dict()
        for f in free_funcs:
            ff = f["file"]
            l = len(f["scope"])
            if l > 0:
                s = f["scope"][l-1]["name"]
                if s not in scope_funcs.keys():
                    scope_funcs[s] = list()
                scope_funcs[s].append(f)

        # add bindings grouped by scope
        for scope in scope_funcs:
            # function pointer typedefs
            for f in scope_funcs[scope]:
                args = cgu.get_funtion_prototype(f)
                code += cgu.src_line("typedef " + f["return_type"] + " (*proc_" + f["name"] + ")" + args + ";")
            # struct
            struct_name = "__" + scope
            code += cgu.src_line("struct " + struct_name + " {")
            code += cgu.src_line("void* " + struct_name + "_start;")
            # function pointers members
            for f in scope_funcs[scope]:
                code += cgu.src_line("proc_" + f["name"] + " " + f["name"] + ";")
            code += cgu.src_line("void* " + struct_name + "_end;")
            code += cgu.src_line("};")

        # pointers to contexts
        inherit = ""
        for scope in scope_funcs:
            if len(inherit) > 0:
                inherit += ", "
            inherit += "public __" + scope
        code += cgu.src_line("struct live_context:")
        code += cgu.src_line(inherit + "{")
        code += cgu.src_line("f32 dt;")
        code += cgu.src_line("ecs::ecs_scene* scene;")
        for scope in scope_funcs:
            code += cgu.src_line("__" + scope + "* " + scope + "_funcs;")
        code += cgu.src_line("live_context() {")
        # bind function pointers to addresses
        code += cgu.src_line("#if !DLL")
        for scope in scope_funcs:
            for f in scope_funcs[scope]:
                full_scope = ""
                for q in f["scope"]:
                    if q["type"] == "namespace":
                        full_scope += q["name"] + "::"
                code += cgu.src_line(f["name"] + " = &" + full_scope + f["name"] + ";")
        code += cgu.src_line("#endif")
        code += cgu.src_line("}")

        code += cgu.src_line("};")
        output_file = open(config["cr"]["output"], "w")
        output_file.write(cgu.format_source(code, 4))
        return


# top level help
def pmbuild_help(config):
    print("pmbuild -help ------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("\nusage: pmbuild <profile> <tasks...>")
    print("\noptions:")
    print("    -help (display this dialog).")
    print("    -<task> -help (display task help).")
    print("    -cfg (print jsn config for current profile).")
    print("    -msbuild (indicates msbuild prompt and no need to call vcvarsall.bat")
    print("\nprofiles:")
    print("    config.jsn (edit task settings in here)")
    for p in config.keys():
        print(" " * 8 + p)
    print("\ntasks (in order of execution):")
    print("    -all (builds all tasks).")
    print("    -n<task name> (excludes task).")
    print("    -clean (delete specified directories).")
    print("    -libs (build thirdparty libs).")
    print("    -premake (run premake, generate ide projects).")
    print("    -models (convert to binary model, skeleton and material format).")
    print("    -pmfx (shader compilation, code-gen, meta-data gen).")
    print("    -textures (convert, compress, generate mip-maps, arrays, cubemaps).")
    print("    -copy (copy files, folders or wildcards) [src, dst].")
    print("    -build (build code) [src, dst]. deprecated use make instead.")
    print("\nexplicit tasks (must specify flag, not included in -all):")
    print("    -make (runs make, xcodebuild, msbuild) <target> <config> <flags>")
    print("    -run (runs exe) <target> <options>")
    print("\n")


def clean_help(config):
    print("clean help ---------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("removes all intermediate and temp directories:")
    print("\njsn syntax: array of [directories to remove...].")
    print("clean: [")
    print("    [<rm dir>],")
    print("    ...")
    print("]")
    print("\n")


def vs_version_help(config):
    print("vs version help ---------------------------------------------------------------")
    print("-------------------------------------------------------------------------------")
    print("select version of visual studio for building libs and porjects:")
    print("\njsn syntax:")
    print("vs_version: <version>")
    print("\n")
    print("version options:")
    print("    latest (will choose latest version installed on your machine)")
    print("    vs2017 (minimum supported compiler)")
    print("    vs2019")
    print("\n")


def libs_help(config):
    print("libs help ----------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("builds tools and third-party libraries:")
    print("\njsn syntax: array of [cmdlines, ..]")
    print("libs: [")
    print("    [\"command line\"],")
    print("    ...")
    print("]\n")
    print("reguires:")
    print("    config[\"env\"][\"pmtech_dir\"]")
    print("    win32:")
    print("        config[\"sdk_version\"]")
    print("        config[\"vcvarsall_dir\"]")
    print("\n")


def premake_help(config):
    print("premake help -------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("generate ide projects or make files from lua descriptions:")
    print("\njsn syntax: array of [<action>, cmdline options..]")
    print("premake: [")
    print("    [\"<action> (vs2017, xcode4, gmake, android-studio)\"],")
    print("    [\"--premake_option <value>\"],")
    print("    ...")
    print("]\n")
    print("reguires: config[\"env\"][\"pmtech_dir\"]\n")
    cmd = tool_to_platform(config["tools"]["premake"])
    cmd += " --help"
    subprocess.call(cmd, shell=True)
    print("\n")


def pmfx_help(config):
    print("pmfx help ----------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("compile platform specific shaders:")
    print("\njsn syntax: array of [cmdline options, ..]")
    print("pmfx: [")
    print("    [\"-pmfx_option <value>\"],")
    print("    ...")
    print("]\n")
    cmd = python_tool_to_platform(config["tools"]["pmfx"])
    cmd += " -help"
    subprocess.call(cmd, shell=True)
    print("\n")


def models_help(config):
    print("models help --------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("create binary pmm and pma model files from collada files:")
    print("\njsn syntax: array of [src, dst] pairs.")
    print("models: [")
    print("    [<src files, directories or wildcards>, <dst file or folder>],")
    print("    ...")
    print("]")
    print("accepted file formats: .dae, .obj")
    print("\n")


def textures_help(config):
    print("textures help ------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("convert, re-size or compress textures:")
    print("\njsn syntax: array of [src, dst] pairs.")
    print("copy: [")
    print("    [<src files, directories or wildcards>, <dst file or folder>],")
    print("    ...")
    print("]")
    print("export.jsn:")
    print("{")
    print("    format: \"RGBA8\"")
    print("    filename.png {")
    print("        format: \"override_per_file\"")
    print("    }")
    print("}\n")
    tool_cmd = tool_to_platform(config["tools"]["texturec"])
    subprocess.call(tool_cmd + " --help", shell=True)
    subprocess.call(tool_cmd + " --formats", shell=True)
    print("\n")


def copy_help(config):
    print("copy help ----------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("copy files from src to dst:")
    print("\njsn syntax: array of [src, dst] pairs.")
    print("copy: [")
    print("    [<src files, directories or wildcards>, <dst file or folder>],")
    print("    ...")
    print("]")
    print("\n")


def jsn_help(config):
    print("jsn help ----------------------------------------------------------------------")
    print("-------------------------------------------------------------------------------")
    print("convert jsn to json:")
    print("\njsn syntax: array of [src, dst] pairs.")
    print("jsn: [")
    print("    [<src files, directories or wildcards>, <dst file or folder>],")
    print("    ...")
    print("]")
    print("\n")


def build_help(config):
    print("build help ----------------------------------------------------------------------")
    print("---------------------------------------------------------------------------------")
    print("\njsn syntax: array of commands.")
    print("build: [")
    print(" command args args args,")
    print("    ...")
    print("]")
    print("\n")


def make_help(config):
    print("make help ----------------------------------------------------------------------")
    print("---------------------------------------------------------------------------------")
    print("\njsn syntax: array of commands.")
    print("make: {")
    print(" toolchain: <make, emmake, xcodebuild, msbuild>")
    print(" dir: <path to makefiles, xcodeproj, etc>")
    print("]")
    print("\ncommandline options.")
    print("-make <target> <config> <flags>")
    print("    target can be all, or basic_texture etc.")
    print("config=<debug, release, etc>")
    print("any additional flags after these will be forwarded to the build toolchain")
    print("\n")


def cr_help(config):
    print("cr help -------------------------------------------------------------------------")
    print("---------------------------------------------------------------------------------")
    print("generate cfunction pointers for calling from fungos/cr")
    print("\njsn syntax: array of commands.")
    print("cr: {")
    print("    files:[...], output: <filepath>")
    print("}")
    print("\n")


# print duration of job, ts is start time
def print_duration(ts):
    millis = int((time.time() - ts) * 1000)
    print("--------------------------------------------------------------------------------")
    print("Took (" + str(millis) + "ms)")


# stub for jobs to do nothing
def stub(config):
    pass


# main function
def main():
    start_time = time.time()

    # must have config.json in working directory
    if not os.path.exists("config.jsn"):
        print("[error] no config.json in current directory.")
        exit(1)

    # load jsn, inherit etc
    config_all = jsn.loads(open("config.jsn", "r").read())

    # top level help
    if "-help" in sys.argv or len(sys.argv) == 1:
        if len(sys.argv) <= 2:
            pmbuild_help(config_all)
            exit(0)

    call = "run"
    if "-help" in sys.argv:
        call = "help"

    # first arg is build profile
    if call == "run":
        if sys.argv[1] not in config_all:
            print("[error] " + sys.argv[1] + " is not a valid pmbuild profile")
            exit(0)
        config = config_all[sys.argv[1]]
        # load config user for user specific values (sdk version, vcvarsall.bat etc.)
        configure_user(config, sys.argv)
        if "-cfg" in sys.argv:
            print(json.dumps(config, indent=4))
    else:
        config = config_all["base"]

    # tasks are executed in order they are declared here
    tasks = collections.OrderedDict()
    tasks["vs_version"] = {"run": run_vs_version, "help": vs_version_help}
    tasks["libs"] = {"run": run_libs, "help": libs_help}
    tasks["premake"] = {"run": run_premake, "help": premake_help}
    tasks["pmfx"] = {"run": run_pmfx, "help": pmfx_help}
    tasks["models"] = {"run": run_models, "help": models_help}
    tasks["textures"] = {"run": run_textures, "help": textures_help}
    tasks["jsn"] = {"run": run_jsn, "help": jsn_help}
    tasks["copy"] = {"run": run_copy, "help": copy_help}
    tasks["build"] = {"run": run_build, "help": build_help}
    tasks["cr"] = {"run": run_cr, "help": cr_help}
    tasks["make"] = {"run": stub, "help": make_help}

    # clean is a special task, you must specify separately
    if "-clean" in sys.argv:
        if call == "help":
            clean_help(config)
        else:
            run_clean(config)

    # run tasks in order they are specified.
    for key in tasks.keys():
        if call == "run":
            if key not in config.keys():
                continue
        ts = time.time()
        run = False
        # check flags to include or exclude jobs
        if "-all" in sys.argv and "-n" + key not in sys.argv:
            run = True
        elif len(sys.argv) != 2 and "-" + key in sys.argv:
            run = True
        elif len(sys.argv) == 2:
            run = True
        # run job
        if run:
            tasks.get(key, lambda config: '')[call](config)
            print_duration(ts)

    # finally metadata for rebuilding and hot reloading
    generate_pmbuild_config(config, sys.argv[1])

    # make
    if "-make" in sys.argv:
        i = sys.argv.index("-make") + 1
        options = []
        while i < len(sys.argv):
            options.append(sys.argv[i])
            i += 1
        if call == "help":
            pass
        else:
            run_make(config, options)

    if "-run" in sys.argv:
        i = sys.argv.index("-run") + 1
        options = []
        while i < len(sys.argv):
            options.append(sys.argv[i])
            i += 1
        if call == "help":
            pass
        else:
            run_exe(config, options)


    print("--------------------------------------------------------------------------------")
    print("all jobs complete --------------------------------------------------------------")
    print_duration(start_time)


# entry point of pmbuild
if __name__ == "__main__":
    print("--------------------------------------------------------------------------------")
    print("pmbuild (v3) -------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("")
    main()
