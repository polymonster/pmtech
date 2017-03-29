import os
import os.path
import shutil

platform_name = "win32"
if os.name == "posix":
    platform_name = "osx"

audio_dir = os.path.join(os.getcwd(), "assets", "audio")
build_dir = os.path.join(os.getcwd(), "bin", platform_name, "data", "audio")
bin_dir = os.path.join(os.getcwd(), "bin", platform_name)

# make directories
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

# copy fmod dll
if platform_name == "win32":
    print("copying dll to binary dir")
    src_file = os.path.join(os.getcwd(), "..", "pen", "third_party", "fmod", "lib", "win32", "fmod.dll")
    shutil.copy(src_file, bin_dir)

# copy audio files
print("copying audio to data dir")
for f in os.listdir(audio_dir):
    src_file = os.path.join(audio_dir,f)
    shutil.copy(src_file, build_dir)