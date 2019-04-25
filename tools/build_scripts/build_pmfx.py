import os
import sys
import json
import re
import dependencies
import util
import math
import subprocess

# paths and info for current build environment
class build_info:
    shader_platform = ""                                                # hlsl, glsl, metal
    shader_sub_platform = ""                                            # gles
    shader_version = ""                                                 # 4_0 (sm 4.0), 330 (glsl 330), 450 (glsl
    os_platform = ""                                                    # win32, osx, linux, ios, android
    root_dir = ""                                                       # cwd dir to run from
    build_config = ""                                                   # json contents of build_config.json
    pmtech_dir = ""                                                     # location of pmtech root dir
    tools_dir = ""                                                      # location of pmtech/tools
    output_dir = ""                                                     # dir to build shader binaries
    this_file = ""                                                      # the file u are reading
    macros_file = ""                                                    # _shader_macros.h
    macros_source = ""                                                  # source code inside _shader_macros.h
    error_code = 0                                                      # non-zero if any shaders failed to build


# info and contents of a .pmfx file
class pmfx_info:
    includes = ""                                                       # list of included files
    json = ""                                                           # json object containing techniques
    json_text = ""                                                      # json as text to reload mutable dictionary
    source = ""                                                         # source code of the entire shader +includes


# info of pmfx technique permutation which is a combination of vs,ps or cs
class technique_permutation_info:
    technique_name = ""                                                 # name of technique
    technique = ""                                                      # technique / permutation json
    permutation = ""                                                    # permutation options
    source = ""                                                         # conditioned source code for permute
    id = ""                                                             # permutation id
    cbuffers = []                                                       # list of cbuffers source code
    functions = []                                                      # list of functions source code
    textures = []                                                       # technique / permutation textures
    shader = []                                                         # list of shaders, vs, ps or cs


# info about a single vs, ps, or cs
class single_shader_info:
    shader_type = ""                                                    # ie. vs (vertex), ps (pixel), cs (compute)
    main_func_name = ""                                                 # entry point ie. vs_main
    functions_source = ""                                               # source code of all used functions
    main_func_source = ""                                               # source code of main function
    input_struct_name = ""                                              # name of input to shader ie. vs_input
    instance_input_struct_name = ""                                     # name of instance input to vertex shader
    output_struct_name = ""                                             # name of output from shader ie. vs_output
    input_decl = ""                                                     # struct decl of input struct
    instance_input_decl = ""                                            # struct decl of instance input struct
    output_decl = ""                                                    # struct decl of shader output
    struct_decls = ""                                                   # decls of all generic structs
    texture_decl = []                                                   # decl of only used textures by shader
    cbuffers = []                                                       # array of cbuffer decls used by shader


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
        if "-shader_version" in sys.argv[i]:
            _info.shader_version = sys.argv[i + 1]


# convert signed to unsigned integer in a c like manner, to do c like things
def us(v):
    if v == -1:
        return sys.maxsize
    return v


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


# parse and split into an array, from a list of textures or cbuffers etc
def parse_and_split_block(code_block):
    start = code_block.find("{") + 1
    end = code_block.find("};")
    block_conditioned = code_block[start:end].replace(";", "")
    block_conditioned = block_conditioned.replace(":", "")
    block_conditioned = block_conditioned.replace("(", "")
    block_conditioned = block_conditioned.replace(")", "")
    block_conditioned = block_conditioned.replace(",", "")
    block_conditioned = re.sub(' +', ' ', block_conditioned)
    return block_conditioned.split()


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


# replace all "input" and "output" tokens to "_input" and "_ouput" to avoid glsl keywords
def replace_io_tokens(text):
    token_io = ["input", "output"]
    token_io_replace = ["_input", "_output"]
    token_post_delimiters = ['.', ';', ' ', '(', ')', ',', '-', '+', '*', '/']
    token_pre_delimiters = [' ', '\t', '\n', '(', ')', ',', '-', '+', '*', '/']
    split = text.split(' ')
    split_replace = []
    for token in split:
        for i in range(0, len(token_io)):
            if token_io[i] in token:
                last_char = len(token_io[i])
                first_char = token.find(token_io[i])
                t = token[first_char:first_char+last_char+1]
                l = len(t)
                if first_char > 0 and token[first_char-1] not in token_pre_delimiters:
                    continue
                if l > last_char:
                    c = t[last_char]
                    if c in token_post_delimiters:
                        token = token.replace(token_io[i], token_io_replace[i])
                        continue
                elif l == last_char:
                    token = token.replace(token_io[i], token_io_replace[i])
                    continue
        split_replace.append(token)
    replaced_text = ""
    for token in split_replace:
        replaced_text += token + " "
    return replaced_text


# returns macros source with only requested platform
def get_macros_for_platform(platform, macros_source):
    platform = platform.upper()
    platform_macros = ""
    start_str = "#ifdef " + platform
    start = macros_source.find(start_str) + len(start_str)
    end = macros_source.find("#endif //" + platform)
    if start == -1:
        return ""
    platform_macros += macros_source[start:end]
    all_platforms = macros_source.find("//GENERIC MACROS")
    platform_macros += macros_source[all_platforms:]
    return platform_macros


# get info filename for dependency checking
def get_resource_info_filename(filename, build_dir):
    global _info
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


# find generic structs
def find_struct_declarations(shader_text):
    special_structs = ["vs_input", "vs_output", "ps_input", "ps_output", "vs_instance_input"]
    struct_list = []
    start = 0
    while start != -1:
        start = shader_text.find("struct", start)
        if start == -1:
            break
        end = shader_text.find("};", start)
        if end != -1:
            end += 2
            found_struct = shader_text[start:end]
            valid = True
            for ss in special_structs:
                if ss in found_struct:
                    valid = False
            if valid:
                struct_list.append(shader_text[start:end] + "\n")
        start = end
    return struct_list


# find global texture samplers
def find_texture_samplers(shader_text):
    start = shader_text.find("declare_texture_samplers")
    if start == -1:
        return "\n"
    start = shader_text.find("{", start) + 1
    end = shader_text.find("};", start)
    texture_sampler_text = shader_text[start:end] + "\n"
    texture_sampler_text = texture_sampler_text.replace("\t", "")
    texture_sampler_text += "\n"
    return texture_sampler_text


# find struct in shader source
def find_struct(shader_text, decl):
    delimiters = [" ", "\n", "{"]
    start = 0
    while True:
        start = shader_text.find(decl, start)
        if start == -1:
            return ""
        for d in delimiters:
            if shader_text[start+len(decl)] == d:
                end = shader_text.find("};", start)
                end += 2
                if start != -1 and end != -1:
                    return shader_text[start:end] + "\n\n"
                else:
                    return ""
        start += len(decl)


# find cbuffers in source
def find_constant_buffers(shader_text):
    cbuffer_list = []
    start = 0
    while start != -1:
        start = shader_text.find("cbuffer", start)
        if start == -1:
            break
        end = shader_text.find("};", start)
        if end != -1:
            end += 2
            cbuffer_list.append(shader_text[start:end] + "\n")
        start = end
    return cbuffer_list


