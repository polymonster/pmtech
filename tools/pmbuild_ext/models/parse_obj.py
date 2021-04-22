import models.helpers as helpers
import os
import struct
import sys
import math

schema = "{http://www.collada.org/2005/11/COLLADASchema}"
x = 0
y = 1
z = 2

def normalise(v):
    m = 0
    for i in range(0, len(v)):
        m += v[i] * v[i]
    mag = math.sqrt(m)
    vr = []
    for i in range(0, len(v)):
        if mag == 0.0:
            vr.append(0.0)
        else:
            vr.append(v[i] / mag)
    return vr


def subtract(v1, v2):
    vr = []
    for i in range(0, len(v1)):
        vr.append(v1[i] - v2[i])
    return vr


def multiply_scalar(v1, s):
    vr = []
    for i in range(0, len(v1)):
        vr.append(v1[i] * s)
    return vr


def dot(v1, v2):
    d = 0.0
    for i in range(0, len(v1)):
        d += v1[i] * v2[i]
    return d


def cross(v1, v2):
    cx = v1[y] * v2[z] - v2[y] * v1[z]
    cy = v1[x] * v2[z] - v2[x] * v1[z]
    cz = v1[x] * v2[y] - v2[x] * v1[y]
    return [cx, cy, cz]


def calc_tangents(vb):
    num_vb_floats = 20
    for tri in range(0, len(vb), num_vb_floats*3):
        p = []
        uv = []
        n = []
        for vert in range(0, 3):
            vi = tri + num_vb_floats * vert
            p.append([vb[vi + 0], vb[vi + 1], vb[vi + 2]])
            uvi = vi + 8
            uv.append([vb[uvi + 0], vb[uvi + 1], vb[uvi + 2]])
            ni = vi + 4
            n.append([vb[ni + 0], vb[ni + 1], vb[ni + 2]])

        v1 = normalise(subtract(p[1], p[0]))
        v2 = normalise(subtract(p[2], p[0]))

        w1 = normalise(subtract(uv[1], uv[0]))
        w2 = normalise(subtract(uv[2], uv[0]))

        c = w1[x] * w2[y] - w2[x] * w1[y]
        if c == 0.0:
            r = 0.0
        else:
            r = 1.0 / c

        sdir = [(w2[y] * v1[x] - w1[y] * v2[x]) * r,
                (w2[y] * v1[y] - w1[y] * v2[y]) * r,
                (w2[y] * v1[z] - w1[y] * v2[z]) * r]

        tdir = [(w1[x] * v2[x] - w2[x] * v1[x]) * r,
                (w1[x] * v2[y] - w2[x] * v1[y]) * r,
                (w1[x] * v2[z] - w2[x] * v1[z]) * r]

        # orthogonalise
        t = normalise(subtract(sdir, multiply_scalar(n[0], dot(n[0], sdir))))
        b = normalise(subtract(tdir, multiply_scalar(n[0], dot(n[0], tdir))))

        for vert in range(0, 3):
            ti = tri + 12 + (num_vb_floats * vert)
            bi = tri + 16 + (num_vb_floats * vert)
            for i in range(0, 3):
                vb[ti + i] = t[i]
                vb[bi + i] = b[i]

    return vb


def calc_normals(vb):
    # without vector math library to reduce dependencies

    num_vb_floats = 20
    for tri in range(0, len(vb), num_vb_floats*3):
        p = []
        uv = []
        for vert in range(0, 3):
            vi = tri + num_vb_floats * vert
            p.append([vb[vi + 0], vb[vi + 1], vb[vi + 2]])

        v1 = normalise(subtract(p[1], p[0]))
        v2 = normalise(subtract(p[2], p[0]))

        n = normalise(cross(v1, v2))

        # flip handedness
        n[x] *= -1.0
        n[z] *= -1.0

        for vert in range(0, 3):
            ni = tri + 4 + (num_vb_floats * vert)
            vb[ni + 0] = n[x]
            vb[ni + 1] = n[y]
            vb[ni + 2] = n[z]

    return vb


