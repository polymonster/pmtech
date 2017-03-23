import os
import subprocess
import struct
import os.path
import shutil
import re

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

def parse_input_layout(filename, temp_extension):
    f = os.path.basename(filename)
    f = os.path.splitext(f)[0] + temp_extension
    temp_file_name = temp_dir + f

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

    temp_ps = open(temp_file_name, "w")
    temp_ps.write(source)
    temp_ps.close()

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
    return shader_text[start:end] + "\n"

def clean_spaces(shader_text):
    return re.sub(' +', ' ', shader_text)

def create_vs_ps_hlsl(filename):
    print("converting: " + filename + "\n")

    macros_file = open(tools_dir + "_shader_macros.h")
    macros_text = macros_file.read()

    shader_file = open(filename, "r")
    shader_file_text = shader_file.read()
    shader_file_text = clean_spaces(shader_file_text)

    vs_source = macros_text + "\n\n"
    ps_source = macros_text + "\n\n"

    # texture sampler
    texture_samplers_source = find_texture_samplers(shader_file_text)

    # vertex shader
    vs_source += find_struct(shader_file_text, "struct vs_input")
    vs_source += find_struct(shader_file_text, "struct vs_output")
    vs_source += texture_samplers_source
    vs_source += find_main(shader_file_text, "vs_output main")

    # pixel shader
    ps_source += find_struct(shader_file_text, "struct vs_output")
    ps_source += find_struct(shader_file_text, "struct ps_output")
    ps_source += texture_samplers_source
    ps_source += find_main(shader_file_text, "ps_output main")

    # texture samplers

    compile_hlsl(vs_source, filename, "vs_4_0", ".vs")
    compile_hlsl(ps_source, filename, "ps_4_0", ".ps")

    parse_input_layout(filename, ".vs")

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




