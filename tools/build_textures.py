import os
import subprocess
import shutil
import json
import helpers
import dependencies

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
texture_dir = helpers.correct_path(build_config["textures_dir"])
build_dir = os.path.join(os.getcwd(), "bin", platform_name, "data", "textures")

built_in_texture_dir = os.path.join(os.getcwd(), pmtech_dir, "assets", "textures")

# create textures dir
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

print("processing directory: " + texture_dir)
print("\n")

supported_formats = [".png", ".jpg", ".tif", ".bmp", ".tga"]
source_dirs = [texture_dir, built_in_texture_dir]

dependency_info = dict()

for source in source_dirs:
    for root, dirs, files in os.walk(source):
        dest_dir = root.replace(source, build_dir)
        dependency_info[dest_dir] = dict()
        dependency_info[dest_dir]["files"] = []
        dependency_info[dest_dir]["dir"] = dest_dir

for source in source_dirs:
    for root, dirs, files in os.walk(source):
        dest_dir = root.replace(source, build_dir)
        for f in files:
            src_file = os.path.join(root, f)
            [fnoext, fext] = os.path.splitext(f)
            if fext not in supported_formats:
                continue
            fnoext = fnoext.replace(source, build_dir)
            dds_filename = fnoext + ".dds"
            dest_file = os.path.join(dest_dir, dds_filename)

            cur = os.path.join(os.getcwd(), "")
            relative_data_filename = dest_file.replace(cur, "")
            data_dir = os.path.join("bin", platform_name, "")
            relative_data_filename = relative_data_filename.replace(data_dir, "")

            dependency_inputs = [src_file]
            dependency_outputs = [relative_data_filename]
            file_info = dependencies.create_dependency_info(dependency_inputs, dependency_outputs)
            dependency_info[dest_dir]["files"].append(file_info)

            rd = relative_data_filename
            if dependencies.check_up_to_date(dependency_info[dest_dir], rd):
                print(dest_file + " already up to date")
                continue

            if not os.path.exists(dest_dir):
                os.makedirs(dest_dir)
            if f.find(".dds") != -1:
                print("copying " + f)
                shutil.copy(src_file, dest_file)
            else:
                for fmt in supported_formats:
                    if fmt in f:
                        print("converting " + src_file)
                        cmdline = nvtt_dir + " -rgb -silent " + src_file + " " + dest_file
                        subprocess.check_call(cmdline, shell=True)

for dest_depends in dependency_info:
    dir = dependency_info[dest_depends]["dir"]
    directory_dependencies = os.path.join(dir, "dependencies.json")
    output_d = open(directory_dependencies, 'wb+')
    output_d.write(bytes(json.dumps(dependency_info[dir], indent=4), 'UTF-8'))
    output_d.close()

