import os
import subprocess
import shutil

# win32 / dds / block compression / mips / cubemaps
print("\nbuild_textures")
print("texture conversion, mip generation and compression")

platform_name = "win32"
if os.name == "posix":
    platform_name = "osx"

nvtt_dir = os.path.join(os.getcwd(), "..", "tools", "bin", "nvtt", platform_name, "nvcompress")
texture_dir = os.path.join(os.getcwd(), "assets", "textures")
build_dir = os.path.join(os.getcwd(), "bin", platform_name, "data", "textures")

# create textures dir
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

print("processing directory: " + texture_dir)

for f in os.listdir(texture_dir):
    [fnoext, fext] = os.path.splitext(f)
    print(f)
    dds_filename = fnoext + ".dds"
    dest_path = os.path.join(build_dir, dds_filename)
    src_path = os.path.join(texture_dir, f)
    if f.find(".png") != -1 or f.find(".jpg") or f.find(".tif") != -1:
        print("converting texture to data dir")
        cmdline = nvtt_dir + " -rgb " + src_path + " " + dest_path
        subprocess.check_call(cmdline, shell=True)
    if f.find(".dds") != -1:
        print("copying texture to data dir")
        # straight copy
        dest_file = os.path.join(build_dir, f)
        shutil.copy(f, dest_file)
    print("\n")
