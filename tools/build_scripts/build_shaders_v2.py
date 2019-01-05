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
    shader_platform = ""
    os_platform = ""
    root_dir = ""
    build_config = ""
    pmtech_dir = ""
    tools_dir = ""
    output_dir = ""
    this_file = ""
    macros_file = ""
    macros_source = ""


# info and contents of a .pmfx file
class pmfx_info:
    includes = ""
    json = ""
    source = ""
    constant_buffers = ""
    texture_samplers = ""


# info about a single vs, ps, or cs
class single_shader_info:
    shader_type = ""
    main_func_name = ""
    functions_source = ""
    main_func_source = ""
    input_struct_name = ""
    instance_input_struct_name = ""
    output_struct_name = ""
    input_decl = ""
    instance_input_decl = ""
    output_decl = ""
    struct_decls = ""


# info of pmfx technique permutation
class technique_permutation_info:
    technique_name = ""                                                     # name of technique
    technique = ""                                                          # technique / permutation json
    permutation = ""                                                        # permutation options
    source = ""                                                             # conditioned source code for permute
    id = ""                                                                 # permutation id
    cbuffers = []                                                           # list of cbuffers source code
    functions = []                                                          # list of functions source code
    textures = []                                                           # technique / permutation textures
    shader = []                                                             # list of shaders, vs, ps or cs


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
    constant_info = [["float", 1], ["float2", 2], ["float3", 3], ["float4", 4], ["float4x4", 16]]

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
    permutation_name = ""
    if int(_tp.id) != 0:
        for p in _tp.permutation:
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
    return tp, define_list, define_string


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

    if _si.instance_input_struct_name:
        _si.instance_input_decl, main = strip_empty_inputs(_si.instance_input_decl, main)

    _si.main_func_source = main

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


def compile_hlsl(_info, pmfx_name, _tp, _shader):
    shader_source = _info.macros_source
    shader_source += _tp.struct_decls
    for cb in _tp.cbuffers:
        shader_source += cb
    shader_source += _shader.input_decl
    shader_source += _shader.instance_input_decl
    shader_source += _shader.output_decl
    shader_source += _tp.texture_decl
    shader_source += _shader.functions_source
    shader_source += _shader.main_func_source
    shader_source = format_source(shader_source, 4)

    exe = os.path.join(_info.tools_dir, "bin", "fxc", "fxc")

    shader_model = {
        "vs": "vs_4_0",
        "ps": "ps_4_0"
    }

    extension = {
        "vs": ".vs",
        "ps": ".ps"
    }

    temp_path = os.path.join(_info.root_dir, "temp", pmfx_name)
    output_path = os.path.join(_info.root_dir, "compiled", pmfx_name, "v2")
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

    subprocess.call(cmdline, shell=True)


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
    force = True
    up_to_date = check_dependencies(file_and_path, included_files)
    if up_to_date and not force:
        print(file + " file up to date")
        return

    print(file)
    c_code = ""

    pmfx_name = os.path.basename(file).replace(".pmfx", "")

    # for techniques in pmfx
    for technique in _pmfx.json:
        pmfx_json = json.loads(_pmfx.json_text)
        technique_json = pmfx_json[technique].copy()
        technique_json = inherit_technique(technique_json, pmfx_json)
        technique_permutations, define_list, c_defines = generate_permutation_list(technique, technique_json)
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

            if _tp.id != 0:
                _tp.name = _tp.technique_name + "__" + str(_tp.id) + "__"
            else:
                _tp.name = _tp.technique_name

            print(_tp.name)

            # strip condition permutations from source
            _tp.source = evaluate_conditional_blocks(_pmfx.source, permutation)

            # get permutation constants.. todo textures
            _tp.technique = get_permutation_conditionals(_tp.technique, _tp.permutation)

            # global cbuffers
            _tp.cbuffers = find_constant_buffers(_pmfx.source)

            # technique, permutation specific constants
            _tp.technique, c_struct, tp_cbuffer = generate_technique_constant_buffers(pmfx_json, _tp)
            c_code += c_struct

            # add technique / permutation specific cbuffer to the list
            _tp.cbuffers.append(tp_cbuffer)

            # technique, permutation specific textures.. todo strip unused
            _tp.textures = generate_technique_texture_variables(_tp)
            _tp.texture_decl = find_texture_samplers(_tp.source)

            # add technique textures
            if _tp.textures:
                for alias in _tp.textures:
                    _tp.texture_decl += str(alias[0]) + "( " + str(alias[1]) + ", " + str(alias[2]) + " );\n"

            # find functions todo strip unused
            _tp.functions = find_functions(_tp.source)
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
                compile_hlsl(_info, pmfx_name, _tp, s)


    # write out a c header for accessing materials in code
    if c_code != "":
        h_filename = file.replace(".pmfx", ".h")
        if not os.path.exists("shader_structs_v2"):
            os.mkdir("shader_structs_v2")
        h_filename = os.path.join("shader_structs_v2", h_filename)
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
