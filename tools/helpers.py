import struct
import os

version_number = 1
current_filename = ""
author = ""
log_level = "verbose"

platform = "win32"
if os.name == "posix":
    platform = "osx"

build_dir = os.path.join(os.getcwd(), "bin", platform, "data", "models")

#helpers
def log_message(msg):
    if log_level != "silent":
        print(msg)

def write_parsable_string(output, str):
    str = str.lower()
    output.write(struct.pack("i", (len(str))))
    for c in str:
        ascii = int(ord(c))
        output.write(struct.pack("i", (int(ascii))))

def write_split_floats(output, str):
    split_floats = str.split()
    for val in range(len(split_floats)):
        output.write(struct.pack("f", (float(split_floats[val]))))

def write_corrected_4x4matrix(output, matrix_array):
    if author == "Maxypad":
        num_mats = len(matrix_array) / 16
        for m in range(0, int(num_mats), 1):
            index = m * 16
            #xrow
            output.write(struct.pack("f", (float(matrix_array[index+0]))))
            output.write(struct.pack("f", (float(matrix_array[index+1]))))
            output.write(struct.pack("f", (float(matrix_array[index+2]))))
            output.write(struct.pack("f", (float(matrix_array[index+3]))))
            #yrow
            output.write(struct.pack("f", (float(matrix_array[index+8]))))
            output.write(struct.pack("f", (float(matrix_array[index+9]))))
            output.write(struct.pack("f", (float(matrix_array[index+10]))))
            output.write(struct.pack("f", (float(matrix_array[index+11]))))
            #zrow
            output.write(struct.pack("f", (float(matrix_array[index+4]) * -1.0)))
            output.write(struct.pack("f", (float(matrix_array[index+5]) * -1.0)))
            output.write(struct.pack("f", (float(matrix_array[index+6]) * -1.0)))
            output.write(struct.pack("f", (float(matrix_array[index+7]) * -1.0)))
            #wrow
            output.write(struct.pack("f", (float(matrix_array[index+12]))))
            output.write(struct.pack("f", (float(matrix_array[index+13]))))
            output.write(struct.pack("f", (float(matrix_array[index+14]))))
            output.write(struct.pack("f", (float(matrix_array[index+15]))))
    else:
        for f in matrix_array:
            output.write(struct.pack("f", (float(f))))
