import os
import subprocess
import os.path
import sys
import shutil
import json
import build_scripts.dependencies as dependencies
import time
import platform
import build_scripts.util as util


def display_help():
    print("run with no arguments for prompted input")
    print("commandline arguments:")
    print("    -all <build all>")
    for i in range(0, len(action_strings)):
        print("    -" + action_strings[i] + " - <" + action_descriptions[i] + ">")
    print("    -platform <osx, win32, ios, linux, android>")
    print("    -ide <xcode4, vs2015, v2017, gmake, android-studio>")
    print("    -clean <clean build, bin and temp dirs>")
    print("    -renderer <dx11, opengl>")
    print("    -toolset <gcc, clang, msc>")


def display_prompted_input():
    print("please enter what you want to build")
    print("1. code")
    print("2. shaders")
    print("3. models")
    print("4. textures")
    print("5. audio")
    print("6. fonts")
    print("7. configs")
    print("8. all")
    print("0. show full command line options")
    input_val = int(input())

    if input_val == 0:
        display_help()

    add_all = False

    all_value = 8
    if input_val == all_value:
        add_all = True

    for index in range(0, all_value - 1):
        if input_val - 1 == index or add_all:
            execute_actions.append(action_strings[index])


def parse_args(args):
    global ide
    global renderer
    global platform_name
    global clean_destinations
    global toolset
    for index in range(0, len(sys.argv)):
        is_action = False
        for action in action_strings:
            arg_action = sys.argv[index][1:]
            if arg_action == action:
                execute_actions.append(action)
                is_action = True
        if is_action:
            continue
        if sys.argv[index] == "-help":
            display_help()
        if sys.argv[index] == "-platform":
            platform_name = sys.argv[index+1]
            index += 1
        if sys.argv[index] == "-ide":
            ide = sys.argv[index+1]
            index += 1
        if sys.argv[index] == "-renderer":
            ide = sys.argv[index+1]
            index += 1
        elif sys.argv[index] == "-actions":
            for j in range(index+1, len(sys.argv)):
                if "-" not in sys.argv[j]:
                    execute_actions.append(sys.argv[j])
                else:
                    break
        elif sys.argv[index] == "-all":
            stats_start = time.time()
            for s in action_strings:
                execute_actions.append(s)
        elif sys.argv[index] == "-clean":
            for s in action_strings:
                execute_actions.append(s)
        elif sys.argv[index] == "-toolset":
            toolset = sys.argv[index+1]
            index += 1


def get_platform_info():
    global ide
    global renderer
    global platform_name
    global python_exec
    global shader_options
    global project_options
    global data_dir
    global toolset
    global extra_build_steps
    global tools_dir
    global extra_target_info

    # platform / renderer auto setup
    if ide == "android-studio":
        renderer = "opengl"
        platform_name = "android"
    elif os.name == "posix":
        if renderer == "":
            renderer = "opengl"
        if platform.system() == "Linux":
            platform_name = "linux"
            ide = "gmake"
        else:
            if ide == "":
                ide = "xcode4"
            if platform_name == "":
                platform_name = "osx"

        python_exec = "python3"
    else:
        if ide == "":
            ide = "vs2017"
        if renderer == "":
            renderer = "dx11"
        if platform_name == "":
            platform_name = "win32"

    extra_target_info = "--platform_dir=" + platform_name
    extra_target_info += " --pmtech_dir=" + build_config["pmtech_dir"] + "/"

    if toolset != "":
        extra_target_info += " --toolset=" + toolset

    if platform_name == "ios":
        extra_target_info = "--xcode_target=ios"

    project_options = ide + " --renderer=" + renderer + " " + extra_target_info

    if renderer == "dx11":
        shader_options = "-shader_platform hlsl"
    elif renderer == "opengl":
        shader_options = "-shader_platform glsl -platform osx"

    if platform_name == "ios":
        shader_options = "-shader_platform glsl -platform ios"

    data_dir = os.path.join("bin", platform_name, "data")


def copy_dir_and_generate_dependencies(dependency_info, dest_sub_dir, src_dir, files):
    platform_bin = os.path.join("bin", platform_name, "")
    excluded_files = [".DS_Store"]
    for file in files:
        skip = False
        for ef in excluded_files:
            if file.endswith(ef):
                skip = True
        if skip:
            continue
        dest_file = os.path.join(dest_sub_dir, file)
        src_file = os.path.join(src_dir, file)
        input_files = [os.path.join(os.getcwd(), src_file)]
        output_files = [dest_file.replace(platform_bin, "")]
        file_info = dependencies.create_dependency_info(input_files, output_files)
        dependency_info[dest_sub_dir]["files"].append(file_info)
        print("copying: " + file + " to " + dest_sub_dir)
        shutil.copy(src_file, dest_file)


def configure_vc_vars_all(build_config):
    if "vcvarsall_dir" in build_config.keys():
        if os.path.exists(build_config["vcvarsall_dir"]):
            return

    while True:
        print("Cannot find 'vcvarsall.exe'")
        print("Please enter the full path to the vc2017 installation directory containing vcvarsall.exe")
        input_dir = str(input())
        input_dir = input_dir.strip("\"")
        input_dir = os.path.normpath(input_dir)
        if os.path.exists(input_dir):
            build_config["vcvarsall_dir"] = input_dir
            bj = open("build_config.json", "w+")
            bj.write(json.dumps(build_config, indent=4))
            bj.close()
            return
        else:
            time.sleep(1)