# find function source
def find_function(shader_text, decl):
    start = shader_text.find(decl)
    if start == -1:
        return ""
    body_pos = shader_text.find("{", start)
    bracket_stack = ["{"]
    text_len = len(shader_text)
    while len(bracket_stack) > 0 and body_pos < text_len:
        body_pos += 1
        character = shader_text[body_pos:body_pos+1]
        if character == "{":
            bracket_stack.insert(0, "{")
        if character == "}" and bracket_stack[0] == "{":
            bracket_stack.pop(0)
            body_pos += 1
    return shader_text[start:body_pos] + "\n\n"


# find functions in source
def find_functions(shader_text):
    deliminator_list = [";", "\n"]
    function_list = []
    start = 0
    while 1:
        start = shader_text.find("(", start)
        if start == -1:
            break
        # make sure the { opens before any other deliminator
        deliminator_pos = shader_text.find(";", start)
        body_pos = shader_text.find("{", start)
        if deliminator_pos < body_pos:
            start = deliminator_pos
            continue
        # find the function name and return type
        function_name = shader_text.rfind(" ", 0, start)
        name_str = shader_text[function_name:start]
        if name_str.find("if:") != -1:
            start = deliminator_pos
            continue
        function_return_type = 0
        for delim in deliminator_list:
            decl_start = shader_text.rfind(delim, 0, function_name)
            if decl_start != -1:
                function_return_type = decl_start
        bracket_stack = ["{"]
        text_len = len(shader_text)
        while len(bracket_stack) > 0 and body_pos < text_len:
            body_pos += 1
            character = shader_text[body_pos:body_pos+1]
            if character == "{":
                bracket_stack.insert(0, "{")
            if character == "}" and bracket_stack[0] == "{":
                bracket_stack.pop(0)
                body_pos += 1
        function_list.append(shader_text[function_return_type:body_pos] + "\n\n")
        start = body_pos
    return function_list


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
    shader_base_name = os.path.basename(filename)
    shader_set_dir = os.path.splitext(shader_base_name)[0]
    shader_set_build_dir = os.path.join(_info.output_dir, shader_set_dir)
    if not os.path.exists(shader_set_build_dir):
        os.makedirs(shader_set_build_dir)
    return shader_file_text, included_files


# gets constants only for this current permutation
def get_permutation_conditionals(pmfx_json, permutation):
    block = pmfx_json.copy()
    if "constants" in block:
        # find conditionals
        conditionals = []
        cblock = block["constants"]
        for key in cblock.keys():
            if key.find("permutation(") != -1:
                conditionals.append((key, cblock[key]))
        # check conditionals valid
        for c in conditionals:
            # remove conditional permutation
            del block["constants"][c[0]]
            full_condition = c[0].replace("permutation", "")
            full_condition = full_condition.replace("&&", "and")
            full_condition = full_condition.replace("||", "or")
            gv = dict()
            for v in permutation:
                gv[str(v[0])] = v[1]
            try:
                if eval(full_condition, gv):
                    block["constants"] = member_wise_merge(block["constants"], c[1])
            except NameError:
                pass
    return block


# get list of technique / permutation specific
def generate_technique_texture_variables(_tp):
    technique_textures = []
    if "texture_samplers" not in _tp.technique.keys():
        return
    textures = _tp.technique["texture_samplers"]
    for t in textures.keys():
        technique_textures.append((textures[t]["type"], t, textures[t]["unit"]))
    return technique_textures


# generate cbuffer meta data, c structs for access in code
def generate_technique_constant_buffers(pmfx_json, _tp):
    offset = 0
    constant_info = [["", 0], ["float", 1], ["float2", 2], ["float3", 3], ["float4", 4], ["float4x4", 16]]

    technique_constants = [_tp.technique]
    technique_json = _tp.technique

    # find inherited constants
    if "inherit_constants" in _tp.technique.keys():
        for inherit in _tp.technique["inherit_constants"]:
            inherit_conditionals = get_permutation_conditionals(pmfx_json[inherit], _tp.permutation)
            technique_constants.append(inherit_conditionals)

    # find all constants
    shader_constant = []
    shader_struct = []
    pmfx_constants = dict()

    for tc in technique_constants:
        if "constants" in tc.keys():
            # sort constants
            sorted_constants = []
            for const in tc["constants"]:
                for ci in constant_info:
                    if ci[0] == tc["constants"][const]["type"]:
                        cc = [const, ci[1]]
                        pos = 0
                        for sc in sorted_constants:
                            if cc[1] > sc[1]:
                                sorted_constants.insert(pos, cc)
                                break
                            pos += 1
                        if pos >= len(sorted_constants):
                            sorted_constants.append(cc)
            for const in sorted_constants:
                const_name = const[0]
                const_elems = const[1]
                pmfx_constants[const_name] = tc["constants"][const_name]
                pmfx_constants[const_name]["offset"] = offset
                pmfx_constants[const_name]["num_elements"] = const_elems
                shader_constant.append("    " + tc["constants"][const_name]["type"] + " " + "m_" + const_name + ";\n")
                shader_struct.append("    " + tc["constants"][const_name]["type"] + " " + "m_" + const_name + ";\n")
                offset += const_elems

    if offset == 0:
        return _tp.technique, "", ""

    # we must pad to 16 bytes alignment
    pre_pad_offset = offset
    diff = offset / 4
    next = math.ceil(diff)
    pad = (next - diff) * 4
    if pad != 0:
        shader_constant.append("    " + constant_info[int(pad)][0] + " " + "m_padding" + ";\n")
        shader_struct.append("    " + constant_info[int(pad)][0] + " " + "m_padding" + ";\n")

    offset += pad

    cb_str = "cbuffer material_data : register(b7)\n"
    cb_str += "{\n"
    for sc in shader_constant:
        cb_str += sc
    cb_str += "};\n"

    # append permutation string to shader c struct
    skips = [_info.shader_platform.upper(), _info.shader_sub_platform.upper()]
    permutation_name = ""
    if int(_tp.id) != 0:
        for p in _tp.permutation:
            if p[0] in skips:
                continue
            if p[1] == 1:
                permutation_name += "_" + p[0].lower()
            if p[1] > 1:
                permutation_name += "_" + p[0].lower() + p[1]

    c_struct = "struct " + _tp.technique_name + permutation_name + "\n"
    c_struct += "{\n"
    for ss in shader_struct:
        c_struct += ss
    c_struct += "};\n\n"

    technique_json["constants"] = pmfx_constants
    technique_json["constants_used_bytes"] = int(pre_pad_offset * 4)
    technique_json["constants_size_bytes"] = int(offset * 4)
    assert int(offset * 4) % 16 == 0

    return technique_json, c_struct, cb_str


def strip_empty_inputs(input, main):
    conditioned = input.replace("\n", "").replace(";", "").replace(";", "").replace("}", "").replace("{", "")
    tokens = conditioned.split(" ")
    for t in tokens:
        if t == "":
            tokens.remove(t)
    if len(tokens) == 2:
        # input is empty so remove from vs_main args
        input = ""
        name = tokens[1]
        pos = main.find(name)
        prev_delim = max(us(main[:pos].rfind(",")), us(main[:pos].rfind("(")))
        next_delim = pos + min(us(main[pos:].find(",")), us(main[pos:].find(")")))
        main = main.replace(main[prev_delim:next_delim], " ")
    return input, main


