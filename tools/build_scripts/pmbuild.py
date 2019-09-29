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


# windows only, prompt user to supply their windows sdk version
def configure_windows_sdk(config):
    if "sdk_version" in config.keys():
        return
    print("Windows SDK version not set.")
    print("Please enter the windows sdk you want to use.")
    print("You can find available sdk versions in:")
    print("Visual Studio > Project Properties > General > Windows SDK Version.")
    input_sdk = str(input())
    config["sdk_version"] = input_sdk
    bj = open("config.user.jsn", "w+")
    bj.write(json.dumps(config, indent=4))
    bj.close()
    return


# windows only, configure vcvarsall directory for commandline vc compilation
def configure_vc_vars_all(config):
    if "vcvarsall_dir" in config.keys():
        if os.path.exists(config["vcvarsall_dir"]):
            return
    while True:
        print("Cannot find 'vcvarsall.bat'")
        print("Please enter the full path to the vc2017/vc2019 installation directory containing vcvarsall.bat")
        input_dir = str(input())
        input_dir = input_dir.strip("\"")
        input_dir = os.path.normpath(input_dir)
        if os.path.exists(input_dir):
            config["vcvarsall_dir"] = input_dir
            bj = open("config.user.jsn", "w+")
            bj.write(json.dumps(config, indent=4))
            bj.close()
            return
        else:
            time.sleep(1)


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
            util.copy_file_create_dir_if_newer(task[0], task[1])
            outputs.append((task[0], task[1]))
    return outputs


# get files for a task sorted by directory
def get_task_files_containers(task):
    container_ext = ".cube"
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
            cf = (container_src, container_dst)
            file_list = ""
            for xf in export["files"]:
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


# premake
def run_premake(config):
    print("--------------------------------------------------------------------------------")
    print("premake ------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    cmd = tool_to_platform(config["tools"]["premake"])
    for c in config["premake"]:
        cmd += " " + c
    # add pmtech dir
    cmd += " --pmtech_dir=\"" + config["env"]["pmtech_dir"] + "\""
    subprocess.call(cmd, shell=True)


# pmfx
def run_pmfx(config):
    cmd = python_tool_to_platform(config["tools"]["pmfx"])
    for c in config["pmfx"]:
        cmd += " " + c
    subprocess.call(cmd, shell=True)


# models
def run_models(config):
    print("--------------------------------------------------------------------------------")
    print("models -------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    tool_cmd = python_tool_to_platform(config["tools"]["models"])
    for task in config["models"]:
        task_files = get_task_files(task)
        for f in task_files:
            cmd = " -i " + f[0] + " -o " + os.path.dirname(f[1])
            subprocess.call(tool_cmd + cmd, shell=True)
    pass


# build third_party libs
def run_libs(config):
    print("--------------------------------------------------------------------------------")
    print("libs ---------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    shell = ["linux", "osx", "ios"]
    cmd = ""
    for arg in config["libs"]:
        cmd += arg + " "
        print(arg)
    if util.get_platform_name() in shell:
        cmd += util.get_platform_name()
    else:
        args = ""
        args += config["env"]["pmtech_dir"] + "/" + " "
        args += config["sdk_version"] + " "
        cmd += "\"" + config["vcvarsall_dir"] + "\"" + " " + args
        print(cmd)
    subprocess.call(cmd, shell=True)


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
                dep_inputs = [f[0]]
                if fext in cont_fmt:
                    export = export_config_for_directory(f[0], "osx")
                    dep_inputs = get_container_dep_inputs(f[0], dep_inputs)
                dst = util.change_ext(f[1], ".dds")
                if not dependencies.check_up_to_date_single(dst):
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
                    cmd += "-o " + dst
                    print("texturec " + f[0])
                    subprocess.call(cmd, shell=True)
                    dependencies.write_to_file_single(dep, util.change_ext(dst, ".json"))


# clean
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
    print("\n")


def clean_help(config):
    print("clean help ---------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("\njsn syntax: array of [directories to remove...].")
    print("clean: [")
    print("    [<rm dir>],")
    print("    ...")
    print("]")
    print("\n")


def libs_help(config):
    print("libs help ----------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
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
    print("\njsn syntax: array of [src, dst] pairs.")
    print("copy: [")
    print("    [<src files, directories or wildcards>, <dst file or folder>],")
    print("    ...")
    print("]")
    print("\n")


# print duration of job, ts is start time
def print_duration(ts):
    millis = int((time.time() - ts) * 1000)
    print("--------------------------------------------------------------------------------")
    print("Took (" + str(millis) + "ms)")


# entry point of pmbuild
if __name__ == "__main__":
    print("--------------------------------------------------------------------------------")
    print("pmbuild (v3) -------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    print("")

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
        config = config_all[sys.argv[1]]
        # load config user for user specific values (sdk version, vcvarsall.bat etc.)
        configure_user(config, sys.argv)
        if "-cfg" in sys.argv:
            print(json.dumps(config, indent=4))
    else:
        config = config_all["base"]

    # tasks are executed in order they are declared here
    tasks = collections.OrderedDict()
    tasks["libs"] = {"run": run_libs,  "help": libs_help}
    tasks["premake"] = {"run": run_premake, "help": premake_help}
    tasks["pmfx"] = {"run": run_pmfx, "help": pmfx_help}
    tasks["models"] = {"run": run_models, "help": models_help}
    tasks["textures"] = {"run": run_textures, "help": textures_help}
    tasks["copy"] = {"run": run_copy, "help": copy_help}

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
        if "-all" in sys.argv and "-n" + key not in sys.argv:
            tasks.get(key, lambda config: '')[call](config)
            print_duration(ts)
        elif len(sys.argv) != 2 and "-" + key in sys.argv:
            tasks.get(key, lambda config: '')[call](config)
            print_duration(ts)
        elif len(sys.argv) == 2:
            tasks.get(key, lambda config: '')[call](config)
            print_duration(ts)
