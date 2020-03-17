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
import jsn.jsn as jsn


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

    msg = "Windows SDK version is not set! Please enter the version you'd like to use.\n" \
            "\tAvailable versions are in: Visual Studio > Project Properties > General > Windows SDK Version"
    util.print_status("warning", msg)

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
        util.print_status("warning", "Unable to locate visual studio, setting vs2017 as default")
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
        msg = "Unable to find 'vcvarsall.bat'! " \
                "Please enter the full path to the vc2017/vc2019 installation directory containing 'vcvarsall.bat'"
        util.print_status("warning", msg)

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

    msg = "Apple Developer Team ID is not set! This can be found on the Apple Developer Website.\n" \
            "Optionally, leave this blank to set one later in xcode: Project > Signing & Capabilities > Team\n" \
            "Please enter your ID (ie. 7C3Y44TX5K): "
    util.print_status("warning", msg)

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
        util.print_status("error", "File tasks must be an array of 2: [src, dst]")
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
        util.print_status("status", f"Setting vs_version to: {config['vs_version']}")
        return config
    else:
        if version not in supported_versions:
            msg = f"Unsupported visual studio version: {version}\n" \
                    "\tSupported version are: {supported_versions}"

            util.print_status("error", msg)


# copy files, directories or wildcards
def run_copy(config):
    util.print_header("copy")
    copy_tasks = config["copy"]
    for task in copy_tasks:
        files = get_task_files(task)
        for f in files:
            util.copy_file_create_dir_if_newer(f[0], f[1])


# convert jsn to json for use at runtime
def run_jsn(config):
    util.print_header("jsn")
    jsn_tasks = config["jsn"]
    ii = " -I "
    for i in jsn_tasks["import_dirs"]:
        ii += i + " "
    for task in jsn_tasks["files"]:
        files = get_task_files(task)
        for f in files:
            if not os.path.exists(f[0]):
                util.print_status("warning", f"Location '{f[0]}' does not exist")
                continue
            cmd = python_tool_to_platform(config["tools"]["jsn"])
            cmd += " -i " + f[0] + " -o " + f[1] + ii
            subprocess.call(cmd, shell=True)


# premake
def run_premake(config):
    util.print_header("premake")
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


# models
def run_models(config):
    util.print_header("models")
    tool_cmd = python_tool_to_platform(config["tools"]["models"])
    for task in config["models"]:
        task_files = get_task_files(task)
        mesh_opt = ""
        if os.path.exists(config["tools"]["mesh_opt"]):
            mesh_opt = config["tools"]["mesh_opt"]
        for f in task_files:
            cmd = " -i " + f[0] + " -o " + os.path.dirname(f[1])
            if len(mesh_opt) > 0:
                cmd += " -mesh_opt " + mesh_opt
            p = subprocess.Popen(tool_cmd + cmd, shell=True)
            p.wait()
    pass


# build third_party libs
def run_libs(config):
    util.print_header("libs")
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
    util.print_header("textures")
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
                dep_inputs = [f[0]]
                if fext in cont_fmt:
                    export = export_config_for_directory(f[0], "osx")
                    dep_inputs = get_container_dep_inputs(f[0], dep_inputs)
                dst = util.change_ext(f[1], ".dds").lower()
                if not dependencies.check_up_to_date_single(dst):
                    if "format" not in export.keys():
                        export["format"] = "RGBA8"
                    dep_outputs = [dst]
                    dep_info = dependencies.create_dependency_info(dep_inputs, dep_outputs)
                    dep = dict()
                    dep["files"] = list()
                    dep["files"].append(dep_info)
                    util.create_dir(dst)
                    cmd = tool_cmd + " "
                    cmd += "-f " + f[0] + " "
                    cmd += "-t " + export["format"] + " "
                    if "cubemap" in export.keys() and export["cubemap"]:
                        cmd += " --cubearray "
                    if "mips" in export.keys() and export["mips"]:
                        cmd += " --mips "
                    cmd += "-o " + dst
                    print("texturec " + f[0])
                    subprocess.call(cmd, shell=True)
                    dependencies.write_to_file_single(dep, util.change_ext(dst, ".json"))


# clean
def run_clean(config):
    util.print_header("clean")
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
        return
    wd = os.getcwd()
    md = {
        "profile": profile,
        "pmtech_dir": config["env"]["pmtech_dir"],
        "pmbuild": "cd " + wd + " && " + config["env"]["pmtech_dir"] + "pmbuild " + profile + " "
    }
    util.create_dir(config["data_dir"])
    f = open(os.path.join(config["data_dir"], "pmbuild_config.json"), "w+")
    f.write(json.dumps(md, indent=4))


# gets a commandline to setup vcvars for msbuil from command line
def setup_vcvars(config):
    return "pushd \ && cd \"" + config["vcvarsall_dir"] + "\" && vcvarsall.bat x86_amd64 && popd"


# run build commands
def run_build(config):
    util.print_header("build")
    for build_task in config["build"]:
        if util.get_platform_name() == "win32":
            build_task = setup_vcvars(config) + " && " + build_task
        p = subprocess.Popen(build_task, shell=True)
        e = p.wait()
        if e != 0:
            exit(0)


# top level help
def pmbuild_help(config):
    util.print_header("pmbuild -help", half=True)
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
    print("\n")


def clean_help(config):
    util.print_header("clean help", half=True)
    print("removes all intermediate and temp directories:")
    print("\njsn syntax: array of [directories to remove...].")
    print("clean: [")
    print("    [<rm dir>],")
    print("    ...")
    print("]")
    print("\n")


def vs_version_help(config):
    util.print_header("vs version help", half=True)
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
    util.print_header("libs help", half=True)
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
    util.print_header("premake help", half=True)
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
    util.print_header("pmfx help", half=True)
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
    util.print_header("models help", half=True)
    print("create binary pmm and pma model files from collada files:")
    print("\njsn syntax: array of [src, dst] pairs.")
    print("models: [")
    print("    [<src files, directories or wildcards>, <dst file or folder>],")
    print("    ...")
    print("]")
    print("accepted file formats: .dae, .obj")
    print("\n")


def textures_help(config):
    util.print_header("textures help", half=True)
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
    util.print_header("copy help", half=True)
    print("copy files from src to dst:")
    print("\njsn syntax: array of [src, dst] pairs.")
    print("copy: [")
    print("    [<src files, directories or wildcards>, <dst file or folder>],")
    print("    ...")
    print("]")
    print("\n")


def jsn_help(config):
    util.print_header("jsn help", half=True)
    print("convert jsn to json:")
    print("\njsn syntax: array of [src, dst] pairs.")
    print("jsn: [")
    print("    [<src files, directories or wildcards>, <dst file or folder>],")
    print("    ...")
    print("]")
    print("\n")


def build_help(config):
    util.print_header("build help", half=True)
    print("\njsn syntax: array of commands.")
    print("build: [")
    print(" command args args args,")
    print("    ...")
    print("]")
    print("\n")


# print duration of job, ts is start time
def print_duration(ts):
    millis = int((time.time() - ts) * 1000)
    print("")
    util.print_header(f"Took {millis}ms", half=True)
    print("")


# main function
def main():
    # must have config.json in working directory
    if not os.path.exists("config.jsn"):
        util.print_status("error", "Unable to find 'config.json' in current directory")
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


# entry point of pmbuild
if __name__ == "__main__":
    util.print_header("pmbuild (v3)")
    print("")
    main()
