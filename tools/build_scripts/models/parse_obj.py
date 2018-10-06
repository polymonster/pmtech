import models.helpers as helpers
import os
import struct
import sys
import math

schema = "{http://www.collada.org/2005/11/COLLADASchema}"


def calc_normals(vb):
    print("Generating Normals")
    # without vector math library to reduce dependencies
    x = 0
    y = 1
    z = 2
    num_vb_floats = 20
    for tri in range(0, len(vb), num_vb_floats*3):
        p = []
        for vert in range(0, 3):
            vi = tri + num_vb_floats * vert
            p.append([vb[vi + 0], vb[vi + 1], vb[vi + 2]])

        # edges
        v1 = [p[1][x] - p[0][x], p[1][y] - p[0][y], p[1][z] - p[0][z]]
        v2 = [p[2][x] - p[0][x], p[2][y] - p[0][y], p[2][z] - p[0][z]]

        # normalise vectors
        v1_mag = math.sqrt(v1[x] * v1[x] + v1[y] * v1[y] + v1[z] * v1[z])
        v2_mag = math.sqrt(v2[x] * v2[x] + v2[y] * v2[y] + v2[z] * v2[z])

        for i in range(0, 3):
            v1[i] /= v1_mag
            v2[i] /= v2_mag

        # cross product
        nx = v1[y] * v2[z] - v2[y] * v1[z]
        ny = v1[x] * v2[z] - v2[x] * v1[z]
        nz = v1[x] * v2[y] - v2[x] * v1[y]

        # normalise
        nmag = math.sqrt(nx * nx + ny * ny + nz * nz)

        for vert in range(0, 3):
            ni = tri + 4 + (num_vb_floats * vert)
            vb[ni + 0] = nx / nmag
            vb[ni + 1] = ny / nmag
            vb[ni + 2] = nz / nmag

    return vb


def grow_extents(v, min, max):
    for i in range(0,3,1):
        if float(v[i]) < min[i]:
            min[i] = float(v[i])
        if float(v[i]) > max[i]:
            max[i] = float(v[i])
    return min, max


