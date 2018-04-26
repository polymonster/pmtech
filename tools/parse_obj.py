import helpers
import os
import struct

schema = "{http://www.collada.org/2005/11/COLLADASchema}"


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

    for line in obj_data:
        element = line.split(' ')
        for vv in vertex_info:
            if vv[0] == element[0]:
                for i in range(0, vv[2]):
                    vertex_data[vv[1]].append(element[1+i])
                for f in vv[3]:
                    vertex_data[vv[1]].append(f)

    