# evaluate permutation / technique defines in if: blocks and remove unused branches
def evaluate_conditional_blocks(source, permutation):
    if not permutation:
        return source
    pos = 0
    case_accepted = False
    while True:
        else_pos = source.find("else:", pos)
        else_if_pos = source.find("else if:", pos)
        pos = source.find("if:", pos)
        else_case = False
        first_case = True

        if us(else_if_pos) < us(pos):
            pos = else_if_pos
            first_case = False

        if us(else_pos) < us(pos):
            pos = else_pos
            else_case = True
            first_case = False

        if first_case:
            case_accepted = False

        if pos == -1:
            break

        if not else_case:
            conditions_start = source.find("(", pos)
            body_start = source.find("{", conditions_start) + 1
            conditions = source[conditions_start:body_start - 1]
            conditions = conditions.replace('\n', '')
            conditions = conditions.replace("&&", " and ")
            conditions = conditions.replace("||", " or ")
            conditions = conditions.replace("!", " not ")
        else:
            body_start = source.find("{", pos) + 1
            conditions = "True"

        gv = dict()
        for v in permutation:
            gv[str(v[0])] = v[1]

        conditional_block = ""

        i = body_start
        stack_size = 1
        while True:
            if source[i] == "{":
                stack_size += 1
            if source[i] == "}":
                stack_size -= 1
            if stack_size == 0:
                break
            i += 1

        if not case_accepted:
            try:
                if eval(conditions, gv):
                    conditional_block = source[body_start:i]
                    case_accepted = True
            except NameError:
                conditional_block = ""
        else:
            conditional_block = ""

        source = source.replace(source[pos:i+1], conditional_block)
        pos += len(conditional_block)

    return source


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
def generate_permutations(technique, technique_json):
    global _info
    output_permutations = []
    define_list = []
    permutation_options = dict()
    permutation_option_mask = 0
    define_string = ""
    define_list.append((_info.shader_platform.upper(), [1], -1))
    define_list.append((_info.shader_sub_platform.upper(), [1], -1))
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

    return tp, permutation_options, permutation_option_mask, define_list, define_string


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
        # pmfx json exists, return the block
        json_loc = shader_file_text.find("{", pmfx_loc)
        pmfx_end = enclose_brackets(shader_file_text[pmfx_loc:])
        pmfx_json = json.loads(shader_file_text[json_loc:pmfx_end + json_loc])
        return pmfx_json
    else:
        # shader can have no pmfx, provided it supplies vs_main and ps_main
        if find_function(shader_file_text, "vs_main") and find_function(shader_file_text, "ps_main"):
            pmfx_json = dict()
            pmfx_json["default"] = {"vs": "vs_main", "ps": "ps_main"}
            return pmfx_json
    return None


# find only used textures
def find_used_textures(shader_source, texture_decl):
    if not texture_decl:
        return
    # find texture uses
    uses = ["sample_texture", "read_texture", "write_texture"]
    texture_uses = []
    pos = 0
    while True:
        sampler = -1
        for u in uses:
            sampler = shader_source.find(u, pos)
            if sampler != -1:
                break
        if sampler == -1:
            break;
        start = shader_source.find("(", sampler)
        end = shader_source.find(")", sampler)
        if us(sampler) < us(start) < us(end):
            args = shader_source[start+1:end-1].split(",")
            if len(args) > 0:
                name = args[0].strip(" ")
                if name not in texture_uses:
                    texture_uses.append(name)
        pos = end
    used_texture_decl = ""
    texture_list = texture_decl.split(";")
    for texture in texture_list:
        start = texture.find("(") + 1
        end = texture.find(")") - 1
        args = texture[start:end].split(",")
        name_positions = [0, 2]  # 0 = single sample texture, 2 = msaa texture
        for p in name_positions:
            if len(args) > p:
                name = args[p].strip(" ")
                if name in texture_uses:
                    used_texture_decl = used_texture_decl.strip(" ")
                    used_texture_decl += texture + ";\n"
    return used_texture_decl


# find only used cbuffers
def find_used_cbuffers(shader_source, cbuffers):
    # turn source to tokens
    non_tokens = ["(", ")", "{", "}", ".", ",", "+", "-", "=", "*", "/", "&", "|", "~", "\n", "<", ">", "[", "]", ";"]
    token_source = shader_source
    for nt in non_tokens:
        token_source = token_source.replace(nt, " ")
    token_list = token_source.split(" ")
    used_cbuffers = []
    for cbuf in cbuffers:
        member_list = parse_and_split_block(cbuf)
        for i in range(1, len(member_list), 2):
            member = member_list[i].strip()
            array = member.find("[")
            if array != -1:
                if array == 0:
                    i += 1
                    continue
                else:
                    member = member[:array]
            if member in token_list:
                used_cbuffers.append(cbuf)
                break
    return used_cbuffers


# find only used functions from a given entry point
def find_used_functions(entry_func, function_list):
    used_functions = [entry_func]
    added_function_names = []
    ordered_function_list = [entry_func]
    for used_func in used_functions:
        for func in function_list:
            if func == used_func:
                continue
            name = func.split(" ")[1]
            end = name.find("(")
            name = name[0:end]
            if used_func.find(name + "(") != -1:
                if name in added_function_names:
                    continue
                used_functions.append(func)
                added_function_names.append(name)
    for func in function_list:
        name = func.split(" ")[1]
        end = name.find("(")
        name = name[0:end]
        if name in added_function_names:
            ordered_function_list.append(func)
    ordered_function_list.remove(entry_func)
    used_function_source = ""
    for used_func in ordered_function_list:
        used_function_source += used_func + "\n\n"
    return used_function_source


# generate a vs, ps or cs from _tp (technique permutation data)
def generate_single_shader(main_func, _tp):
    _si = single_shader_info()
    _si.main_func_name = main_func

    # find main func
    main = ""
    for func in _tp.functions:
        pos = func.find(main_func)
        if pos != -1:
            if func[pos+len(main_func)] == "(" and func[pos-1] == " ":
                main = func

    if main == "":
        print("error: could not find main function " + main_func)
        return None

    # find used functions,
    _si.functions_source = find_used_functions(main, _tp.functions)

    # find inputs / outputs
    _si.instance_input_struct_name = None
    _si.output_struct_name = main[0:main.find(" ")].strip()
    input_signature = main[main.find("(")+1:main.find(")")].split(" ")
    for i in range(0, len(input_signature)):
        input_signature[i] = input_signature[i].replace(",", "")
        if input_signature[i] == "_input" or input_signature[i] == "input":
            _si.input_struct_name = input_signature[i-1]
        elif input_signature[i] == "_instance_input" or input_signature[i] == "instance_input":
            _si.instance_input_struct_name = input_signature[i-1]

    # find source decl for inputs / outputs
    if _si.instance_input_struct_name:
        _si.instance_input_decl = find_struct(_tp.source, "struct " + _si.instance_input_struct_name)
    _si.input_decl = find_struct(_tp.source, "struct " + _si.input_struct_name)
    _si.output_decl = find_struct(_tp.source, "struct " + _si.output_struct_name)

    # remove empty inputs which have no members due to permutation conditionals
    _si.input_decl, main = strip_empty_inputs(_si.input_decl, main)

    # condition main function with stripped inputs
    if _si.instance_input_struct_name:
        _si.instance_input_decl, main = strip_empty_inputs(_si.instance_input_decl, main)
        if _si.instance_input_decl == "":
            _si.instance_input_struct_name = None
    _si.main_func_source = main

    # find only used textures by this shader
    full_source = _si.functions_source + main
    _si.texture_decl = find_used_textures(full_source, _tp.texture_decl)
    _si.cbuffers = find_used_cbuffers(full_source, _tp.cbuffers)

    return _si


