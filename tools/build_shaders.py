import os
import subprocess
import struct
import os.path
import re
import sys
import json

hlsl_key = ["float4",   "float3",   "float2",   "float4x4", "float3x3"]
glsl_key = ["vec4",     "vec3",     "vec2",     "mat4",     "mat3"]

tools_dir = os.path.join(os.getcwd(), "..", "tools")
compiler_dir = os.path.join(os.getcwd(), "..", "tools", "bin", "fxc")
temp_dir = os.path.join(os.getcwd(), "temp")

if not os.path.exists(temp_dir):
    os.mkdir(temp_dir)

shader_source_dir = os.path.join(os.getcwd(), "assets", "shaders")

shader_platform = "hlsl"
os_platform = "win32"
if len(sys.argv) > 1:
    shader_platform = sys.argv[1]

if len(sys.argv) > 2:
    os_platform = sys.argv[2]

shader_build_dir = os.path.join(os.getcwd(),"bin", os_platform, "data", "shaders", shader_platform)

# create shaders dir
if not os.path.exists(shader_build_dir):
    os.makedirs(shader_build_dir)

print("fx compiler directory :" + compiler_dir)
print("compiling directory: " + shader_source_dir + "\n")


def parse_and_split_block(code_block):
    start = code_block.find("{") + 1
    end = code_block.find("};")
    block_conditioned = code_block[start:end].replace(";", "")
    block_conditioned = block_conditioned.replace(":", "")
    block_conditioned = block_conditioned.replace("(", "")
    block_conditioned = block_conditioned.replace(")", "")
    block_conditioned = block_conditioned.replace(",", "")
    return block_conditioned.split()

