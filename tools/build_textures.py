
import os
import subprocess

#win32 / dds / block compression / mips / cubemaps
print("texture conversion, mip generation and compression" + "\n")

nvtt_dir = os.getcwd() + "\\..\\tools\\bin\\nvtt\\win32\\"
texture_dir = os.getcwd() + "\\assets\\textures\\"
build_dir = os.getcwd() + "\\bin\\win32\\data\\textures\\"

#create textures dir
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

print("processing directory: " + texture_dir + "\n")

for f in os.listdir(texture_dir):
    [fnoext, fext] = os.path.splitext(f)
    print(f)
    print(fnoext)
    print(nvtt_dir)
    if f.find(".png") != -1 or f.find(".jpg") or f.find(".tif") != -1:
        print("converting texture")
        cmdline = nvtt_dir + "nvcompress -rgb " + texture_dir + f + " " + build_dir + fnoext + ".dds"
    if f.find(".dds") != -1:
        print("copying texture")
        #straight copy
    subprocess.check_call(cmdline)
    print("\n")
