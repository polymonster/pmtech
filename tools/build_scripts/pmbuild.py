import collections
import sys
import os.path
import json
import fnmatch
import util
import subprocess
import platform
import shutil
import jsn.jsn as jsn


# returns tool to run from cmdline with .exe
def tool_to_platform(tool):
    tool = util.sanitize_file_path(tool)
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


# copy files, directories or wildcards
def run_copy(config):
    print("--------------------------------------------------------------------------------")
    print("copy ---------------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")
    copy_tasks = config["copy"]
    for task in copy_tasks:
        if len(task) != 2:
            print("[error] copy tasks must be an array of size 2 [src, dst]")
            exit(1)
        fn = task[0].find("*")
        if fn != -1:
            # wildcards
            fnroot = task[0][:fn-1]
            for root, dirs, files in os.walk(fnroot):
                for file in files:
                    src = util.sanitize_file_path(os.path.join(root, file))
                    if fnmatch.fnmatch(src, task[0]):
                        dst = src.replace(util.sanitize_file_path(fnroot), util.sanitize_file_path(task[1]))
                        util.copy_file_create_dir_if_newer(src, dst)
        elif os.path.isdir(task[0]):
            # dir
            for root, dirs, files in os.walk(task[0]):
                for file in files:
                    src = util.sanitize_file_path(os.path.join(root, file))
                    dst = src.replace(util.sanitize_file_path(task[0]), util.sanitize_file_path(task[1]))
                    util.copy_file_create_dir_if_newer(src, dst)
        else:
            # single file
            util.copy_file_create_dir_if_newer(task[0], task[1])


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
    print(cmd)
    subprocess.call(cmd, shell=True)


# pmfx
def run_pmfx(config):
    cmd = python_tool_to_platform(config["tools"]["pmfx"])
    for c in config["pmfx"]:
        cmd += " " + c
    print(cmd)
    subprocess.call(cmd, shell=True)


# models
def run_models(config):
    pass


# textures
def run_textures(config):
    pass


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

    # tasks are executed in order they are declared
    tasks = collections.OrderedDict()
    tasks["premake"] = run_premake
    tasks["pmfx"] = run_pmfx
    tasks["models"] = run_models
    tasks["textures"] = run_textures
    tasks["copy"] = run_copy

    # first arg is build profile
    config = config_all[sys.argv[1]]
    print(json.dumps(config_all, indent=4))

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