def build_thirdparty_libs():
    shell_build = ["linux", "osx", "ios"]
    third_party_folder = os.path.join(build_config["pmtech_dir"], "third_party")
    third_party_build = ""
    if util.get_platform_name() in shell_build:
        third_party_build = "cd " + third_party_folder + "; ./build_libs.sh " + util.get_platform_name()
    else:
        configure_vc_vars_all(build_config)
        third_party_build += "cd " + third_party_folder + "&& build_libs.bat \"" + build_config["vcvarsall_dir"] + "\""
    return third_party_build


if __name__ == "__main__":
    print("--------------------------------------------------------------------------------")
    print("pmtech build -------------------------------------------------------------------")
    print("--------------------------------------------------------------------------------")

    stats_start = time.time()

    if len(sys.argv) > 1:
        if "-root_dir" in sys.argv[1]:
            os.chdir(sys.argv[2])

    if os.path.exists("build_config.json"):
        config = open("build_config.json")
        build_config = json.loads(config.read())
    else:
        build_config = dict()

    tools_dir = os.path.join(util.correct_path(build_config["pmtech_dir"]), "tools")

    assets_dir = "assets"

    action_strings = ["code", "shaders", "models", "textures", "audio", "fonts", "configs", "scenes"]
    action_descriptions = ["generate projects and workspaces",
                           "generate shaders and compile binaries",
                           "make binary mesh and animation files",
                           "compress textures and generate mips",
                           "compress and convert audio to platorm format",
                           "copy fonts to data directory",
                           "copy json configs to data directory",
                           "copy scene files to data directory"]
    execute_actions = []
    extra_build_steps = []
    build_steps = []

    python_exec = ""
    shader_options = ""
    project_options = ""

    platform_name = ""
    ide = ""
    renderer = ""
    data_dir = ""
    toolset = ""

    premake_exec = os.path.join(tools_dir, "premake", "premake5")
    if platform.system() == "Linux":
        premake_exec = os.path.join(tools_dir, "premake", "premake5_linux")

    # add premake modules
    android_studio = os.path.join(tools_dir, "premake", "premake-android-studio")
    premake_exec += " --scripts="
    premake_exec += android_studio

    shader_script = os.path.join(tools_dir, "build_scripts", "build_shaders.py")
    textures_script = os.path.join(tools_dir, "build_scripts", "build_textures.py")
    audio_script = os.path.join(tools_dir, "build_scripts", "build_audio.py")
    models_script = os.path.join(tools_dir, "build_scripts", "build_models.py")

    clean_destinations = False

    if len(sys.argv) <= 1:
        display_prompted_input()
    else:
        parse_args(sys.argv)

    get_platform_info()
    copy_steps = []

    common_args = " -platform " + platform_name

    for action in execute_actions:
        if action == "clean":
            built_dirs = [os.path.join("..", "pen", "build", platform_name),
                          os.path.join("..", "put", "build", platform_name),
                          os.path.join("build", platform_name),
                          "bin",
                          "temp"]
            for bd in built_dirs:
                if os.path.exists(bd):
                    shutil.rmtree(bd)
        elif action == "code":
            build_steps.append(build_thirdparty_libs())
            build_steps.append(premake_exec + " " + project_options)
        elif action == "shaders":
            build_steps.append(python_exec + " " + shader_script + " " + shader_options)
        elif action == "models":
            build_steps.append(python_exec + " " + models_script + common_args)
        elif action == "textures":
            build_steps.append(python_exec + " " + textures_script + common_args)
        elif action == "audio":
            build_steps.append(python_exec + " " + audio_script + common_args)
        elif action == "fonts" or action == "configs" or action == "scene":
            copy_steps.append(action)

    for step in build_steps:
        subprocess.check_call(step, shell=True)

    if "code" in execute_actions:
        for step in extra_build_steps:
            subprocess.check_call(step, shell=True)

    if len(copy_steps) > 0:
        print("--------------------------------------------------------------------------------")
        print("Copy data ----------------------------------------------------------------------")
        print("--------------------------------------------------------------------------------")

    for step in copy_steps:
        source_dirs = [assets_dir, os.path.join(build_config["pmtech_dir"], "assets")]
        dependency_info = dict()

        # clear dependencies and create directories
        for source in source_dirs:
            src_dir = os.path.join(source, step)
            dest_dir = os.path.join(data_dir, step)
            if not os.path.exists(src_dir):
                continue
            base_root = ""
            for root, dirs, files in os.walk(src_dir):
                if base_root == "":
                    base_root = root
                dest_sub = dest_dir + root.replace(base_root, "")
                dependency_info[dest_sub] = dict()
                dependency_info[dest_sub]["dir"] = dest_sub
                dependency_info[dest_sub]["files"] = []
                if not os.path.exists(dest_sub):
                    os.makedirs(dest_sub)

        # iterate over directories and generate dependencies
        for source in source_dirs:
            src_dir = os.path.join(source, step)
            dest_dir = os.path.join(data_dir, step)
            if not os.path.exists(src_dir):
                continue
            base_root = ""
            for root, dirs, files in os.walk(src_dir):
                if base_root == "":
                    base_root = root
                dest_sub = dest_dir + root.replace(base_root, "")
                copy_dir_and_generate_dependencies(dependency_info, dest_sub, root, files)

        # write out dependencies
        for dest_depends in dependency_info:
            dir = dependency_info[dest_depends]["dir"]
            directory_dependencies = os.path.join(dir, "dependencies.json")
            output_d = open(directory_dependencies, 'wb+')
            output_d.write(bytes(json.dumps(dependency_info[dir], indent=4), 'UTF-8'))
            output_d.close()

    print("--------------------------------------------------------------------------------")
    stats_end = time.time()
    millis = int((stats_end - stats_start) * 1000)
    print("All Jobs Done (" + str(millis) + "ms)")








