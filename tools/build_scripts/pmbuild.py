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
    bj = open("config_user.jsn", "w+")
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
            bj = open("config_user.jsn", "w+")
            bj.write(json.dumps(config, indent=4))
            bj.close()
            return
        else:
            time.sleep(1)


# configure user settings for each platform
def configure_user(config):
    config_user = dict()
    if os.path.exists("config_user.jsn"):
        config_user = jsn.loads(open("config_user.jsn", "r").read())
    if util.get_platform_name() == "win32":
        configure_vc_vars_all(config_user)
        configure_windows_sdk(config_user)
    if os.path.exists("config_user.jsn"):
        config_user = jsn.loads(open("config_user.jsn", "r").read())
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


# get files for task, will iterate dirs, match wildcards or return single files
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
    pass


# third_party libs
def run_thirdparty(config):
    shell_build = ["linux", "osx", "ios"]
    third_party_folder = os.path.join(config["env"]["pmtech_dir"], "third_party")
    third_party_build = ""
    if util.get_platform_name() in shell_build:
        third_party_build = "cd " + third_party_folder + "; ./build_libs.sh " + util.get_platform_name()
    else:
        args = ""
        args += config["env"]["pmtech_dir"] + "/" + " "
        args += config["sdk_version"] + " "
        third_party_build += "cd " + third_party_folder + "&& build_libs.bat \"" + config["vcvarsall_dir"] + "\"" + " " + args
    return third_party_build


# textures
def run_textures(config):
    print("--------------------------------------------------------------------------------")
    print("textures -----------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    tool_cmd = tool_to_platform(config["tools"]["texturec"])
    for task in config["textures"]:
        files = get_task_files(task)
        for f in files:
            copy_fmt = [".dds", ".pmv"]
            conv_fmt = [".png", ".jpg", ".tga", ".bmp"]
            if os.path.splitext(f[1])[1] in copy_fmt:
                util.copy_file_create_dir_if_newer(f[0], f[1])
            if os.path.splitext(f[1])[1] in conv_fmt:
                export = export_config_for_file(f[0])
                dst = util.change_ext(f[1], ".dds")
                util.create_dir(dst)
                cmd = tool_cmd + " "
                cmd += "-f " + f[0] + " "
                cmd += "-t " + export["format"] + " "
                cmd += "-o " + dst
                print("texturec " + f[0])
                subprocess.call(cmd, shell=True)


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


# entry point of pmbuild
if __name__ == "__main__":
    print("--------------------------------------------------------------------------------")
    print("pmbuild (v3) -------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")

    # must have config.json in working directory
    if not os.path.exists("config.jsn"):
        print("[error] no config.json in current directory.")
        exit(1)

    # load jsn, inherit etc
    config_all = jsn.loads(open("config.jsn", "r").read())

    # first arg is build profile
    config = config_all[sys.argv[1]]

    # load config user for user specific values (sdk version, vcvarsall.bat etc.)
    configure_user(config)
    print(json.dumps(config, indent=4))

    # tasks are executed in order they are declared
    tasks = collections.OrderedDict()
    tasks["premake"] = run_premake
    tasks["pmfx"] = run_pmfx
    tasks["models"] = run_models
    tasks["textures"] = run_textures
    tasks["copy"] = run_copy

    # clean is a special task, you must specify separately
    if "-clean" in sys.argv:
        run_clean(config)

    # run tasks in order they are specified.
    for key in tasks.keys():
        if key not in config.keys():
            continue
        if "-all" in sys.argv and "-n" + key not in sys.argv:
            tasks.get(key, lambda config: '')(config)
        elif len(sys.argv) != 2 and "-" + key in sys.argv:
            tasks.get(key, lambda config: '')(config)
        elif len(sys.argv) == 2:
            tasks.get(key, lambda config: '')(config)
