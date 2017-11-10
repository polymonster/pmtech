import os
import subprocess
import os.path

#ios todo
#python ../tools/project_ios/copy_files.py
#../tools/premake/premake5 xcode4 --renderer=opengl --xcode_target=ios
#python ../tools/project_ios/set_xcode_target.py

tools_dir = os.path.join("..", "tools")

print("please enter what you want to build")
print("1. code projects")
print("2. shaders")
print("3. models")
print("4. textures")
print("5. audio")
print("6. all")

all_value = 6
input_val = int(input())

action_strings = ["code", "shaders", "models", "textures", "audio"]
execute_actions = []

add_all = False

if input_val == all_value:
    add_all = True

for index in range(0,all_value-1):
    if input_val-1 == index or add_all:
        execute_actions.append(action_strings[index])

#default win32
premake_exec = os.path.join(tools_dir, "premake","premake5")
python_exec = os.path.join(tools_dir, "bin", "python", "win32", "python3")
project_options = "vs2015 --renderer=dx11"
shader_options = "hlsl win32"

if os.name == "posix":
    python_exec = os.path.join(tools_dir, "bin", "python", "osx", "python3")
    project_options = "xcode4 --renderer=opengl"
    shader_options = "glsl osx"

shader_script = os.path.join(tools_dir, "build_shaders.py")
textures_script = os.path.join(tools_dir, "build_textures.py")
audio_script =  os.path.join(tools_dir, "build_audio.py")
models_script = os.path.join(tools_dir, "build_models.py")

build_steps = []

print(execute_actions)

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

for step in build_steps:
    subprocess.check_call(step, shell=True)






