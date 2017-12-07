import os
import subprocess
import shutil

# win32 / dds / block compression / mips / cubemaps
print("\n")
print("--------------------------------------------------------------------------------------------------------------")
print("pmtech texture compression and mip map generation ------------------------------------------------------------")
print("--------------------------------------------------------------------------------------------------------------")

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
print("\n")

supported_formats = [".png", ".jpg", ".tif", ".bmp", ".tga"]

for root, dirs, files in os.walk(texture_dir):
    for f in files:
        f = os.path.join(root, f)
        [fnoext, fext] = os.path.splitext(f)
        fnoext = fnoext.replace(texture_dir, build_dir)

        dest_dir = root.replace(texture_dir, build_dir)
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
                    src_path = os.path.join(texture_dir, f)
                    print("converting " + f)
                    cmdline = nvtt_dir + " -rgb -silent " + src_path + " " + dest_path
                    subprocess.check_call(cmdline, shell=True)



