import helpers
import os
import struct
import sys

schema = "{http://www.collada.org/2005/11/COLLADASchema}"


def grow_extents(v, min, max):
    for i in range(0,3,1):
        if float(v[i]) < min[i]:
            min[i] = float(v[i])
        if float(v[i]) > max[i]:
            max[i] = float(v[i])
    return min, max


def write_geometry(file, root):
    full_path = os.path.join(root, file)
    obj_file = open(full_path)
    obj_data = obj_file.read().split('\n')

    vertex_info = [
        ('v', 0, 3, [1.0]),
        ('vt', 1, 2, [0.0, 0.0]),
        ('vn', 2, 3, [1.0])
    ]

    vertex_data = [[], [], []]

    min_extents = [sys.float_info.max, sys.float_info.max, sys.float_info.max]
    max_extents = [-sys.float_info.max, -sys.float_info.max, -sys.float_info.max]

    for line in obj_data:
        element = line.split(' ')
        for vv in vertex_info:
            if element[0] == 'v':
                grow_extents(element[1:], min_extents, max_extents)
            if vv[0] == element[0]:
                for i in range(0, vv[2]):
                    vertex_data[vv[1]].append(element[1+i])
                for f in vv[3]:
                    vertex_data[vv[1]].append(f)

    geometry_data = [struct.pack("i", (int(helpers.version_number))),
                     struct.pack("i", (int(1)))]

    helpers.pack_parsable_string(geometry_data, "obj_default")

    data_size = len(geometry_data) * 4