def make_input_info(inputs):
    semantic_info = [
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
                num_elements = int(inputs_split[i].replace(type, ""))
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


def generate_shader_info(filename, included_files, vs_inputs, instance_inputs, texture_samplers, constant_buffers):
    base_filename = os.path.basename(filename)
    dir_path = os.path.dirname(filename)
    info_filename = os.path.splitext(base_filename)[0] + ".json"
    info_filename = os.path.join(shader_build_dir, info_filename)

    shader_info = dict()
    shader_info["files"] = []

    included_files.insert(0, base_filename)
    for file in included_files:
        full_name = os.path.join(dir_path, file)
        modified_time = os.path.getmtime(full_name)
        file_info = {"name": full_name, "timestamp": modified_time}
        shader_info["files"].append(file_info)

    shader_info["vs_inputs"] = make_input_info(vs_inputs)
    shader_info["instance_inputs"] = make_input_info(instance_inputs)

    shader_info["texture_samplers"] = []
    texture_samplers_split = parse_and_split_block(texture_samplers)
    for i in range(0, len(texture_samplers_split), 3):
        sampler_desc = {
            "name": texture_samplers_split[i+1],
            "type": texture_samplers_split[i+0],
            "location": texture_samplers_split[i+2]
        }
        shader_info["texture_samplers"].append(sampler_desc)

    shader_info["cbuffers"] = []
    for buffer in constant_buffers:
        buffer_decl = buffer[0:buffer.find("{")-1]
        buffer_decl_split = buffer_decl.split(":")
        buffer_name = buffer_decl_split[0].split()[1]
        buffer_loc_start = buffer_decl_split[1].find("(") + 1
        buffer_loc_end = buffer_decl_split[1].find(")", buffer_loc_start)
        buffer_reg = buffer_decl_split[1][buffer_loc_start:buffer_loc_end]
        buffer_reg = buffer_reg.strip('b')
        buffer_desc = {"name": buffer_name, "location": buffer_reg}
        shader_info["cbuffers"].append(buffer_desc)

    output_info = open(info_filename, 'wb+')
    output_info.write(bytes(json.dumps(shader_info, indent=4), 'UTF-8'))
    output_info.close()
    return shader_info

def parse_input_layout(vs_input_source, filename, temp_extension):
    f = os.path.basename(filename)
    f = os.path.splitext(f)[0] + temp_extension

    outfilename = os.path.join(shader_build_dir, f)

    stri = vs_input_source
    vs_input_loc = stri.find("vs_input")
    if vs_input_loc < 0:
        return
    vs_input_start = stri.find("{", vs_input_loc) + 1
    vs_input_end = stri.find("}", vs_input_loc)
    input_str = stri[vs_input_start:vs_input_end]
    input_str = input_str.replace(";", "")
    input_str = input_str.replace(":", "")
    splitted = input_str.split()
    num_attribs = int(len(splitted)/3)
    output = open(outfilename, 'wb+')
    output.write(struct.pack("i", (int(num_attribs))))
    count = 0
    offset = 0
    for s in splitted:
        semantic_lookup_index = 0
        semantics = ["POSITION", "TEXCOORD", "COLOR"]
        data_sizes = [4, 4, 1]
        semantic_index = 0
        loc = s.find("float")
        if loc >= 0 and (count % 3) == 0:
            num_elems = int(s[loc+5])
            cur_size = int(s[loc+5]) * data_sizes[semantic_index]
            output.write(struct.pack("i", int(cur_size)))
            output.write(struct.pack("i", int(offset)))
            offset += cur_size
        for semantic_name in semantics:
            loc = s.find(semantic_name)
            if loc >= 0 and (count % 3) == 2:
                output.write(struct.pack("i",int(semantic_lookup_index)))
                semantic_index = s.replace(semantic_name,"")
                if semantic_index == "":
                    output.write(struct.pack("i", int(0)))
                else:
                    output.write(struct.pack("i", int(semantic_index)))
            semantic_lookup_index += 1
        count += 1

def compile_hlsl(source, filename, shader_model, temp_extension):
    f = os.path.basename(filename)
    f = os.path.splitext(f)[0] + temp_extension
    temp_file_name = os.path.join(temp_dir, f)

    output_file_and_path = os.path.join(shader_build_dir, f + "c")
    compiler_exe_path = os.path.join(compiler_dir,"fxc")

    print(output_file_and_path)
    print(temp_file_name)
    print(compiler_dir)

    temp_shader_source = open(temp_file_name, "w")
    temp_shader_source.write(source)
    temp_shader_source.close()

    cmdline = compiler_exe_path + " /T " + shader_model + " /Fo " + output_file_and_path + " " + temp_file_name
    subprocess.call(cmdline)
    print("\n")

def find_struct(shader_text, decl):
    start = shader_text.find(decl)
    end = shader_text.find("};", start)
    end += 2
    if start != -1 and end != -1:
        return shader_text[start:end] + "\n\n"
    else:
        return ""

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

def find_main(shader_text, decl):
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

def find_generic_functions(shader_text):
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

def find_texture_samplers(shader_text):
    start = shader_text.find("declare_texture_samplers")
    if start == -1:
        return "\n"
    start = shader_text.find("{", start) + 1
    end = shader_text.find("};", start)
    texture_sampler_text = shader_text[start:end] + "\n"
    texture_sampler_text = texture_sampler_text.replace("\t", "")
    return texture_sampler_text

def clean_spaces(shader_text):
    return re.sub(' +', ' ', shader_text)

def parse_io_struct(source, decl):
    io_source = find_struct(source, decl)
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
    #the last input will always be "};" pop it out
    elements.pop(len(elements)-1)
    semantics.pop(len(semantics)-1)

    return elements, semantics

def generate_global_io_struct(io_elements, decl):
    # global input struct for hlsl compatibility to access like input.value
    struct_source = decl
    struct_source += "\n{\n"
    for element in io_elements:
        struct_source += "\t" + element + ";\n"
    struct_source += "};\n"
    struct_source += "\n"
    return struct_source

def generate_input_assignment(io_elements, decl, local_var, suffix):
    assign_source = "\t//assign input struct from glsl inputs\n"
    assign_source += "\t" + decl + " " + local_var + ";\n"
    for element in io_elements:
        if element.split()[1] == "position" and decl == "vs_output":
            continue
        assign_source += "\t"
        var_name = element.split()[1]
        assign_source += local_var + "." + var_name + " = " + var_name + suffix + ";\n"
    return assign_source

def generate_output_assignment(io_elements, local_var, suffix):
    assign_source = "\n\t//assign glsl global outputs from structs\n"
    for element in io_elements:
        assign_source += "\t"
        var_name = element.split()[1]
        if var_name == "position":
           assign_source += "gl_Position = " + local_var + "." + var_name + ";\n"
        else:
            assign_source += var_name + suffix + " = " + local_var + "." + var_name + ";\n"
    return assign_source

def compile_glsl(vs_shader_source, ps_shader_source, has_ps,
                 source_filename, texture_samplers_source, macros):
    shader_name = os.path.basename(source_filename)
    shader_name = os.path.splitext(shader_name)[0]

    glsl_vs_source = vs_shader_source
    glsl_ps_source = ps_shader_source

    # replace all key words
    for key_index in range(0, len(hlsl_key)):
        glsl_vs_source = glsl_vs_source.replace(hlsl_key[key_index], glsl_key[key_index])
        glsl_ps_source = glsl_ps_source.replace(hlsl_key[key_index], glsl_key[key_index])

    # parse input block
    vs_inputs, vs_input_semantics = parse_io_struct(glsl_vs_source, "struct vs_input")
    vs_outputs, vs_output_semantics = parse_io_struct(glsl_vs_source, "struct vs_output")

    # start making vs shader code
    final_vs_source = "#version 330 core\n"
    final_vs_source += "#define GLSL\n"
    final_vs_source += macros
    final_vs_source += "\n\n"

    # glsl inputs
    index_counter = 0
    for vs_input in vs_inputs:
        final_vs_source += "layout(location = " + str(index_counter) + ") in " + vs_input + "_vs_input;\n"
        index_counter += 1
    final_vs_source += "\n"

    # vs outputs
    for vs_output in vs_outputs:
        if vs_output.split()[1] != "position":
            final_vs_source += "out " + vs_output + "_vs_output;\n"
    final_vs_source += "\n"

    final_vs_source += generate_global_io_struct(vs_inputs, "struct vs_input")
    final_vs_source += generate_global_io_struct(vs_outputs, "struct vs_output")
    final_vs_source += texture_samplers_source

    glsl_vs_main = find_main(glsl_vs_source, "vs_output main")
    skip_function_start = glsl_vs_main.find("{") + 1
    skip_function_end = glsl_vs_main.find("return")
    glsl_vs_main = glsl_vs_main[skip_function_start:skip_function_end].strip()

    vs_main_pre_assign = generate_input_assignment(vs_inputs, "vs_input", "_input", "_vs_input")
    vs_main_post_assign = generate_output_assignment(vs_outputs, "_output", "_vs_output")

    final_vs_source += "void main()\n{\n"
    final_vs_source += vs_main_pre_assign
    final_vs_source += "\n\t//main body from " + source_filename + "\n"
    final_vs_source += "\t" + glsl_vs_main + "\n"
    final_vs_source += vs_main_post_assign
    final_vs_source += "}\n"

    vs_fn = os.path.join(shader_build_dir, shader_name + ".vsc")
    vs_file = open(vs_fn, "w")
    vs_file.write(final_vs_source)
    vs_file.close()

    # start making ps shader code
    if has_ps:
        ps_outputs, ps_output_semantics = parse_io_struct(glsl_ps_source, "struct ps_output")

        final_ps_source = "#version 330 core\n"
        final_ps_source += "#define GLSL\n"
        final_ps_source += macros
        final_ps_source += "\n\n"

        # ps inputs
        for vs_output in vs_outputs:
            if vs_output.split()[1] != "position":
                final_ps_source += "in " + vs_output + "_vs_output;\n"
        final_ps_source += "\n"

        # ps outputs
        for ps_output in ps_outputs:
            final_ps_source += "out " + ps_output + "_ps_output;\n"
        final_ps_source += "\n"

        final_ps_source += generate_global_io_struct(vs_outputs, "struct vs_output")
        final_ps_source += generate_global_io_struct(ps_outputs, "struct ps_output")
        final_ps_source += texture_samplers_source

        glsl_ps_main = find_main(glsl_ps_source, "ps_output main")
        skip_function_start = glsl_ps_main.find("{") + 1
        skip_function_end = glsl_ps_main.find("return")
        glsl_ps_main = glsl_ps_main[skip_function_start:skip_function_end].strip()

        ps_main_pre_assign = generate_input_assignment(vs_outputs, "vs_output", "_input", "_vs_output")
        ps_main_post_assign = generate_output_assignment(ps_outputs, "_output", "_ps_output")

        final_ps_source += "void main()\n{\n"
        final_ps_source += ps_main_pre_assign
        final_ps_source += "\n\t//main body from " + source_filename + "\n"
        final_ps_source += "\t" + glsl_ps_main + "\n"
        final_ps_source += ps_main_post_assign
        final_ps_source += "}\n"

        ps_fn = os.path.join(shader_build_dir, shader_name + ".psc")
        ps_file = open(ps_fn, "w")
        ps_file.write(final_ps_source)
        ps_file.close()

def find_includes(file_text):
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
        include_list.append(file_text[start:end])
    return include_list

def find_used_functions(entry_func, function_list):
    used_functions = [entry_func]
    ordered_function_list = [entry_func]
    for used_func in used_functions:
        for func in function_list:
            if func == used_func:
                continue
            name = func.split(" ")[1]
            end = name.find("(")
            name = name[0:end]
            if used_func.find(name + "(") != -1:
                used_functions.append(func)
                ordered_function_list.insert(0, func)
    used_function_source = ""
    for used_func in ordered_function_list:
        used_function_source += used_func + "\n\n"
    return used_function_source

def add_files_recursive(filename, root):
    file_path = os.path.join(root, filename)
    included_file = open(file_path, "r")
    shader_source = included_file.read()
    shader_source = clean_spaces(shader_source)
    include_list = find_includes(shader_source)
    for slib in include_list:
        included_source, sub_includes = add_files_recursive(slib, root)
        shader_source = included_source + shader_source
        include_list = include_list + sub_includes
    return shader_source, include_list

def create_vsc_psc_vsi(filename, root):
    print("converting: " + filename + "\n")

    macros_fn = os.path.join(tools_dir, "_shader_macros.h")
    macros_file = open(macros_fn)
    macros_text = macros_file.read()

    shader_file_text, included_files = add_files_recursive(filename, root)

    function_list = find_generic_functions(shader_file_text)

    #_find main ps and vs
    main_ps = ""
    main_vs = ""
    has_ps = False
    for func in function_list:
        if func.find("ps_output main") != -1:
            main_ps = func
            has_ps = True
        if func.find("vs_output main") != -1:
            main_vs = func

    # remove from generic function list
    function_list.remove(main_vs)
    if has_ps:
        function_list.remove(main_ps)

    vs_functions = ""
    vs_functions += find_used_functions(main_vs, function_list)

    ps_functions = ""
    if has_ps:
        ps_functions = find_used_functions(main_ps, function_list)

    vs_source = macros_text + "\n\n"
    ps_source = macros_text + "\n\n"

    instance_input_source = find_struct(shader_file_text, "struct instance_input")
    vs_input_source = find_struct(shader_file_text, "struct vs_input")
    vs_output_source = find_struct(shader_file_text, "struct vs_output")

    #constant / uniform buffers
    constant_buffers = find_constant_buffers(shader_file_text)

    # texture samplers
    texture_samplers_source = find_texture_samplers(shader_file_text)

    # vertex shader
    vs_source += vs_input_source
    vs_source += vs_output_source
    for cbuf in constant_buffers:
        vs_source += cbuf
    vs_source += texture_samplers_source
    vs_source += vs_functions

    # pixel shader
    ps_source += vs_output_source
    for cbuf in constant_buffers:
        ps_source += cbuf
    ps_source += find_struct(shader_file_text, "struct ps_output")
    ps_source += texture_samplers_source

    # allow null pixel shaders
    if has_ps:
        ps_source += ps_functions

    if shader_platform == "hlsl":
        compile_hlsl(vs_source, filename, "vs_4_0", ".vs")
        if has_ps:
            compile_hlsl(ps_source, filename, "ps_4_0", ".ps")
    elif shader_platform == "glsl":
        compile_glsl(vs_source,
                     ps_source,
                     has_ps,
                     filename,
                     texture_samplers_source,
                     macros_text)

    generate_shader_info(
        filename,
        included_files,
        vs_input_source,
        instance_input_source,
        texture_samplers_source,
        constant_buffers)

    parse_input_layout(vs_input_source, filename, ".vsi")

    debug_print = 0
    if debug_print > 0:
        print("Vertex Shader")
        print(vs_source)
        print("Pixel Shader")
        print(ps_source)

for root, dirs, files in os.walk(shader_source_dir):
    for file in files:
        if file.endswith(".shp"):
            file_and_path = os.path.join(root, file)
            create_vsc_psc_vsi(file_and_path, root)




