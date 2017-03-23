import os
import subprocess
import struct
import os.path
import shutil
import re

hlsl_key = ["float4",   "float3",   "float2"]
glsl_key = ["vec4",     "vec3",     "vec2"]

tools_dir = os.getcwd() + "\\..\\tools\\"
compiler_dir = os.getcwd() + "\\..\\tools\\bin\\fxc\\"
temp_dir = os.getcwd() + "\\temp\\"

if not os.path.exists(temp_dir):
    os.mkdir(temp_dir)

shader_dir = os.getcwd() + "\\assets\\shaders\\"
build_dir = os.getcwd() + "\\bin\\win32\\data\\shaders\\"

# create shaders dir
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

print("fx compiler directory :" + compiler_dir)
print("compiling directory: " + shader_dir + "\n")

class e_shader_format:
    glsl = 0
    dx11 = 1
    gles = 2

def parse_input_layout(vs_input_source, filename, temp_extension):
    f = os.path.basename(filename)
    f = os.path.splitext(f)[0] + temp_extension
    temp_file_name = temp_dir + f

    temp_shader_source = open(temp_file_name, "w")
    temp_shader_source.write(vs_input_source)
    temp_shader_source.close()

    outfilename = build_dir + f + "i"

    text = open(temp_file_name)
    stri = text.read()
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
        loc = s.find("float")
        if loc >= 0 and (count % 3) == 0:
            cur_size = int(s[loc+5]) * 4
            output.write(struct.pack("i", int(cur_size)))
            output.write(struct.pack("i", int(offset)))
            offset += cur_size
        semantic_lookup_index = 0
        semantics = ["POSITION", "TEXCOORD", "COLOR"]
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
    temp_file_name = temp_dir + f

    temp_shader_source = open(temp_file_name, "w")
    temp_shader_source.write(source)
    temp_shader_source.close()

    cmdline = compiler_dir + "fxc " + "/T " + shader_model + " " + "/Fo " + build_dir + f + "c " + temp_file_name
    subprocess.call(cmdline)
    print("\n")

def find_struct(shader_text, decl):
    start = shader_text.find(decl)
    end = shader_text.find("};", start)
    end += 2
    return shader_text[start:end] + "\n\n"

def find_main(shader_text, decl):
    start = shader_text.find(decl)
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
    struct_source += "}\n"
    struct_source += "\n"
    return struct_source

def assign_global_io_struct(io_elements, decl, local_var):
    assign_source = decl + " " + local_var + ";\n"
    for element in io_elements:
        var_name = element.split()[1]
        assign_source += local_var + "." + var_name + " = " + var_name + "_;\n"
    return assign_source

def compile_glsl(vs_shader_source, ps_shader_source, macros):
    glsl_vs_source = vs_shader_source
    glsl_ps_source = ps_shader_source

    # replace all key words
    for key_index in range(0, len(hlsl_key)):
        glsl_vs_source = glsl_vs_source.replace(hlsl_key[key_index], glsl_key[key_index])

    # parse input block
    vs_inputs, vs_input_semantics = parse_io_struct(glsl_vs_source, "struct vs_input")
    vs_outputs, vs_output_semantics = parse_io_struct(glsl_vs_source, "struct vs_output")
    ps_outputs, ps_output_semantics = parse_io_struct(glsl_ps_source, "struct ps_output")

    #start making vs shader code
    final_vs_source = "#version 330 core\n"
    final_vs_source += "#define GLSL\n"
    final_vs_source += macros
    final_vs_source += "\n\n"

    # glsl inputs
    index_counter = 0
    for vs_input in vs_inputs:
        final_vs_source += "layout(location = " + str(index_counter) + ") in " + vs_input + "_;\n"
        index_counter += 1
    final_vs_source += "\n"

    final_vs_source += generate_global_io_struct(vs_inputs, "struct vs_input")
    final_vs_source += generate_global_io_struct(vs_outputs, "struct vs_output")

    glsl_vs_main = find_main(glsl_vs_source, "vs_output main")
    skip_function_start = glsl_vs_main.find("{") + 1
    skip_function_end = glsl_vs_main.find("return")
    glsl_vs_main = glsl_vs_main[skip_function_start:skip_function_end].strip()

    vs_main_pre_assign = assign_global_io_struct(vs_inputs, "vs_input", "input")

    glsl_vs_main = vs_main_pre_assign + glsl_vs_main

    print(glsl_vs_main)

def create_vs_ps_hlsl(filename):
    print("converting: " + filename + "\n")

    macros_file = open(tools_dir + "_shader_macros.h")
    macros_text = macros_file.read()

    shader_file = open(filename, "r")
    shader_file_text = shader_file.read()
    shader_file_text = clean_spaces(shader_file_text)

    vs_source = macros_text + "\n\n"
    ps_source = macros_text + "\n\n"

    vs_input_source = find_struct(shader_file_text, "struct vs_input")
    vs_output_source = find_struct(shader_file_text, "struct vs_output")

    # texture samplers
    texture_samplers_source = find_texture_samplers(shader_file_text)

    # vertex shader
    vs_source += vs_input_source
    vs_source += vs_output_source
    vs_source += texture_samplers_source
    vs_source += find_main(shader_file_text, "vs_output main")

    # pixel shader
    ps_source += vs_output_source
    ps_source += find_struct(shader_file_text, "struct ps_output")
    ps_source += texture_samplers_source
    ps_source += find_main(shader_file_text, "ps_output main")

    shader_format = e_shader_format.glsl

    if shader_format == e_shader_format.dx11:
        compile_hlsl(vs_source, filename, "vs_4_0", ".vs")
        compile_hlsl(ps_source, filename, "ps_4_0", ".ps")
    elif shader_format == e_shader_format.glsl:
        compile_glsl(vs_source, ps_source, macros_text)

    parse_input_layout(vs_input_source, filename, ".vsi")

    debug_print = 0
    if debug_print > 0:
        print("Vertex Shader")
        print(vs_source)
        print("Pixel Shader")
        print(ps_source)

for root, dirs, files in os.walk(shader_dir):
    for file in files:
        if file.endswith(".shp"):
            file_and_path = os.path.join(root, file)
            create_vs_ps_hlsl(file_and_path)