# format source with indents
def format_source(source, indent_size):
    formatted = ""
    lines = source.split("\n")
    indent = 0
    indents = ["{"]
    unindnets = ["}"]
    for line in lines:
        cur_indent = indent
        line = line.strip(" ")
        if len(line) < 1:
            continue
        if line[0] in indents:
            indent += 1
        elif line[0] in unindnets:
            indent -= 1
            cur_indent = indent
        for i in range(0, cur_indent*indent_size):
            formatted += " "
        formatted += line
        formatted += "\n"
    return formatted


# compile hlsl shader model 4
def compile_hlsl(_info, pmfx_name, _tp, _shader):
    shader_source = get_macros_for_platform("hlsl", _info.macros_source)
    shader_source += _tp.struct_decls
    for cb in _shader.cbuffers:
        shader_source += cb
    shader_source += _shader.input_decl
    shader_source += _shader.instance_input_decl
    shader_source += _shader.output_decl
    shader_source += _shader.texture_decl
    shader_source += _shader.functions_source
    if _shader.shader_type == "cs":
        shader_source += "[numthreads(16, 16, 1)]"
    shader_source += _shader.main_func_source
    shader_source = format_source(shader_source, 4)

    exe = os.path.join(_info.tools_dir, "bin", "fxc", "fxc")

    # default sm 4
    if _info.shader_version == "":
        _info.shader_version = "4_0"

    sm = str(_info.shader_version)

    shader_model = {
        "vs": "vs_" + sm,
        "ps": "ps_" + sm,
        "cs": "cs_" + sm
    }

    extension = {
        "vs": ".vs",
        "ps": ".ps",
        "cs": ".cs"
    }

    temp_path = os.path.join(_info.root_dir, "temp", pmfx_name)
    output_path = os.path.join(_info.output_dir, pmfx_name)
    os.makedirs(temp_path, exist_ok=True)
    os.makedirs(output_path, exist_ok=True)

    temp_file_and_path = os.path.join(temp_path, _tp.name + extension[_shader.shader_type])
    output_file_and_path = os.path.join(output_path, _tp.name + extension[_shader.shader_type] + "c")

    temp_shader_source = open(temp_file_and_path, "w")
    temp_shader_source.write(shader_source)
    temp_shader_source.close()

    cmdline = exe + " "
    cmdline += "/T " + shader_model[_shader.shader_type] + " "
    cmdline += "/E " + _shader.main_func_name + " "
    cmdline += "/Fo " + output_file_and_path + " " + temp_file_and_path + " "

    # process = subprocess.Popen([exe + ".exe", cmdline], shell=True, stdout=subprocess.PIPE)
    # rt = subprocess.call(cmdline, shell=True)

    p = subprocess.Popen(cmdline, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    error_code = p.wait()
    output, err = p.communicate()
    err_str = err.decode('utf-8')
    err_str = err_str.strip(" ")
    err_list = err_str.split("\n")
    for e in err_list:
        if e != "":
            print(e)

    if error_code != 0:
        _info.error_code = error_code

    return error_code


# parse shader inputs annd output source into a list of elements and semantics
def parse_io_struct(source):
    if len(source) == 0:
        return [], []
    io_source = source
    start = io_source.find("{")
    end = io_source.find("}")
    elements = []
    semantics = []
    prev_input = start+1
    next_input = 0
    while next_input < end:
        next_input = io_source.find(";", prev_input)
        if next_input > 0:
            next_semantic = io_source.find(":", prev_input)
            elements.append(io_source[prev_input:next_semantic].strip())
            semantics.append(io_source[next_semantic+1:next_input].strip())
            prev_input = next_input + 1
        else:
            break
    # the last input will always be "};" pop it out
    elements.pop(len(elements)-1)
    semantics.pop(len(semantics)-1)
    return elements, semantics


# generate a global struct to access input structures in a hlsl like manner
def generate_global_io_struct(io_elements, decl):
    # global input struct for hlsl compatibility to access like input.value
    struct_source = decl
    struct_source += "\n{\n"
    for element in io_elements:
        struct_source += element + ";\n"
    struct_source += "};\n"
    struct_source += "\n"
    return struct_source


# assign vs or ps inputs to the global struct
def generate_input_assignment(io_elements, decl, local_var, suffix):
    assign_source = "//assign " + decl + " struct from glsl inputs\n"
    assign_source += decl + " " + local_var + ";\n"
    for element in io_elements:
        if element.split()[1] == "position" and "vs_output" in decl:
            continue
        var_name = element.split()[1]
        assign_source += local_var + "." + var_name + " = " + var_name + suffix + ";\n"
    return assign_source


# assign vs or ps outputs from the global struct to the output locations
def generate_output_assignment(io_elements, local_var, suffix):
    assign_source = "\n//assign glsl global outputs from structs\n"
    for element in io_elements:
        var_name = element.split()[1]
        if var_name == "position":
           assign_source += "gl_Position = " + local_var + "." + var_name + ";\n"
        else:
            assign_source += var_name + suffix + " = " + local_var + "." + var_name + ";\n"
    return assign_source


# generates a texture declaration from a texture list
def generate_texture_decl(texture_list):
    if not texture_list:
        return ""
    texture_decl = ""
    for alias in texture_list:
        decl = str(alias[0]) + "( " + str(alias[1]) + ", " + str(alias[2]) + " );\n"
        texture_decl += decl
    return texture_decl


# compile glsl
def compile_glsl(_info, pmfx_name, _tp, _shader):
    # parse inputs and outputs into semantics
    inputs, input_semantics = parse_io_struct(_shader.input_decl)
    outputs, output_semantics = parse_io_struct(_shader.output_decl)
    instance_inputs, instance_input_semantics = parse_io_struct(_shader.instance_input_decl)

    uniform_buffers = ""
    for cbuf in _shader.cbuffers:
        name_start = cbuf.find(" ")
        name_end = cbuf.find(":")
        if name_end == -1:
            continue
        uniform_buf = "layout (std140) uniform"
        uniform_buf += cbuf[name_start:name_end]
        body_start = cbuf.find("{")
        body_end = cbuf.find("};") + 2
        uniform_buf += "\n"
        uniform_buf += cbuf[body_start:body_end] + "\n"
        uniform_buffers += uniform_buf + "\n"

    # header and macros
    shader_source = ""
    if _info.shader_sub_platform == "gles":
        shader_source += "#version 300 es\n"
        shader_source += "#define GLSL\n"
        shader_source += "#define GLES\n"
        shader_source += "precision highp float;\n"
    else:
        shader_source += "#version 330 core\n"
        shader_source += "#define GLSL\n"
    shader_source += "//" + pmfx_name + " " + _tp.name + " " + _shader.shader_type + " " + str(_tp.id) + "\n"
    shader_source += get_macros_for_platform("glsl", _info.macros_source)

    # input structs
    index_counter = 0
    for input in inputs:
        if _shader.shader_type == "vs":
            shader_source += "layout(location = " + str(index_counter) + ") in " + input + "_vs_input;\n"
        elif _shader.shader_type == "ps":
            shader_source += "in " + input + "_vs_output;\n"
        index_counter += 1
    for instance_input in instance_inputs:
        shader_source += "layout(location = " + str(index_counter) + ") in " + instance_input + "_instance_input;\n"
        index_counter += 1

    # outputs structs
    if _shader.shader_type == "vs":
        for output in outputs:
            if output.split()[1] != "position":
                shader_source += "out " + output + "_" + _shader.shader_type + "_output;\n"
    elif _shader.shader_type == "ps":
        for p in range(0, len(outputs)):
            if "SV_Depth" in output_semantics[p]:
                continue
            else:
                output_index = output_semantics[p].replace("SV_Target", "")
                if output_index != "":
                    shader_source += "layout(location = " + output_index + ") "
                shader_source += "out " + outputs[p] + "_ps_output;\n"

    # global structs for access to inputs or outputs from any function
    shader_source += generate_global_io_struct(inputs, "struct " + _shader.input_struct_name)
    if _shader.instance_input_struct_name:
        if len(instance_inputs) > 0:
            shader_source += generate_global_io_struct(instance_inputs, "struct " + _shader.instance_input_struct_name)
    if len(outputs) > 0:
        shader_source += generate_global_io_struct(outputs, "struct " + _shader.output_struct_name)

    shader_source += _tp.struct_decls
    shader_source += uniform_buffers
    shader_source += _shader.texture_decl
    shader_source += _shader.functions_source

    glsl_main = _shader.main_func_source
    skip_function_start = glsl_main.find("{") + 1
    skip_function_end = glsl_main.find("return")
    glsl_main = glsl_main[skip_function_start:skip_function_end].strip()

    input_name = {
        "vs": "_vs_input",
        "ps": "_vs_output"
    }

    output_name = {
        "vs": "_vs_output",
        "ps": "_ps_output"
    }

    pre_assign = generate_input_assignment(inputs, _shader.input_struct_name, "_input", input_name[_shader.shader_type])
    if _shader.instance_input_struct_name:
        if len(instance_inputs) > 0:
            pre_assign += generate_input_assignment(instance_inputs,
                                                    _shader.instance_input_struct_name, "instance_input", "_instance_input")
    post_assign = generate_output_assignment(outputs, "_output", output_name[_shader.shader_type])

    shader_source += "void main()\n{\n"
    shader_source += "\n" + pre_assign + "\n"
    shader_source += glsl_main
    shader_source += "\n" + post_assign + "\n"
    shader_source += "}\n"

    # condition source
    shader_source = replace_io_tokens(shader_source)
    shader_source = format_source(shader_source, 4)

    extension = {
        "vs": ".vsc",
        "ps": ".psc"
    }

    temp_extension = {
        "vs": ".vert",
        "ps": ".frag"
    }

    temp_path = os.path.join(_info.root_dir, "temp", pmfx_name)
    output_path = os.path.join(_info.output_dir, pmfx_name)
    os.makedirs(temp_path, exist_ok=True)
    os.makedirs(output_path, exist_ok=True)

    temp_file_and_path = os.path.join(temp_path, _tp.name + temp_extension[_shader.shader_type])

    temp_shader_source = open(temp_file_and_path, "w")
    temp_shader_source.write(shader_source)
    temp_shader_source.close()

    output_path = os.path.join(_info.output_dir, pmfx_name)
    os.makedirs(output_path, exist_ok=True)

    output_file_and_path = os.path.join(output_path, _tp.name + extension[_shader.shader_type])

    exe = os.path.join(_info.tools_dir, "bin", "glsl", util.get_platform_name(), "validator")

    p = subprocess.Popen(exe + " " + temp_file_and_path, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    error_code = p.wait()
    output, err = p.communicate()
    output = output.decode('utf-8')
    output = output.strip(" ")
    output = output.split("\n")

    for e in output:
        if e != "":
            print(e)

    if error_code != 0:
        _info.error_code = error_code

    # copy shader to data
    shader_file = open(output_file_and_path, "w")
    shader_file.write(shader_source)
    shader_file.close()

    return error_code


# we need to convert ubytes 255 to float 1.0
def convert_ubyte_to_float(semantic):
    if semantic.find("COLOR"):
        return False
    return True


# gets metal packed types from hlsl semantic, all types are float except COLOR: uchar, BLENDINDICES uchar
def get_metal_packed_decl(stage_in, input, semantic):
    vector_sizes = ["2", "3", "4"]
    packed_decl = ""
    if not stage_in:
        packed_decl = "packed_"
    split = input.split(" ")
    type = split[0]
    if semantic.find("COLOR") != -1 or semantic.find("BLENDINDICES") != -1:
        packed_decl += "uchar"
        count = type[len(type)-1]
        if count in vector_sizes:
            packed_decl += count
    else:
        packed_decl += type
    for i in range(1, len(split)):
        packed_decl += " " + split[i]
    return packed_decl


def find_token(token, string):
    delimiters = [",", " ", "\n", "\t", ")", "(", "=", "!", ">", "<", ";", "[", "]"]
    fp = string.find(token)
    if fp != -1:
        left = False
        right = False
        # check left
        if fp > 0:
            for d in delimiters:
                if string[fp-1] == d:
                    left = True
                    break
        else:
            left = True
        # check right
        ep = fp + len(token)
        if fp < ep-1:
            for d in delimiters:
                if string[ep] == d:
                    right = True
                    break
        else:
            right = True
        if left and right:
            return fp
    return -1


def metal_functions(functions, cbuffers, textures):
    cbuf_members_list = []
    for c in cbuffers:
        cbuf_members = parse_and_split_block(c)
        cbuf_members_list.append(cbuf_members)
    texture_list = textures.split(";")
    texture_args = []
    for t in texture_list:
        cpos = t.find(",")
        if cpos == -1:
            continue
        spos = t.find("(")
        macro_args = t[spos + 1:].split(",")
        tex_type = t[:spos] + "_arg"
        name_pos = 0
        if t.find("texture_2dms") != -1:
            name_pos = 2
        name = macro_args[name_pos].strip()
        texture_args.append((name, tex_type + "(" + name + ")"))
    fl = find_functions(functions)
    final_funcs = ""
    func_sig_additions = dict()
    for f in fl:
        bp = f.find("(")
        ep = f.find(")")
        fb = f[ep:]
        fn = f.find(" ")
        fn = f[fn+1:bp]
        sig = f[:bp+1]
        count = 0
        # insert cbuf members
        for c in cbuf_members_list:
            for i in range(0, len(c), 2):
                ap = c[i+1].find("[")
                member = c[i+1]
                if ap != -1:
                    member = member[:ap]
                if find_token(member, fb) != -1:
                    if count > 0:
                        sig += ",\n"
                    if fn in func_sig_additions.keys():
                        func_sig_additions[fn].append(member)
                    else:
                        func_sig_additions[fn] = [member]
                    ref_type = "& "
                    if ap != -1:
                        ref_type = "* "
                    sig += "constant " + c[i] + ref_type + member
                    count += 1
        # insert texture members
        for t in texture_args:
            if find_token(t[0], fb) != -1:
                if count > 0:
                    sig += ",\n"
                sig += t[1]
                count += 1
                if fn in func_sig_additions.keys():
                    func_sig_additions[fn].append(t[0])
                    func_sig_additions[fn].append("sampler_" + t[0])
                else:
                    func_sig_additions[fn] = [t[0]]
                    func_sig_additions[fn].append("sampler_" + t[0])
        if bp != -1 and ep != -1:
            args = f[bp+1:ep]
            arg_list = args.split(",")
            for arg in arg_list:
                if count > 0:
                    sig += ",\n"
                count += 1
                address_space = "thread"
                toks = arg.split(" ")
                if '' in toks:
                    toks.remove('')
                if '\n' in toks:
                    toks.remove('\n')
                ref = False
                for t in toks:
                    if t == "out" or t == "inout":
                        ref = True
                    if t == "in":
                        address_space = "constant"
                        ref = True
                if not ref:
                    sig += arg
                else:
                    sig += address_space + " " + toks[1] + "& " + toks[2]
        # find used cbuf memb
        func = sig + fb
        final_funcs += func
    return final_funcs, func_sig_additions


def insert_function_sig_additions(function_body, function_sig_additions):
    for k in function_sig_additions.keys():
        op = 0
        fp = 0
        while fp != -1:
            fp = find_token(k, function_body[op:])
            if fp != -1:
                fp = op + fp
                fp += len(k)
                insert_string = function_body[:fp+1]
                for a in function_sig_additions[k]:
                    insert_string += a + ", "
                insert_string += function_body[fp+1:]
                function_body = insert_string
                op = fp
    return function_body


# compile shader for apple metal
def compile_metal(_info, pmfx_name, _tp, _shader):
    # parse inputs and outputs into semantics
    inputs, input_semantics = parse_io_struct(_shader.input_decl)
    outputs, output_semantics = parse_io_struct(_shader.output_decl)
    instance_inputs, instance_input_semantics = parse_io_struct(_shader.instance_input_decl)

    shader_source = get_macros_for_platform("metal", _info.macros_source)
    shader_source += "using namespace metal;\n"

    # struct decls
    shader_source += _tp.struct_decls

    # cbuffer decls
    metal_cbuffers = []
    for cbuf in _shader.cbuffers:
        name_start = cbuf.find(" ")
        name_end = cbuf.find(":")
        body_start = cbuf.find("{")
        body_end = cbuf.find("};") + 2
        register_start = cbuf.find("(") + 1
        register_end = cbuf.find(")")
        name = cbuf[name_start:name_end].strip()
        reg = cbuf[register_start:register_end]
        reg = reg.replace('b', '')
        metal_cbuffers.append((name, reg))
        shader_source += "struct c_" + name + "\n"
        shader_source += cbuf[body_start:body_end]
        shader_source += "\n"

    # packed inputs
    vs_stage_in = False
    attrib_index = 0
    if _shader.shader_type == "vs":
        if len(inputs) > 0:
            shader_source += "struct packed_" + _shader.input_struct_name + "\n{\n"
            for i in range(0, len(inputs)):
                shader_source += get_metal_packed_decl(vs_stage_in, inputs[i], input_semantics[i])
                if vs_stage_in:
                    shader_source += " [[attribute(" + str(attrib_index) + ")]]"
                shader_source += ";\n"
                attrib_index += 1
            shader_source += "};\n"

        if _shader.instance_input_struct_name:
            if len(instance_inputs) > 0:
                shader_source += "struct packed_" + _shader.instance_input_struct_name + "\n{\n"
                for i in range(0, len(instance_inputs)):
                    shader_source += get_metal_packed_decl(vs_stage_in, instance_inputs[i], instance_input_semantics[i])
                    if vs_stage_in:
                        shader_source += " [[attribute(" + str(attrib_index) + ")]]"
                    shader_source += ";\n"
                    attrib_index += 1
                shader_source += "};\n"

    # inputs
    if len(inputs) > 0:
        shader_source += "struct " + _shader.input_struct_name + "\n{\n"
        for i in range(0, len(inputs)):
            shader_source += inputs[i] + ";\n"
        shader_source += "};\n"

    if _shader.instance_input_struct_name:
        if len(instance_inputs) > 0:
            shader_source += "struct " + _shader.instance_input_struct_name + "\n{\n"
            for i in range(0, len(instance_inputs)):
                shader_source += instance_inputs[i] + ";\n"
            shader_source += "};\n"

    # outputs
    if len(outputs) > 0:
        shader_source += "struct " + _shader.output_struct_name + "\n{\n"
        for i in range(0, len(outputs)):
            shader_source += outputs[i]
            if output_semantics[i].find("SV_POSITION") != -1:
                shader_source += " [[position]]"
            # mrt
            sv_pos = output_semantics[i].find("SV_Target")
            if sv_pos != -1:
                channel_pos = sv_pos + len("SV_Target")
                if channel_pos < len(output_semantics[i]):
                    shader_source += " [[color(" + output_semantics[i][channel_pos] + ")]]"
                else:
                    shader_source += " [[color(0)]]"
            sv_pos = output_semantics[i].find("SV_Depth")
            if sv_pos != -1:
                shader_source += " [[depth(any)]]"
            shader_source += ";\n"
        shader_source += "};\n"

    main_type = {
        "vs": "vertex",
        "ps": "fragment",
        "cs": "kernel"
    }

    # functions
    function_source, function_sig_additions = metal_functions(_shader.functions_source, _shader.cbuffers, _shader.texture_decl)
    shader_source += function_source

    # main decl
    shader_source += main_type[_shader.shader_type] + " "
    shader_source += _shader.output_struct_name + " " + _shader.shader_type + "_main" + "("

    if _shader.shader_type == "vs" and not vs_stage_in:
        shader_source += "\n  device packed_" + _shader.input_struct_name + "* vertices" + "[[buffer(0)]]"
        shader_source += "\n, uint vid [[vertex_id]]"
        if _shader.instance_input_struct_name:
            if len(instance_inputs) > 0:
                shader_source += "\n, device packed_" + _shader.instance_input_struct_name + "* instances" + "[[buffer(1)]]"
                shader_source += "\n, uint iid [[instance_id]]"
    elif _shader.shader_type == "vs":
        shader_source += "\n  packed_" + _shader.input_struct_name + " in_vertex [[stage_in]]"
        if _shader.instance_input_struct_name:
            if len(instance_inputs) > 0:
                shader_source += "\n, packed_" + _shader.instance_input_struct_name + " in_instance [[stage_in]]"
    elif _shader.shader_type == "ps":
        shader_source += _shader.input_struct_name + " input [[stage_in]]"
    elif _shader.shader_type == "cs":
        shader_source += "uint2 gid[[thread_position_in_grid]]"

    # pass in textures
    invalid = ["", "\n"]
    texture_list = _shader.texture_decl.split(";")
    for texture in texture_list:
        if texture not in invalid:
            shader_source += "\n, " + texture.strip("\n")

    # pass in cbuffers.. cbuffers start at 8 reserving space for 8 vertex buffers..
    for cbuf in metal_cbuffers:
        regi = int(cbuf[1]) + 8
        shader_source += "\n, " + "constant " "c_" + cbuf[0] + " &" + cbuf[0] + " [[buffer(" + str(regi) + ")]]"

    shader_source += ")\n{\n"

    vertex_array_index = "(vertices[vid]."
    instance_array_index = "(instances[iid]."
    if vs_stage_in:
        vertex_array_index = "(in_vertex."
        instance_array_index = "(in_instance."


    # create function prologue for main and insert assignment to unpack vertex
    from_ubyte = "0.00392156862"
    if _shader.shader_type == "vs":
        shader_source += _shader.input_struct_name + " input;\n"
        v_inputs = [(inputs, input_semantics, "input.", vertex_array_index)]
        if _shader.instance_input_struct_name:
            if len(instance_inputs) > 0:
                shader_source += _shader.instance_input_struct_name + " instance_input;\n"
                v_inputs.append((instance_inputs, instance_input_semantics, "instance_input.", instance_array_index))
        for vi in v_inputs:
            for i in range(0, len(vi[0])):
                split_input = vi[0][i].split(" ")
                input_name = split_input[1]
                input_unpack_type = split_input[0]
                shader_source += vi[2] + input_name + " = "
                shader_source += input_unpack_type
                shader_source += vi[3] + input_name
                # convert ubyte to float
                if convert_ubyte_to_float(vi[1][i]):
                    shader_source += ") * " + from_ubyte + ";"
                else:
                    shader_source += ");\n"

    # create a function prologue for cbuffer assignment
    for c in range(0, len(_shader.cbuffers)):
        cbuf_members = parse_and_split_block(_shader.cbuffers[c])
        for i in range(0, len(cbuf_members), 2):
            ref_type = "& "
            point = ""
            decl = cbuf_members[i + 1]
            assign = decl
            array_pos = cbuf_members[i + 1].find("[")
            if array_pos != -1:
                decl = decl[:array_pos]
                ref_type = "* "
                assign = decl + "[0]"
                point = "&"
            shader_source += "constant " + cbuf_members[i] + ref_type + decl
            shader_source += " = " + point + metal_cbuffers[c][0] + "." + assign
            shader_source += ";\n"

    main_func_body = _shader.main_func_source.find("{") + 1
    main_body_source =  _shader.main_func_source[main_func_body:]
    main_body_source = insert_function_sig_additions(main_body_source, function_sig_additions)

    shader_source += main_body_source
    shader_source = format_source(shader_source, 4)

    temp_path = os.path.join(_info.root_dir, "temp", pmfx_name)
    output_path = os.path.join(_info.output_dir, pmfx_name)
    os.makedirs(temp_path, exist_ok=True)
    os.makedirs(output_path, exist_ok=True)

    extension = {
        "vs": "_vs.metal",
        "ps": "_ps.metal",
        "cs": "_cs.metal"
    }

    output_extension = {
        "vs": ".vsc",
        "ps": ".psc",
        "cs": ".csc"
    }

    temp_file_and_path = os.path.join(temp_path, _tp.name + extension[_shader.shader_type])
    output_file_and_path = os.path.join(output_path, _tp.name + output_extension[_shader.shader_type])

    temp_shader_source = open(output_file_and_path, "w")
    temp_shader_source.write(shader_source)
    temp_shader_source.close()

    # 0 is error code
    return 0

    # todo precompile
    if pmfx_name != "forward_render":
        return 0

    temp_shader_source = open(temp_file_and_path, "w")
    temp_shader_source.write(shader_source)
    temp_shader_source.close()

    # compile .air
    cmdline = "xcrun -sdk macosx metal -c "
    cmdline += temp_file_and_path + " "
    cmdline += "-o " + output_file_and_path

    subprocess.call(cmdline, shell=True)


# generate a shader info file with an array of technique permutation descriptions and dependency timestamps
def generate_shader_info(filename, included_files, techniques):
    global _info
    info_filename, base_filename, dir_path = get_resource_info_filename(filename, _info.output_dir)

    shader_info = dict()
    shader_info["files"] = []
    shader_info["techniques"] = techniques["techniques"]

    # special files which affect the validity of compiled shaders
    shader_info["files"].append(dependencies.create_info(_info.this_file))
    shader_info["files"].append(dependencies.create_info(_info.macros_file))

    included_files.insert(0, os.path.join(dir_path, base_filename))
    for ifile in included_files:
        full_name = os.path.join(_info.root_dir, ifile)
        shader_info["files"].append(dependencies.create_info(full_name))

    output_info = open(info_filename, 'wb+')
    output_info.write(bytes(json.dumps(shader_info, indent=4), 'UTF-8'))
    output_info.close()
    return shader_info


# generate json description of vs inputs and outputs
def generate_input_info(inputs):
    semantic_info = [
        ["SV_POSITION", "4"],
        ["POSITION", "4"],
        ["TEXCOORD", "4"],
        ["NORMAL", "4"],
        ["TANGENT", "4"],
        ["BITANGENT", "4"],
        ["COLOR", "1"],
        ["BLENDINDICES", "1"]
    ]
    type_info = ["int", "uint", "float", "double"]
    input_desc = []
    inputs_split = parse_and_split_block(inputs)
    offset = int(0)
    for i in range(0, len(inputs_split), 3):
        num_elements = 1
        element_size = 1
        for type in type_info:
            if inputs_split[i].find(type) != -1:
                str_num = inputs_split[i].replace(type, "")
                if str_num != "":
                    num_elements = int(str_num)
        for sem in semantic_info:
            if inputs_split[i+2].find(sem[0]) != -1:
                semantic_id = semantic_info.index(sem)
                semantic_name = sem[0]
                semantic_index = inputs_split[i+2].replace(semantic_name, "")
                if semantic_index == "":
                    semantic_index = "0"
                element_size = sem[1]
                break
        size = int(element_size) * int(num_elements)
        input_attribute = {
            "name": inputs_split[i+1],
            "semantic_index": int(semantic_index),
            "semantic_id": int(semantic_id),
            "size": int(size),
            "element_size": int(element_size),
            "num_elements": int(num_elements),
            "offset": int(offset),
        }
        input_desc.append(input_attribute)
        offset += size
    return input_desc


# generate metadata for the technique with info about textures, cbuffers, inputs, outputs, binding points and more
def generate_technique_permutation_info(_tp):
    _tp.technique["name"] = _tp.technique_name
    # textures
    texture_samplers_split = parse_and_split_block(_tp.texture_decl)
    i = 0
    _tp.technique["texture_sampler_bindings"] = []
    while i < len(texture_samplers_split):
        offset = i
        tex_type = texture_samplers_split[i+0]
        if tex_type == "texture_2dms":
            data_type = texture_samplers_split[i+1]
            fragments = texture_samplers_split[i+2]
            offset = i+2
        else:
            data_type = "float4"
            fragments = 1
        sampler_desc = {
            "name": texture_samplers_split[offset+1],
            "data_type": data_type,
            "fragments": fragments,
            "type": tex_type,
            "unit": int(texture_samplers_split[offset+2])
        }
        i = offset+3
        _tp.technique["texture_sampler_bindings"].append(sampler_desc)
    # cbuffers
    _tp.technique["cbuffers"] = []
    for buffer in _tp.cbuffers:
        pos = buffer.find("{")
        if pos == -1:
            continue
        buffer_decl = buffer[0:pos-1]
        buffer_decl_split = buffer_decl.split(":")
        buffer_name = buffer_decl_split[0].split()[1]
        buffer_loc_start = buffer_decl_split[1].find("(") + 1
        buffer_loc_end = buffer_decl_split[1].find(")", buffer_loc_start)
        buffer_reg = buffer_decl_split[1][buffer_loc_start:buffer_loc_end]
        buffer_reg = buffer_reg.strip('b')
        buffer_desc = {"name": buffer_name, "location": int(buffer_reg)}
        _tp.technique["cbuffers"].append(buffer_desc)
    # io structs from vs.. vs input, instance input, vs output (ps input)
    _tp.technique["vs_inputs"] = generate_input_info(_tp.shader[0].input_decl)
    _tp.technique["instance_inputs"] = generate_input_info(_tp.shader[0].instance_input_decl)
    _tp.technique["vs_outputs"] = generate_input_info(_tp.shader[0].output_decl)
    # vs and ps files
    _tp.technique["vs_file"] = _tp.name + ".vsc"
    _tp.technique["ps_file"] = _tp.name + ".psc"
    _tp.technique["cs_file"] = _tp.name + ".csc"
    # permutation
    _tp.technique["permutations"] = _tp.permutation_options
    _tp.technique["permutation_id"] = _tp.id
    _tp.technique["permutation_option_mask"] = _tp.mask
    return _tp.technique


# parse a pmfx file which is a collection of techniques and permutations, made up of vs, ps, cs combinations
def parse_pmfx(file, root):
    global _info

    # new pmfx info
    _pmfx = pmfx_info()

    file_and_path = os.path.join(root, file)
    shader_file_text, included_files = create_shader_set(file_and_path, root)

    _pmfx.json = find_pmfx_json(shader_file_text)
    _pmfx.source = shader_file_text
    _pmfx.json_text = json.dumps(_pmfx.json)

    # pmfx file may be an include or library module containing only functions
    if not _pmfx.json:
        return

    # check dependencies
    force = False
    up_to_date = check_dependencies(file_and_path, included_files)
    if up_to_date and not force:
        print(file + " file up to date")
        return

    print(file)
    c_code = ""

    pmfx_name = os.path.basename(file).replace(".pmfx", "")

    pmfx_output_info = dict()
    pmfx_output_info["techniques"] = []

    # for techniques in pmfx
    success = True
    for technique in _pmfx.json:
        pmfx_json = json.loads(_pmfx.json_text)
        technique_json = pmfx_json[technique].copy()
        technique_json = inherit_technique(technique_json, pmfx_json)
        technique_permutations, permutation_options, mask, define_list, c_defines = generate_permutations(technique, technique_json)
        c_code += c_defines

        # for permutations in technique
        for permutation in technique_permutations:
            pmfx_json = json.loads(_pmfx.json_text)
            _tp = technique_permutation_info()
            _tp.shader = []
            _tp.cbuffers = []

            # gather technique permutation info
            _tp.id = generate_permutation_id(define_list, permutation)
            _tp.permutation = permutation
            _tp.technique_name = technique
            _tp.technique = inherit_technique(pmfx_json[technique], pmfx_json)
            _tp.mask = mask
            _tp.permutation_options = permutation_options

            valid = True
            shader_types = ["vs", "ps", "cs"]
            for s in shader_types:
                if s in _tp.technique.keys():
                    if _info.shader_platform == "glsl":
                        if s == "cs":
                            print("[warning] compute shaders not implemented for platform: " + _info.shader_platform)
                            valid = False

            if not valid:
                continue

            if _tp.id != 0:
                _tp.name = _tp.technique_name + "__" + str(_tp.id) + "__"
            else:
                _tp.name = _tp.technique_name

            print(_tp.name)

            # strip condition permutations from source
            _tp.source = evaluate_conditional_blocks(_pmfx.source, permutation)

            # get permutation constants..
            _tp.technique = get_permutation_conditionals(_tp.technique, _tp.permutation)

            # global cbuffers
            _tp.cbuffers = find_constant_buffers(_pmfx.source)

            # technique, permutation specific constants
            _tp.technique, c_struct, tp_cbuffer = generate_technique_constant_buffers(pmfx_json, _tp)
            c_code += c_struct

            # add technique / permutation specific cbuffer to the list
            _tp.cbuffers.append(tp_cbuffer)

            # technique, permutation specific textures..
            _tp.textures = generate_technique_texture_variables(_tp)
            _tp.texture_decl = find_texture_samplers(_tp.source)

            # add technique textures
            if _tp.textures:
                _tp.texture_decl += generate_texture_decl(_tp.textures)

            # find functions
            _tp.functions = find_functions(_tp.source)

            # find structs
            struct_list = find_struct_declarations(_tp.source)
            _tp.struct_decls = ""
            for struct in struct_list:
                _tp.struct_decls += struct + "\n"

            # generate single shader data
            shader_types = ["vs", "ps", "cs"]
            for s in shader_types:
                if s in _tp.technique.keys():
                    single_shader = generate_single_shader(_tp.technique[s], _tp)
                    single_shader.shader_type = s
                    if single_shader:
                        _tp.shader.append(single_shader)

            # convert single shader to platform specific variation
            for s in _tp.shader:
                if _info.shader_platform == "hlsl":
                    ss = compile_hlsl(_info, pmfx_name, _tp, s)
                elif _info.shader_platform == "glsl":
                    ss = compile_glsl(_info, pmfx_name, _tp, s)
                elif _info.shader_platform == "metal":
                    ss = compile_metal(_info, pmfx_name, _tp, s)
                else:
                    print("error: invalid shader platform " + _info.shader_platform)

                if ss != 0:
                    print("failed: " + pmfx_name)
                    success = False

            pmfx_output_info["techniques"].append(generate_technique_permutation_info(_tp))

    if not success:
        return

    # write a shader info file with timestamp for dependencies
    generate_shader_info(file_and_path, included_files, pmfx_output_info)

    # write out a c header for accessing materials in code
    if c_code != "":
        h_filename = file.replace(".pmfx", ".h")
        if not os.path.exists("shader_structs"):
            os.mkdir("shader_structs")
        h_filename = os.path.join("shader_structs", h_filename)
        h_file = open(h_filename, "w+")
        h_file.write(c_code)
        h_file.close()


# entry
if __name__ == "__main__":
    print("--------------------------------------------------------------------------------")
    print("pmfx shader compilation (v2)----------------------------------------------------")
    print("--------------------------------------------------------------------------------")

    global _info
    _info = build_info()
    _info.os_platform = util.get_platform_name()
    _info.error_code = 0

    parse_args()

    # pm build config
    config = open("build_config.json")

    if _info.os_platform == "ios" or _info.os_platform == "android":
        _info.shader_sub_platform = "gles"

    # get dirs for build output
    _info.root_dir = os.getcwd()
    _info.build_config = json.loads(config.read())
    _info.pmtech_dir = util.correct_path(_info.build_config["pmtech_dir"])
    _info.tools_dir = os.path.join(_info.pmtech_dir, "tools")
    _info.output_dir = os.path.join(_info.root_dir, "bin", _info.os_platform, "data", "pmfx", _info.shader_platform)
    _info.this_file = os.path.join(_info.root_dir, _info.tools_dir, "build_scripts", "build_pmfx.py")
    _info.macros_file = os.path.join(_info.root_dir, _info.tools_dir, "_shader_macros.h")

    # dirs for build input
    shader_source_dir = os.path.join(_info.root_dir, "assets", "shaders")
    pmtech_shaders = os.path.join(_info.pmtech_dir, "assets", "shaders")

    # global shader macros for glsl, hlsl and metal portability
    mf = open(_info.macros_file)
    _info.macros_source = mf.read()
    mf.close()

    source_list = [pmtech_shaders, shader_source_dir]
    for source_dir in source_list:
        for root, dirs, files in os.walk(source_dir):
            for file in files:
                if file.endswith(".pmfx"):
                    parse_pmfx(file, root)

    # error code for ci
    sys.exit(_info.error_code)
