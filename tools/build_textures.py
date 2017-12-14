import os
import subprocess
import shutil
import json
import helpers

# win32 / dds / block compression / mips / cubemaps
print("\n")
print("--------------------------------------------------------------------------------------------------------------")
print("pmtech texture compression and mip map generation ------------------------------------------------------------")
print("--------------------------------------------------------------------------------------------------------------")

platform_name = "win32"
if os.name == "posix":
    platform_name = "osx"

config = open("build_config.json")
build_config = json.loads(config.read())
pmtech_dir = helpers.correct_path(build_config["pmtech_dir"])

nvtt_dir = os.path.join(pmtech_dir, "tools", "bin", "nvtt", platform_name, "nvcompress")
texture_dir = os.path.join(os.getcwd(), "assets", "textures")
build_dir = os.path.join(os.getcwd(), "bin", platform_name, "data", "textures")

built_in_texture_dir = os.path.join(pmtech_dir, "assets", "textures")

# create textures dir
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

print("processing directory: " + texture_dir)
print("\n")

supported_formats = [".png", ".jpg", ".tif", ".bmp", ".tga"]
source_dirs = [texture_dir, built_in_texture_dir]

for source in source_dirs:
    for root, dirs, files in os.walk(source):
        for f in files:
            print(f)
            f = os.path.join(root, f)
            print(f)
            [fnoext, fext] = os.path.splitext(f)
            fnoext = fnoext.replace(source, build_dir)
            dest_dir = root.replace(source, build_dir)
            if not os.path.exists(dest_dir):
                os.makedirs(dest_dir)
            if f.find(".dds") != -1:
                print("copying " + f)
                dest_file = os.path.join(build_dir, f)
                shutil.copy(f, dest_file)
            else:
                for fmt in supported_formats:
                    if fmt in f:
                        dds_filename = fnoext + ".dds"
                        dest_path = dds_filename
                        src_path = f
                        print("converting " + f)
                        cmdline = nvtt_dir + " -rgb -silent " + src_path + " " + dest_path
                        subprocess.check_call(cmdline, shell=True)