def write_geometry(file, root):
    basename = os.path.basename(file)
    full_path = os.path.join(root, file)
    obj_file = open(full_path)
    obj_data = obj_file.read().split('\n')

    vertex_info = [
        ('v', 0, 3, [1.0]),
        ('vn', 1, 3, [1.0]),
        ('vt', 2, 2, [0.0, 0.0]),
    ]

    vertex_data = [[], [], []]

    min_extents = [sys.float_info.max, sys.float_info.max, sys.float_info.max]
    max_extents = [-sys.float_info.max, -sys.float_info.max, -sys.float_info.max]

    mat_name = 1
    vb = 2
    ib = 3
    pb = 4
    cb = 5

    meshes = []
    cur_mesh = None
    index = 0
    flip_winding = False

    for line in obj_data:
        if line.find("pmtech_flip_winding") != -1:
            flip_winding = True
        element = line.split(' ')
        for vv in vertex_info:
            if element[0] == 'v':
                grow_extents(element[1:], min_extents, max_extents)
            if vv[0] == element[0]:
                vec = []
                for i in range(0, vv[2]):
                    vec.append(element[1+i])
                for f in vv[3]:
                    vec.append(f)
                vertex_data[vv[1]].append(vec)
        if element[0] == 'f':
            face_list = element[1:]
            if flip_winding:
                face_list.reverse()
            if not cur_mesh:
                cur_mesh = (basename, "obj_default", [], [], [], [])
            tri_list = []
            if len(face_list) == 4:
                tri_list.append(face_list[0])
                tri_list.append(face_list[1])
                tri_list.append(face_list[2])
                tri_list.append(face_list[2])
                tri_list.append(face_list[3])
                tri_list.append(face_list[0])
            elif len(face_list) == 3:
                tri_list.append(face_list[0])
                tri_list.append(face_list[1])
                tri_list.append(face_list[2])
            for trivert in tri_list:
                elem_indices = [0]
                if trivert.find("//") != -1:
                    velems = trivert.split("//")
                    elem_indices.append(1)
                else:
                    velems = trivert.split("/")
                    # elem_indices.append(1)
                    elem_indices.append(2)
                vertex = [[0.0, 0.0, 0.0, 1.0],     # pos
                          [0.0, 1.0, 0.0, 1.0],     # normal
                          [0.0, 0.0, 0.0, 1.0],     # texcoord
                          [1.0, 0.0, 0.0, 1.0],     # tangent
                          [0.0, 0.0, 1.0, 1.0]]     # bitangent
                elem_index = 0
                for vi in velems:
                    ii = elem_indices[elem_index]
                    li = int(vi)-1
                    if li < len(vertex_data[ii]):
                        vertex[ii] = vertex_data[ii][li]
                        for vf in vertex_data[ii][int(vi)-1]:
                            if elem_index == 0:
                                cur_mesh[pb].append(float(vf))
                                cur_mesh[cb].append(float(vf))
                    elem_index += 1
                for v in vertex:
                    for f in v:
                        cur_mesh[vb].append(float(f))
                cur_mesh[ib].append(index)
                index += 1
        if element[0] == 'g':
            if cur_mesh:
                meshes.append(cur_mesh)
            cur_mesh = (element[1], "", [], [], [], [])
            index = 0
        if element[0] == 'usemtl':
            if not cur_mesh:
                cur_mesh = (basename, element[0], [], [], [], [])
            else:
                cur_mesh[mat_name] = element[0]

    if cur_mesh:
        meshes.append(cur_mesh)

    geometry_data = [struct.pack("i", (int(helpers.version_number))),
                     struct.pack("i", (int(len(meshes))))]

    for m in meshes:
        helpers.pack_parsable_string(geometry_data, m[0])

    data_size = len(geometry_data) * 4

    for mesh in meshes:
        generated_vb = mesh[vb]
        if len(vertex_data[1]) == 0:
            generated_vb = calc_normals(generated_vb)

        mesh_data = []

        # write min / max extents
        for i in range(0, 3, 1):
            mesh_data.append(struct.pack("f", (float(min_extents[i]))))
        for i in range(0, 3, 1):
            mesh_data.append(struct.pack("f", (float(max_extents[i]))))

        # write vb and ib
        mesh_data.append(struct.pack("i", (len(mesh[pb]))))
        mesh_data.append(struct.pack("i", (len(generated_vb))))
        mesh_data.append(struct.pack("i", (len(mesh[ib]))))
        mesh_data.append(struct.pack("i", (len(mesh[cb]))))

        index_type = "i"
        if len(mesh[ib]) < 65535:
            index_type = "H"

        # obj always non-skinned
        mesh_data.append(struct.pack("i", int(0)))

        for vf in mesh[pb]:
            # position only buffer
            mesh_data.append(struct.pack("f", (float(vf))))
        for vf in generated_vb:
            mesh_data.append(struct.pack("f", (float(vf))))

        data_size += len(mesh_data)*4

        for index in mesh[ib]:
            mesh_data.append(struct.pack(index_type, (int(index))))

        data_size += len(mesh[ib]) * 2

        for vf in mesh[cb]:
            mesh_data.append(struct.pack("f", (float(vf))))

        data_size += len(mesh[cb]) * 4

        for m in mesh_data:
            geometry_data.append(m)

    helpers.output_file.geometry_names.append(mesh[0])
    helpers.output_file.geometry.append(geometry_data)
    helpers.output_file.geometry_sizes.append(data_size)

    scene_data = [struct.pack("i", (int(helpers.version_number))),
                  struct.pack("i", (int(len(meshes))))]

    for m in meshes:
        scene_data.append(struct.pack("i", (int(0)))) # node type
        helpers.pack_parsable_string(scene_data, basename)
        helpers.pack_parsable_string(scene_data, m[0])
        scene_data.append(struct.pack("i", int(1))) # num sub meshes

        # material name / symbol
        helpers.pack_parsable_string(scene_data, m[mat_name])
        helpers.pack_parsable_string(scene_data, m[mat_name])

        scene_data.append(struct.pack("i", (int(0)))) # parent
        scene_data.append(struct.pack("i", (int(1)))) # transform count
        scene_data.append(struct.pack("i", (int(3)))) # transform type

    helpers.output_file.scene.append(scene_data)






