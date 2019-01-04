import os
import sys
import json
import re
import dependencies
import util


# paths and info for current build environment
class build_info:
    shader_platform = ""
    os_platform = ""
    root_dir = ""
    build_config = ""
    pmtech_dir = ""
    tools_dir = ""
    output_dir = ""
    this_file = ""
    macros_file = ""


class shader_info:
    included_files = ""


# parse command line args passed in
def parse_args():
    global _info
    for i in range(1, len(sys.argv)):
        if "-root_dir" in sys.argv[i]:
            os.chdir(sys.argv[i + 1])
        if "-shader_platform" in sys.argv[i]:
            _info.shader_platform = sys.argv[i + 1]
        if "-platform" in sys.argv[i]:
            _info.os_platform = sys.argv[i + 1]


# remove comments, taken from stub_format.py ()
def remove_comments(file_data):
    lines = file_data.split("\n")
    inside_block = False
    conditioned = ""
    for line in lines:
        if inside_block:
            ecpos = line.find("*/")
            if ecpos != -1:
                inside_block = False
                line = line[ecpos+2:]
            else:
                continue
        cpos = line.find("//")
        mcpos = line.find("/*")
        if cpos != -1:
            conditioned += line[:cpos] + "\n"
        elif mcpos != -1:
            conditioned += line[:mcpos] + "\n"
            inside_block = True
        else:
            conditioned += line + "\n"
    return conditioned


# tidy shader source with consistent spaces, remove tabs and comments to make subsequent operations easier
def sanitize_shader_source(shader_source):
    # replace tabs with spaces
    shader_source = shader_source.replace("\t", " ")
    # replace all spaces with single space
    shader_source = re.sub(' +', ' ', shader_source)
    # remove comments
    shader_source = remove_comments(shader_source)
    return shader_source


# get info filename for dependency checking
def get_resource_info_filename(filename, build_dir):
    global _info
    base_filename = os.path.basename(filename)
    dir_path = os.path.dirname(filename)
    info_filename = os.path.join(_info.output_dir, os.path.splitext(base_filename)[0], "info.json")
    return info_filename, base_filename, dir_path


# check file time stamps and build times to determine if rebuild needs to happen
def check_dependencies(filename, included_files):
    global _info
    # look for .json file
    file_list = list()
    file_list.append(dependencies.sanitize_filename(os.path.join(_info.root_dir, filename)))
    file_list.append(dependencies.sanitize_filename(_info.this_file))
    file_list.append(dependencies.sanitize_filename(_info.macros_file))
    info_filename, base_filename, dir_path = get_resource_info_filename(filename, _info.output_dir)
    for f in included_files:
        file_list.append(dependencies.sanitize_filename(os.path.join(_info.root_dir, f)))
    if os.path.exists(info_filename):
        info_file = open(info_filename, "r")
        info = json.loads(info_file.read())
        for prev_built_with_file in info["files"]:
            sanitized_name = dependencies.sanitize_filename(prev_built_with_file["name"])
            if sanitized_name in file_list:
                if not os.path.exists(sanitized_name):
                    return False
                if prev_built_with_file["timestamp"] < os.path.getmtime(sanitized_name):
                    info_file.close()
                    print(os.path.basename(sanitized_name) + " is out of date")
                    return False
            else:
                print(file_list)
                print(sanitized_name + " is not in list")
                return False
        info_file.close()
    else:
        return False
    return True


# find #include statements
def find_includes(file_text, root):
    global added_includes
    include_list = []
    start = 0
    while 1:
        start = file_text.find("#include", start)
        if start == -1:
            break
        start = file_text.find("\"", start) + 1
        end = file_text.find("\"", start)
        if start == -1 or end == -1:
            break
        include_name = file_text[start:end]
        include_path = os.path.join(root, include_name)
        include_path = util.sanitize_file_path(include_path)
        if include_path not in added_includes:
            include_list.append(include_path)
            added_includes.append(include_path)
    return include_list


# recursively search for #includes
def add_files_recursive(filename, root):
    file_path = filename
    if not os.path.exists(filename):
        file_path = os.path.join(root, filename)
    included_file = open(file_path, "r")
    shader_source = included_file.read()
    included_file.close()
    shader_source = sanitize_shader_source(shader_source)
    sub_root = os.path.dirname(file_path)
    include_list = find_includes(shader_source, sub_root)
    for include_file in reversed(include_list):
        included_source, sub_includes = add_files_recursive(include_file, sub_root)
        shader_source = included_source + "\n" + shader_source
        include_list = include_list + sub_includes
    return shader_source, include_list


# gather include files and
def create_shader_set(filename, root):
    global _info
    global added_includes
    added_includes = []
    shader_file_text, included_files = add_files_recursive(filename, root)
    up_to_date = check_dependencies(filename, included_files)

    shader_base_name = os.path.basename(filename)
    shader_set_dir = os.path.splitext(shader_base_name)[0]
    shader_set_build_dir = os.path.join(_info.output_dir, shader_set_dir)

    if not os.path.exists(shader_set_build_dir):
        os.makedirs(shader_set_build_dir)

    if up_to_date:
        print(filename + " file up to date")
        return False, None, None

    return True, shader_file_text, included_files


# parse a pmfx file which is a collection of techniques and permutations, made up of vs, ps, cs combinations
def parse_pmfx(file, root):
    global _info
    file_and_path = os.path.join(root, file)
    needs_building, shader_file_text, included_files = create_shader_set(file_and_path, root)

    if not needs_building:
        return

    print(file_and_path)


# main func
if __name__ == "__main__":
    print("--------------------------------------------------------------------------------")
    print("pmfx shader compilation (v2)----------------------------------------------------")
    print("--------------------------------------------------------------------------------")

    global _info
    _info = build_info()

    parse_args()

    # pm build config
    config = open("build_config.json")

    # get dirs for build output
    _info.root_dir = os.getcwd()
    _info.build_config = json.loads(config.read())
    _info.pmtech_dir = util.correct_path(_info.build_config["pmtech_dir"])
    _info.tools_dir = os.path.join(_info.pmtech_dir, "tools")
    _info.output_dir = os.path.join(_info.root_dir, "bin", _info.os_platform, "data", "pmfx", _info.shader_platform)
    _info.this_file = os.path.join(_info.root_dir, _info.tools_dir, "build_scripts", "build_shaders.py")
    _info.macros_file = os.path.join(_info.root_dir, _info.tools_dir, "_shader_macros.h")

    # dirs for build input
    shader_source_dir = os.path.join(_info.root_dir, "assets", "shaders")
    pmtech_shaders = os.path.join(_info.pmtech_dir, "assets", "shaders")

    source_list = [pmtech_shaders, shader_source_dir]
    for source_dir in source_list:
        for root, dirs, files in os.walk(source_dir):
            for file in files:
                if file.endswith(".pmfx"):
                    parse_pmfx(file, root)
