import struct
import os
import sys

version_number = 1
anim_version_number = 1
current_filename = ""
author = ""
log_level = "verbose"

platform = "win32"
if os.name == "posix":
    platform = "osx"

build_dir = os.path.join(os.getcwd(), "bin", platform, "data", "models")


class pmm_file:
    geometry = []
    geometry_names = []
    geometry_sizes = []

    materials = []
    material_names = []

    scene = []
    joints = []

    def __init__(self):
        self.geometry = []
        self.geometry_names = []
        self.materials = []
        self.material_names = []
        self.scene = []
        self.joints = []

    def write(self, filename):
        print("writing: " + filename)
        output = open(filename, 'wb+')

        num_geom = len(self.geometry)
        num_material = len(self.materials)
        num_scene = len(self.scene)
        num_joints = len(self.joints)

        output.write(struct.pack("i", num_scene))
        output.write(struct.pack("i", num_geom))
        output.write(struct.pack("i", num_material))

        offset = 0
        for s in self.scene:
            output.write(struct.pack("i", offset))
            offset += len(s) * 4

        for i in range(0, len(self.material_names)):
            print(self.material_names[i])
            write_parsable_string(output, self.material_names[i])
            output.write(struct.pack("i", offset))
            offset += len(self.materials[i]) * 4

        for i in range(0, len(self.geometry_names)):
            write_parsable_string(output, self.geometry_names[i])
            output.write(struct.pack("i", offset))
            offset += self.geometry_sizes[i]

        for s in self.scene:
            for b in s:
                output.write(b)

        for m in self.materials:
            for b in m:
                output.write(b)

        for g in self.geometry:
            for b in g:
                output.write(b)

        output.close()


output_file = pmm_file()


# helpers
def log_message(msg):
    if log_level != "silent":
        print(msg)


def write_parsable_string(output, str):
    str = str.lower()
    output.write(struct.pack("i", (len(str))))
    for c in str:
        ascii = int(ord(c))
        output.write(struct.pack("i", (int(ascii))))


def pack_parsable_string(output, str):
    str = str.lower()
    output.append(struct.pack("i", (len(str))))
    for c in str:
        ascii = int(ord(c))
        output.append(struct.pack("i", (int(ascii))))


def write_split_floats(output, str):
    split_floats = str.split()
    for val in range(len(split_floats)):
        output.write(struct.pack("f", (float(split_floats[val]))))


def pack_split_floats(output, str):
    split_floats = str.split()
    for val in range(len(split_floats)):
        output.append(struct.pack("f", (float(split_floats[val]))))


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


def pack_corrected_4x4matrix(output, matrix_array):
    if author == "Maxypad":
        num_mats = len(matrix_array) / 16
        for m in range(0, int(num_mats), 1):
            index = m * 16
            # x row
            output.append(struct.pack("f", (float(matrix_array[index+0]))))
            output.append(struct.pack("f", (float(matrix_array[index+1]))))
            output.append(struct.pack("f", (float(matrix_array[index+2]))))
            output.append(struct.pack("f", (float(matrix_array[index+3]))))
            # y row
            output.append(struct.pack("f", (float(matrix_array[index+8]))))
            output.append(struct.pack("f", (float(matrix_array[index+9]))))
            output.append(struct.pack("f", (float(matrix_array[index+10]))))
            output.append(struct.pack("f", (float(matrix_array[index+11]))))
            # z row
            output.append(struct.pack("f", (float(matrix_array[index+4]) * -1.0)))
            output.append(struct.pack("f", (float(matrix_array[index+5]) * -1.0)))
            output.append(struct.pack("f", (float(matrix_array[index+6]) * -1.0)))
            output.append(struct.pack("f", (float(matrix_array[index+7]) * -1.0)))
            # w row
            output.append(struct.pack("f", (float(matrix_array[index+12]))))
            output.append(struct.pack("f", (float(matrix_array[index+13]))))
            output.append(struct.pack("f", (float(matrix_array[index+14]))))
            output.append(struct.pack("f", (float(matrix_array[index+15]))))
    else:
        for f in matrix_array:
            output.append(struct.pack("f", (float(f))))


def correct_4x4matrix(matrix_string):
    corrected = []
    matrix_array = matrix_string.split()
    if author == "Maxypad":
        num_mats = len(matrix_array) / 16
        for m in range(0, int(num_mats), 1):
            index = m * 16
            # x row
            corrected.append(float(matrix_array[index+0]))
            corrected.append(float(matrix_array[index+1]))
            corrected.append(float(matrix_array[index+2]))
            corrected.append(float(matrix_array[index+3]))
            # y row
            corrected.append(float(matrix_array[index+8]))
            corrected.append(float(matrix_array[index+9]))
            corrected.append(float(matrix_array[index+10]))
            corrected.append(float(matrix_array[index+11]))
            # z row
            corrected.append(float(matrix_array[index+4]) * -1.0)
            corrected.append(float(matrix_array[index+5]) * -1.0)
            corrected.append(float(matrix_array[index+6]) * -1.0)
            corrected.append(float(matrix_array[index+7]) * -1.0)
            # w row
            corrected.append(float(matrix_array[index+12]))
            corrected.append(float(matrix_array[index+13]))
            corrected.append(float(matrix_array[index+14]))
            corrected.append(float(matrix_array[index+15]))

    out_str = ""
    for m in matrix_array:
        out_str += m + " "

    return out_str
