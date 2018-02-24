import os
import subprocess
import shutil
import json
import helpers
import dependencies
import time
import json
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


def process_single_file(source, f):
    src_file = os.path.join(root, f)
    supported, dest_file = get_output_name(source, src_file)
    if not supported:
        return

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

    if f.endswith(".dds"):
        print("copying " + f)
        shutil.copy(src_file, dest_file)
    else:
        export_options_string = options_from_export(export_info, src_file)
        print("compress and generate mips " + src_file)
        cmdline = nvcompress + " " + export_options_string + " -silent " + src_file + " " + dest_file
        subprocess.call(cmdline, shell=True)


def process_collection(source, container):
    supported, cubemap_file = get_output_name(source, container)
    if not supported:
        return
    cubemap_faces = []
    for root, dirs, files in os.walk(container):
        for file in files:
            [fnoext, fext] = os.path.splitext(file)
            if fext not in supported_formats:
                continue
            cubemap_faces.append(os.path.join(container, file))

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

platform_name = "win32"
if os.name == "posix":
    platform_name = "osx"

config = open("build_config.json")
build_config = json.loads(config.read())
pmtech_dir = helpers.correct_path(build_config["pmtech_dir"])

nvcompress = os.path.join(pmtech_dir, "tools", "bin", "nvtt", platform_name, "nvcompress")
nvassemble = os.path.join(pmtech_dir, "tools", "bin", "nvtt", platform_name, "nvassemble")
texture_dir = helpers.correct_path(build_config["textures_dir"])
build_dir = os.path.join(os.getcwd(), "bin", platform_name, "data", "textures")
current_directory = os.path.join(os.getcwd(), "")
platform_data_dir = os.path.join("bin", platform_name, "")

supported_formats = [".png", ".jpg", ".tif", ".bmp", ".tga", ".cube"]
container_formats = [".cube", ".volume", ".array"]
built_in_texture_dir = os.path.join(os.getcwd(), pmtech_dir, "assets", "textures")

# create textures dir
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

print("processing directory: " + texture_dir)

source_dirs = [texture_dir, built_in_texture_dir]

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
        export_info = dict()
        dir_export_file = os.path.join(root, "export.json")
        if os.path.exists(dir_export_file):
            file = open(dir_export_file, "r")
            file_json = file.read()
            export_info = json.loads(file_json)
        if root.endswith(".cube"):
            process_collection(source, root)
            continue
        for f in files:
            process_single_file(source, f)


for dest_depends in dependency_info:
    dependencies.write_to_file(dependency_info[dest_depends])

stats_end = time.time()
millis = int((stats_end - stats_start) * 1000)
print("Done (" + str(millis) + "ms)")
