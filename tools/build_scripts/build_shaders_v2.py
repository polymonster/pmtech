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


# info and contents of a .pmfx file
class pmfx_info:
    includes = ""
    json = ""
    source = ""
    constant_buffers = ""
    texture_samplers = ""


# info of pmfx technique permutation
class technique_permutation_info:
    technique = ""
    permutation = ""


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


# recursively merge members of 2 json objects
def member_wise_merge(j1, j2):
    for key in j2.keys():
        if key not in j1.keys():
            j1[key] = j2[key]
        elif type(j1[key]) is dict:
            j1[key] = member_wise_merge(j1[key], j2[key])
    return j1


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


# find the end of a body text enclosed in brackets
def enclose_brackets(text):
    body_pos = text.find("{")
    bracket_stack = ["{"]
    text_len = len(text)
    while len(bracket_stack) > 0 and body_pos < text_len:
        body_pos += 1
        character = text[body_pos:body_pos+1]
        if character == "{":
            bracket_stack.insert(0, "{")
        if character == "}" and bracket_stack[0] == "{":
            bracket_stack.pop(0)
            body_pos += 1
    return body_pos


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


# recursively generate all possible permutations from inputs
def permute(define_list, permute_list, output_permutations):
    if len(define_list) == 0:
        output_permutations.append(list(permute_list))
    else:
        d = define_list.pop()
        for s in d[1]:
            ds = (d[0], s)
            permute_list.append(ds)
            output_permutations = permute(define_list, permute_list, output_permutations)
            if len(permute_list) > 0:
                permute_list.pop()
        define_list.append(d)
    return output_permutations


# generate numerical id for permutation
def generate_permutation_id(define_list, permutation):
    pid = 0
    for p in permutation:
        for d in define_list:
            if p[0] == d[0]:
                if p[1] > 0:
                    exponent = d[2]
                    if exponent < 0:
                        continue
                    if p[1] > 1:
                        exponent = p[1]+exponent-1
                    pid += pow(2, exponent)
    return pid


# generate permutation list from technique json
def generate_permutation_list(technique, technique_json):
    output_permutations = []
    define_list = []
    permutation_options = dict()
    permutation_option_mask = 0
    define_string = ""
    if "permutations" in technique_json:
        for p in technique_json["permutations"].keys():
            pp = technique_json["permutations"][p]
            define_list.append((p, pp[1], pp[0]))
        if "defines" in technique_json.keys():
            for d in technique_json["defines"]:
                define_list.append((d, [1], -1))
        output_permutations = permute(define_list, [], [])
        for key in technique_json["permutations"]:
            tp = technique_json["permutations"][key]
            ptype = "checkbox"
            if len(tp[1]) > 2:
                ptype = "input_int"
            permutation_options[key] = {"val": pow(2, tp[0]), "type": ptype}
            mask = pow(2, tp[0])
            permutation_option_mask += mask
            define_string += "#define " + technique.upper() + "_" + key + " " + str(mask) + "\n"
        define_string += "\n"

    # generate default permutation, inherit / get permutation constants
    tp = list(output_permutations)
    if len(tp) == 0:
        default_permute = []
        if "defines" in technique_json.keys():
            for d in technique_json["defines"]:
                default_permute.append((d, 1))
        else:
            default_permute = [("SINGLE_PERMUTATION", 1)]
        tp.append(default_permute)
    return tp, define_string


# look for inherit member and inherit another pmfx technique
def inherit_technique(technique, pmfx_json):
    if "inherit" in technique.keys():
        inherit = technique["inherit"]
        if inherit in pmfx_json.keys():
            technique = member_wise_merge(technique, pmfx_json[inherit])
    return technique


# parse pmfx file to find the json block pmfx: { }
def find_pmfx_json(shader_file_text):
    pmfx_loc = shader_file_text.find("pmfx:")
    if pmfx_loc != -1:
        json_loc = shader_file_text.find("{", pmfx_loc)
        # find pmfx json
        pmfx_end = enclose_brackets(shader_file_text[pmfx_loc:])
        pmfx_json = json.loads(shader_file_text[json_loc:pmfx_end + json_loc])
        return pmfx_json
    return ""


# parse a pmfx file which is a collection of techniques and permutations, made up of vs, ps, cs combinations
def parse_pmfx(file, root):
    global _info
    global _pmfx
    global _tech

    # new pmfx info
    _pmfx = pmfx_info()

    file_and_path = os.path.join(root, file)
    needs_building, shader_file_text, included_files = create_shader_set(file_and_path, root)

    if not needs_building:
        return

    _pmfx.json = find_pmfx_json(shader_file_text)
    _pmfx.source = shader_file_text

    # for techniques in pmfx
    for technique in _pmfx.json:
        technique_json = inherit_technique(_pmfx.json[technique], _pmfx.json)
        technique_permutations, defines = generate_permutation_list(technique, technique_json)
        # for permutatuons in technique
        print(technique_permutations)
        print(defines)


# entry
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
