import os
import subprocess
import struct

# win32 / d3d / hlsl
print("hlsl shader compiler" + "\n")

# compiler_dir = os.environ.get("PEN_DIR") + "third_party\\FXC\\"

compiler_dir = "C:\\Program Files (x86)\\Windows Kits\\8.0\\bin\\x86\\"

print(compiler_dir)

shader_dir = os.getcwd() + "\\assets\\shaders\\"
build_dir = os.getcwd() + "\\bin\\win32\\data\\shaders\\"

#create shaders dir
if not os.path.exists(build_dir):
    os.makedirs(build_dir)

print("fx compiler directory :" + compiler_dir )

print("compiling directory: " + shader_dir + "\n")


def parse_input_layout(filename,outfilename):
    text = open(filename)
    stri = text.read()
    vs_input_loc = stri.find("vs_input")

    if vs_input_loc < 0:
        return

    print("input_layout")

    vs_input_start = stri.find("{",vs_input_loc) + 1
    vs_input_end = stri.find("}",vs_input_loc)
    input_str = stri[vs_input_start:vs_input_end]
    input_str = input_str.replace(";","")
    input_str = input_str.replace(":","")
    splitted = input_str.split( )
    num_attribs = int(len(splitted)/3)
    output = open(outfilename,'wb+')
    output.write(struct.pack("i",(int(num_attribs))))
    count = 0
    offset = 0
    for s in splitted:
        loc = s.find("float")
        if loc >= 0 and (count % 3) == 0:
            cur_size = int(s[loc+5]) * 4
            output.write(struct.pack("i",int(cur_size)))
            output.write(struct.pack("i",int(offset)))
            offset+=cur_size

        semantic_lookup_index = 0
        semantics = ["POSITION","TEXCOORD","COLOR"]
        for semantic_name in semantics:
            loc = s.find(semantic_name)
            if loc >= 0 and (count % 3) == 2:
                output.write(struct.pack("i",int(semantic_lookup_index)))
                semantic_index = s.replace(semantic_name,"")
                if semantic_index == "":
                    output.write(struct.pack("i",int(0)))
                else:
                    output.write(struct.pack("i",int(semantic_index)))
            semantic_lookup_index = semantic_lookup_index+1

        count = count + 1

for f in os.listdir(shader_dir):

    print(f)
    if f.find(".vs") != -1:
        print("vertex_shader")
        cmdline = compiler_dir + "fxc " + "/T vs_4_0 " + "/Fo " + build_dir + f + "c " + shader_dir + f
        parse_input_layout(shader_dir + f, build_dir + f + "i" )
    if f.find(".ps") != -1:
        print("pixel_shader")
        cmdline = compiler_dir + "fxc " + "/T ps_4_0 " + "/Fo " + build_dir + f + "c " + shader_dir + f
    subprocess.call(cmdline)
    print("\n")

print("press any key to continue")
i = input()

