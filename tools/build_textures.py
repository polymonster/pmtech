import os
import subprocess
import shutil
import dependencies
import time
import json
import build
import sys

stats_start = time.time()


def options_from_export(info, filename):
    base_name = os.path.basename(filename)
    if "files" in info.keys():
        if base_name in info["files"]:
            return info["files"][base_name]
    return "-rgb"


def get_output_name(source_dir, file):
    supported = True
    [fnoext, fext] = os.path.splitext(file)
    if fext not in supported_formats:
        supported = False
    fnoext = fnoext.replace(source_dir, build_dir)
    dds_filename = fnoext + ".dds"
    dest_file = os.path.join(dest_dir, dds_filename)
    return supported, dest_file


def process_single_file(f):
    src_file = os.path.join(root, f)
    supported, dest_file = get_output_name(source, src_file)
    if not supported:
        return

    export_info = dependencies.get_export_config(os.path.join(root, f))

    relative_data_filename = dest_file.replace(current_directory, "")
    relative_data_filename = relative_data_filename.replace(platform_data_dir, "")

    dependency_inputs = [os.path.join(os.getcwd(), src_file)]
    dependency_outputs = [relative_data_filename]

    file_info = dependencies.create_dependency_info(dependency_inputs, dependency_outputs)
    dependency_info[dest_dir]["files"].append(file_info)

    if dependencies.check_up_to_date(dependency_info[dest_dir], relative_data_filename):
        print(relative_data_filename + " already up to date")
        return

    if not os.path.exists(dest_dir):
        os.makedirs(dest_dir)

    if f.endswith(".dds") or f.endswith(".pmv"):
        print("copying " + f)
        shutil.copy(src_file, dest_file)
    else:
        export_options_string = options_from_export(export_info, src_file)
        print("compress and generate mips " + src_file)
        cmdline = nvcompress + " " + export_options_string + " -silent " + src_file + " " + dest_file
        print(cmdline)
        subprocess.call(cmdline, shell=True)


def process_collection(container):
    supported, cubemap_file = get_output_name(source, container)
    if not supported:
        return

    export_info = dependencies.get_export_config(container)

    if "cubemap_faces" not in export_info.keys():
        print("missing cubemap_faces array in export")
        return

    cubemap_faces = []
    for f in export_info["cubemap_faces"]:
        cubemap_faces.append(os.path.join(container, f))

    relative_data_filename = cubemap_file.replace(current_directory, "")
    relative_data_filename = relative_data_filename.replace(platform_data_dir, "")
    dependency_outputs = [relative_data_filename]

    dest_container_dir = os.path.dirname(cubemap_file)
    file_info = dependencies.create_dependency_info(cubemap_faces, dependency_outputs)
    dependency_info[dest_container_dir]["files"].append(file_info)

    if dependencies.check_up_to_date(dependency_info[dest_container_dir], relative_data_filename):
        print(relative_data_filename + " already up to date")
        return

    print("assembling " + cubemap_file)
    cmdline = nvassemble
    for face in cubemap_faces:
        cmdline += " " + face
    cmdline += " -o " + cubemap_file
    subprocess.call(cmdline, shell=True)


# win32 / dds / block compression / mips / cubemaps
print("--------------------------------------------------------------------------------")
print("pmtech texture compression and mip map generation ------------------------------")
print("--------------------------------------------------------------------------------")

platform_name = build.get_platform_name_args(sys.argv)

config = open("build_config.json")
build_config = json.loads(config.read())
pmtech_dir = build.correct_path(build_config["pmtech_dir"])

nvcompress = os.path.join(pmtech_dir, "tools", "bin", "nvtt", platform_name, "nvcompress")
nvassemble = os.path.join(pmtech_dir, "tools", "bin", "nvtt", platform_name, "nvassemble")
texture_dir = build.correct_path(build_config["textures_dir"])
build_dir = os.path.join(os.getcwd(), "bin", platform_name, "data", "textures")
current_directory = os.path.join(os.getcwd(), "")
platform_data_dir = os.path.join("bin", platform_name, "")

supported_formats = [".png", ".jpg", ".tif", ".bmp", ".tga", ".cube", ".dds", ".pmv"]
container_formats = [".cube", ".volume", ".array"]
built_in_texture_dir = os.path.join(os.getcwd(), pmtech_dir, "assets", "textures")

# create textures dir
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

print("processing directory: " + texture_dir)

source_dirs = [texture_dir, built_in_texture_dir]

dependencies.delete_orphaned_files(build_dir, platform_data_dir)

dependency_info = dict()
for source in source_dirs:
    for root, dirs, files in os.walk(source):
        dest_dir = root.replace(source, build_dir)
        skip = False
        for c in container_formats:
            if dest_dir.endswith(c):
                skip = True
                break
        if not skip:
            dependency_info[dest_dir] = dict()
            dependency_info[dest_dir]["files"] = []
            dependency_info[dest_dir]["dir"] = dest_dir

for source in source_dirs:
    for root, dirs, files in os.walk(source):
        dest_dir = root.replace(source, build_dir)
        if root.endswith(".cube"):
            process_collection(root)
            continue
        for f in files:
            process_single_file(f)


for dest_depends in dependency_info:
    dependencies.write_to_file(dependency_info[dest_depends])

stats_end = time.time()
millis = int((stats_end - stats_start) * 1000)
print("Done (" + str(millis) + "ms)")
