import os
import subprocess
import os.path
import sys
import shutil

tools_dir = os.path.join("..", "tools")
assets_dir = "assets"

action_strings = ["code", "shaders", "models", "textures", "audio", "fonts"]
action_descriptions = ["generate projects and workspaces",
                       "generate shaders and compile binaries",
                       "make binary mesh and animation files",
                       "compress textures and generate mips",
                       "compress and convert audio to platorm format",
                       "copy fonts to data directory"]
execute_actions = []
extra_build_steps = []
build_steps = []

python_exec = ""
shader_options = ""
project_options = ""

platform = ""
ide = ""
renderer = ""
data_dir = ""

premake_exec = os.path.join(tools_dir, "premake","premake5")
shader_script = os.path.join(tools_dir, "build_shaders.py")
textures_script = os.path.join(tools_dir, "build_textures.py")
audio_script = os.path.join(tools_dir, "build_audio.py")
models_script = os.path.join(tools_dir, "build_models.py")


def display_help():
    print("--------pmtech build--------")
    print("run with no arguments for prompted input")
    print("commandline arguments")
    print("\t-platform <osx, win32, ios>")
    print("\t-ide <xcode4, vs2015, v2017>")
    print("\t-renderer <dx11, opengl>")
    print("\t-actions")
    for i in range(0, len(action_strings)):
        print("\t\t" + action_strings[i] + " - " + action_descriptions[ i ])


def parse_args(args):
    global ide
    global renderer
    global platform
    for index in range(0, len(sys.argv)):
        if sys.argv[index] == "-help":
            display_help()
        if sys.argv[index] == "-platform":
            platform = sys.argv[index+1]
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


def get_platform_info():
    global ide
    global renderer
    global platform
    global python_exec
    global shader_options
    global project_options
    global data_dir

    if os.name == "posix":
        if ide == "":
            ide = "xcode4"
        if renderer == "":
            renderer = "opengl"
        if platform == "":
            platform = "osx"
        python_exec = os.path.join(tools_dir, "bin", "python", "osx", "python3")
    else:
        if ide == "":
            ide = "vs2017"
        if renderer == "":
            renderer = "dx11"
        if platform == "":
            platform = "win32"
        python_exec = os.path.join(tools_dir, "bin", "python", "win32", "python3")

    extra_target_info = ""
    if platform == "ios":
        extra_target_info = "--xcode_target=ios"
        extra_build_steps.append(python_exec + " " + os.path.join(tools_dir, "project_ios", "copy_files.py"))
        extra_build_steps.append(python_exec + " " + os.path.join(tools_dir, "project_ios", "set_xcode_target.py"))

    project_options = ide + " --renderer=" + renderer + " " + extra_target_info

    if renderer == "dx11":
        shader_options = "hlsl win32"
    elif renderer == "opengl":
        shader_options = "glsl osx"

    data_dir = os.path.join("bin", platform, "data")

if len(sys.argv) <= 1:
    print("please enter what you want to build")
    print("1. code projects")
    print("2. shaders")
    print("3. models")
    print("4. textures")
    print("5. audio")
    print("6. all")
    print("0. show full command line options")

    all_value = 6
    input_val = int(input())

    if input_val==0:
        display_help()

    add_all = False

    if input_val == all_value:
        add_all = True

    for index in range(0,all_value-1):
        if input_val-1 == index or add_all:
            execute_actions.append(action_strings[index])
else:
    parse_args(sys.argv)

get_platform_info()

copy_steps = []

for action in execute_actions:
    if action == "code":
        build_steps.append(premake_exec + " " + project_options)
    elif action == "shaders":
        build_steps.append(python_exec + " " + shader_script + " " + shader_options)
    elif action == "models":
        build_steps.append(python_exec + " " + models_script)
    elif action == "textures":
        build_steps.append(python_exec + " " + textures_script)
    elif action == "audio":
        build_steps.append(python_exec + " " + audio_script)
    elif action == "fonts":
        copy_steps.append(action)

for step in build_steps:
    subprocess.check_call(step, shell=True)

for step in extra_build_steps:
    subprocess.check_call(step, shell=True)

for step in copy_steps:
    dest_dir = os.path.join(data_dir, step)
    if os.path.exists(dest_dir):
        shutil.rmtree(dest_dir)
    shutil.copytree(os.path.join(assets_dir, step), dest_dir)