def grow_extents(v, min, max):
    for i in range(0, 3, 1):
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

    meshes = []
    cur_mesh = None
    index = 0
    flip_winding = False

    for line in obj_data:
        if line.find("pmtech_flip_winding") != -1:
            flip_winding = True
        element_raw = line.split(' ')
        element = []
        # strip dead elems
        for e in element_raw:
            if e != '':
                element.append(e)
        if len(element) == 0:
            continue
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
                    elem_indices.append(2)
                    elem_indices.append(1)
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
            generated_vb = calc_tangents(generated_vb)

        mesh_data = []
        skinned = 0
        num_joint_floats = 0
        bind_shape_matrix = [
            1.0, 0.0, 0.0, 0.0,
            0.0, 1.0, 1.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0
        ]

        num_verts = len(mesh[pb])/4
        index_type = "i"
        index_size = 4
        if num_verts < 65535:
            index_type = "H"
            index_size = 2

        # write min / max extents
        for i in range(0, 3, 1):
            mesh_data.append(struct.pack("f", (float(min_extents[i]))))
        for i in range(0, 3, 1):
            mesh_data.append(struct.pack("f", (float(max_extents[i]))))

        # write vb and ib
        handedness = 0
        if flip_winding:
            handedness = 1
        mesh_data.append(struct.pack("i", int(handedness)))
        mesh_data.append(struct.pack("i", int(num_verts)))   # num pos verts
        mesh_data.append(struct.pack("i", int(index_size)))  # pos index size
        mesh_data.append(struct.pack("i", (len(mesh[ib]))))  # pos buffer index count
        mesh_data.append(struct.pack("i", int(num_verts)))   # num vb verts
        mesh_data.append(struct.pack("i", int(index_size)))  # vertex index size
        mesh_data.append(struct.pack("i", (len(mesh[ib]))))  # vertex buffer index count
        # skinning is not supported in obj, but write any fixed length data anyway
        mesh_data.append(struct.pack("i", int(skinned)))
        mesh_data.append(struct.pack("i", int(num_joint_floats)))
        mesh_data.append(struct.pack("i", int(0))) # bone offset
        helpers.pack_corrected_4x4matrix(mesh_data, bind_shape_matrix)

        for vf in mesh[pb]:
            # position only buffer
            mesh_data.append(struct.pack("f", (float(vf))))
        for vf in generated_vb:
            mesh_data.append(struct.pack("f", (float(vf))))
        data_size += len(mesh_data)*4

        # write index buffer twice, at this point they match, but after optimisation
        # the number of indices in position only vs vertex buffer may changes
        for index in mesh[ib]:
            mesh_data.append(struct.pack(index_type, (int(index))))
        data_size += len(mesh[ib]) * 2
        for index in mesh[ib]:
            mesh_data.append(struct.pack(index_type, (int(index))))
        data_size += len(mesh[ib]) * 2

        for m in mesh_data:
            geometry_data.append(m)

    helpers.output_file.geometry_names.append(mesh[0])
    helpers.output_file.geometry.append(geometry_data)
    helpers.output_file.geometry_sizes.append(data_size)

    scene_data = [struct.pack("i", int(helpers.version_number)),
                  struct.pack("i", int(len(meshes))),
                  struct.pack("i", int(0))]

    for m in meshes:
        scene_data.append(struct.pack("i", (int(0))))  # node type
        helpers.pack_parsable_string(scene_data, basename)
        helpers.pack_parsable_string(scene_data, m[0])
        scene_data.append(struct.pack("i", int(1)))  # num sub meshes

        # material name / symbol
        helpers.pack_parsable_string(scene_data, m[mat_name])
        helpers.pack_parsable_string(scene_data, m[mat_name])

        scene_data.append(struct.pack("i", (int(0))))  # parent
        scene_data.append(struct.pack("i", (int(1))))  # transform count
        scene_data.append(struct.pack("i", (int(3))))  # transform type

    helpers.output_file.scene.append(scene_data)






