import helpers
import os
import struct

schema = "{http://www.collada.org/2005/11/COLLADASchema}"

class skin_controller:
    bind_shape_matrix = []
    joints_sid = []
    joint_bind_matrix = []
    indices = []
    weights = []
    bones_per_vertex = []
    joint_weight_indices = []
    vec4_weights = None
    vec4_indices = None

# Geometries
class vertex_input:
    id = ""
    source_ids = []
    semantic_ids = []
    offsets = []
    sets = []

    def __init__(self):
        self.source_ids = []
        self.semantic_ids = []
        self.offsets = []
        self.sets = []
        self.id = ""

class geometry_source:
    id = ""
    float_values = []
    stride = 0

class vertex_stream:
    semantic_id = ""
    float_values = []

    def __init__(self):
        self.float_values = []

class geometry_mesh:
    sources = []
    triangle_inputs = []
    triangle_indices = []
    triangle_count = 0
    vertices = []
    vertex_buffer = []
    index_buffer = []
    min_extents = []
    max_extents = []
    collision_vertices = []
    controller = None

    semantic_ids = ["POSITION", "NORMAL", "TEXCOORD0", "TEXCOORD1", "TEXTANGENT1", "TEXBINORMAL1", "BLENDINDICES", "BLENDWEIGHTS"]
    required_elements = [True, True, True, True, True, True, False, False]
    vertex_elements = []

    def get_element_stream(self,sem_id):
        index = 0
        for s in self.semantic_ids:
            if s == sem_id:
                return self.vertex_elements[index]
            index += 1
        print("Error: unsupported vertex semantic " + sem_id)

    def __init__(self):
        self.sources = []
        self.triangle_inputs = []
        self.triangle_indices = []
        self.vertices = []
        self.vertex_buffer = []
        self.vertex_buffer = []
        self.max_extents = []
        self.min_extents = []
        self.collision_vertices = []
        self.vertex_elements = []

        index = 0
        for s in self.semantic_ids:
            self.vertex_elements.append(vertex_stream())
            self.vertex_elements[index].semantic_id = s
            index += 1

class geometry_container:
    id = ""
    name = ""
    materials = []
    meshes = []


def add_vertex_input(input_node, vertex_input_instance):
    set = input_node.get("set")
    if set == None:
        set = ""

    vertex_input_instance.semantic_ids.append(input_node.get("semantic") + set )
    src_str = input_node.get("source")
    src_str = src_str.replace("#", "")
    vertex_input_instance.source_ids.append(src_str)
    vertex_input_instance.offsets.append(input_node.get("offset"))
    vertex_input_instance.offsets.append(input_node.get("set"))


def grow_extents(vals, mesh):
    if len(mesh.min_extents) == 0:
        mesh.min_extents.append(vals[0])
        mesh.min_extents.append(vals[1])
        mesh.min_extents.append(vals[2])
        mesh.max_extents.append(vals[0])
        mesh.max_extents.append(vals[1])
        mesh.max_extents.append(vals[2])
    else:
        for i in range(0, 3, 1):
            if vals[i] <= float(mesh.min_extents[i]):
                mesh.min_extents[i] = vals[i]
            if vals[i] >= float(mesh.max_extents[i]):
                mesh.max_extents[i] = vals[i]


def write_source_float_channel(p, src, sem_id, mesh):
    stream = mesh.get_element_stream(sem_id)

    if stream != None:
        base_p = int(p) * int(src.stride)
        # always write 4 values per source / semantic so they occupy 1 vector register
        #if sem_id == "POSITION" or sem_id == "NORMAL" or sem_id == "TEXTANGENT1" or sem_id == "TEXBINORMAL1":

        # correct cordspace
        vals = [0.0, 0.0, 0.0, 1.0]

        for i in range(0, int(src.stride)):
            vals[i] = float(src.float_values[base_p + i])

        if sem_id == "POSITION":
            grow_extents(vals, mesh)

        cval_x = vals[0]
        cval_y = vals[1]
        cval_z = vals[2]
        if helpers.author == "Max" \
                and sem_id.find("TEXCOORD") == -1 \
                and sem_id.find("BLENDINDICES") == -1 \
                and sem_id.find("BLENDWEIGHTS") == -1:
            cval_x = vals[0]
            cval_y = vals[2]
            cval_z = vals[1] * -1.0
        stream.float_values.append(cval_x)
        stream.float_values.append(cval_y)
        stream.float_values.append(cval_z)
        stream.float_values.append(vals[3])


def write_vertex_data(p, src_id, sem_id, mesh):
    source_index = mesh.triangle_indices[int(p)]

    # find source
    for src in mesh.sources:
        if src.id == src_id:
            write_source_float_channel(source_index, src, sem_id, mesh)
            return

    # look in vertex list
    # This is the vertex position buffer
    for v in mesh.vertices:
        if v.id == src_id:
            itr = 0
            for v_src in v.source_ids:
                for src in mesh.sources:
                    if v_src == src.id:
                        write_source_float_channel(source_index, src, v.semantic_ids[itr], mesh)
                        # also write WEIGHTS and JOINTS if we need them
                        if mesh.controller != None:
                            write_source_float_channel(source_index, mesh.controller.vec4_indices, "BLENDINDICES", mesh)
                            write_source_float_channel(source_index, mesh.controller.vec4_weights, "BLENDWEIGHTS", mesh)
                itr = itr + 1


def generate_vertex_buffer(mesh):
    index_stride = 0
    for v in mesh.triangle_inputs:
        for o in v.offsets:
            if (o != None):
                index_stride = max(int(o) + 1, index_stride)

    p = 0
    while p < len(mesh.triangle_indices):
        for v in mesh.triangle_inputs:
            for s in range(0, len(v.source_ids), 1):
                write_vertex_data(p + int(v.offsets[s]), v.source_ids[s], v.semantic_ids[s], mesh)
        p += index_stride

    # interleave streams
    num_floats = len(mesh.vertex_elements[0].float_values)

    for stream in mesh.vertex_elements:
        stream_len = len(stream.float_values)
        if stream_len != num_floats and stream_len > 0:
            print("error mismatched vertex size : ")
            print(stream.semantic_id + " is " + str(stream_len) + " should be " + str(num_floats))

    # pad out required streams
    for i in range(0, len(mesh.semantic_ids)):
        if len(mesh.vertex_elements[i].float_values) == 0 and mesh.required_elements[i]:
            for j in range(0, num_floats):
                mesh.vertex_elements[i].float_values.append(0.0)

    for i in range(0, num_floats, 4):
        for f in range(0, 4, 1):
            # write vertex always
            mesh.vertex_buffer.append(mesh.vertex_elements[0].float_values[i+f])
        if len(mesh.vertex_elements[1].float_values) == num_floats:
            # write normal
            for f in range(0, 4, 1):
                mesh.vertex_buffer.append(mesh.vertex_elements[1].float_values[i+f])
        if len(mesh.vertex_elements[2].float_values) == num_floats \
                and len(mesh.vertex_elements[3].float_values) == num_floats:
            # texcoord 0 and 1 packed
            for f in range(0, 2, 1):
                mesh.vertex_buffer.append(mesh.vertex_elements[2].float_values[i+f])
            for f in range(0, 2, 1):
                mesh.vertex_buffer.append(mesh.vertex_elements[3].float_values[i+f])
        elif len(mesh.vertex_elements[2].float_values) == num_floats:
            for f in range(0, 4, 1):
                # texcoord 0
                mesh.vertex_buffer.append(mesh.vertex_elements[2].float_values[i+f])
        if len(mesh.vertex_elements[4].float_values) == num_floats:
            # write textangent
            for f in range(0, 4, 1):
                mesh.vertex_buffer.append(mesh.vertex_elements[4].float_values[i+f])
        if len(mesh.vertex_elements[5].float_values) == num_floats:
            # write texbinormal
            for f in range(0, 4, 1):
                mesh.vertex_buffer.append(mesh.vertex_elements[5].float_values[i+f])
        if len(mesh.vertex_elements[6].float_values) == num_floats:
            # write blendindices
            for f in range(0, 4, 1):
                mesh.vertex_buffer.append(mesh.vertex_elements[6].float_values[i+f])
        if len(mesh.vertex_elements[7].float_values) == num_floats:
            # write blendweights
            for f in range(0, 4, 1):
                mesh.vertex_buffer.append(mesh.vertex_elements[7].float_values[i+f])


def generate_index_buffer(mesh):
    mesh.index_buffer = []
    vert_count = int(len(mesh.vertex_buffer)) / (int(4) * int(5))
    for i in range(0, int(vert_count), 3):
        mesh.index_buffer.append(i + 2)
        mesh.index_buffer.append(i + 1)
        mesh.index_buffer.append(i + 0)


def parse_mesh(node, tris, controller):
    mesh_instance = geometry_mesh()
    mesh_instance.controller = controller

    # find geometry sources
    for src in node.iter(schema + 'source'):
        geom_src = geometry_source()
        geom_src.float_values = []
        geom_src.id = src.get("id")
        for accessor in src.iter(schema + 'accessor'):
            geom_src.stride = accessor.get("stride")
        for data in src.iter(schema + 'float_array'):
            splitted = data.text.split()
            for vf in splitted:
                geom_src.float_values.append(vf)
        mesh_instance.sources.append(geom_src)

    # find vertex struct
    for v in node.iter(schema + 'vertices'):
        vertex_instance = vertex_input()
        vertex_instance.id = v.get("id")
        for i in v.iter(schema + 'input'):
            add_vertex_input(i, vertex_instance)
        mesh_instance.vertices.append(vertex_instance)

    # find triangles (multi stream index buffers)
    mesh_instance.triangle_count = tris.get("count")
    for i in tris.iter(schema + 'input'):
        vertex_instance = vertex_input()
        vertex_instance.id = i.get("id")
        add_vertex_input(i, vertex_instance)
        mesh_instance.triangle_inputs.append(vertex_instance)

    # find indices
    for indices in tris.iter(schema + 'p'):
        splitted = indices.text.split()
        for vi in splitted:
            mesh_instance.triangle_indices.append(vi)

    # roll the multi stream vertex buffer into 1
    generate_vertex_buffer(mesh_instance)

    # wind triangles the other way
    generate_index_buffer(mesh_instance)

    return mesh_instance


def parse_controller(controller_root,geom_name):
    for contoller in controller_root.iter(schema + 'controller'):
        for skin in contoller.iter(schema + 'skin'):
            skin_source = skin.get("source")
            if skin_source == "#geom-" + geom_name or skin_source == "#" + geom_name:
                sc = skin_controller()
                for bs_node in contoller.iter(schema + 'bind_shape_matrix'):
                    sc.bind_shape_matrix = bs_node.text.split()
                for src in skin.iter(schema + 'source'):
                    param_name = None
                    for param in src.iter(schema + 'param'):
                        param_name = param.get("name")
                        break
                    for names in src.iter(schema + 'Name_array'):
                        sc.joints_sid = names.text.split()
                    for floats in src.iter(schema + 'float_array'):
                        if param_name == "WEIGHT":
                            sc.weights = floats.text.split()
                        elif param_name == "TRANSFORM":
                            sc.joint_bind_matrix = floats.text.split()
                for stream in skin.iter(schema + 'vertex_weights'):
                    for vcount in stream.iter(schema + 'vcount'):
                        sc.bones_per_vertex = vcount.text.split()
                    for v in stream.iter(schema + 'v'):
                        sc.joint_weight_indices = v.text.split()

                sc.vec4_weights = geometry_source()
                sc.vec4_indices = geometry_source()

                sc.vec4_weights.stride = 4
                sc.vec4_indices.stride = 4

                sc.vec4_indices.float_values = []
                sc.vec4_weights.float_values = []

                vpos = 0
                for influences in sc.bones_per_vertex:
                    indices = [0, 0, 0, 0, 0, 0, 0, 0]
                    weights = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]
                    strongest_index = 0
                    strongest_weight = 0.0
                    for i in range(0,int(influences),1):
                        indices[i] = sc.joint_weight_indices[vpos]
                        weight_index = sc.joint_weight_indices[vpos+1]
                        weights[i] = sc.weights[int(weight_index)]
                        if float(strongest_weight) < float(weights[i]):
                            strongest_weight = weights[i]
                            strongest_index = indices[i]
                        vpos += 2
                    for i in range(int(influences),4,1):
                        indices[i] = strongest_index
                        weights[i] = 0.0
                    for i in range(0,4,1):
                        # print( "index = " + str(indices[i]) + " wght = " + str(weights[i]))
                        sc.vec4_indices.float_values.append(indices[i])
                        sc.vec4_weights.float_values.append(weights[i])

            return sc
    return None

def parse_geometry(node, lib_controllers):

    helpers.output_file.geometry_names.clear()
    helpers.output_file.geometry_sizes.clear()
    helpers.output_file.geometry.clear()

    for geom in node.iter(schema + 'geometry'):
        geom_container = geometry_container()
        geom_container.id = geom.get("id")
        geom_container.name = geom.get("name")
        geom_container.meshes = []
        geom_container.materials = []
        geom_container.controller = None

        if lib_controllers != None:
            geom_container.controller = parse_controller(lib_controllers, geom_container.name)

        for mesh in geom.iter(schema + 'mesh'):
            submesh = 0
            for tris in mesh.iter(schema + 'polylist'):
                geom_container.materials.append(tris.get("material"))
                geom_container.meshes.append(parse_mesh(mesh, tris, geom_container.controller))
                submesh = submesh + 1

            for tris in mesh.iter(schema + 'triangles'):
                geom_container.materials.append(tris.get("material"))
                geom_container.meshes.append(parse_mesh(mesh, tris, geom_container.controller))
                submesh = submesh + 1

        write_geometry_file(geom_container)


def write_geometry_file(geom_instance):
    # write out geometry
    num_meshes = len(geom_instance.meshes)
    if num_meshes == 0:
        return

    print("packing geometry: " + geom_instance.name)

    geometry_data = [struct.pack("i", (int(helpers.version_number)))]
    geometry_data.append(struct.pack("i", (int(num_meshes))))

    collision_type = 0
    collision_dynamic = 0

    collision_names = ["pxbox", "pxcyl", "pxsph", "pxcap", "pxhul", "pxmsh", "pxcmp"]
    for col_type in range(0, len(collision_names), 1):
        if geom_instance.name.find(collision_names[col_type] + "_d") != -1:
            collision_type = col_type + 1
            collision_dynamic = 1
        elif geom_instance.name.find(collision_names[col_type] + "_s") != -1:
            collision_type = col_type + 1
            collision_dynamic = 0

    for mat in geom_instance.materials:
        helpers.pack_parsable_string(geometry_data, mat)

    data_size = len(geometry_data)*4
    for mesh in geom_instance.meshes:
        # write collision info
        mesh_data = []
        mesh_data.append(struct.pack("i", (int(collision_type))))
        mesh_data.append(struct.pack("i", (int(collision_dynamic))))

        # write min / max extents
        for i in range(0, 3, 1):
            mesh_data.append(struct.pack("f", (float(mesh.min_extents[i]))))
        for i in range(0, 3, 1):
            mesh_data.append(struct.pack("f", (float(mesh.max_extents[i]))))
        # write vb and ib
        mesh_data.append(struct.pack("i", (len(mesh.vertex_elements[0].float_values))))
        mesh_data.append(struct.pack("i", (len(mesh.vertex_buffer))))

        mesh_data.append(struct.pack("i", (len(mesh.index_buffer))))
        mesh_data.append(struct.pack("i", (len(mesh.collision_vertices))))

        index_type = "i"
        if len(mesh.index_buffer) < 65535:
            index_type = "H"

        skinned = 0
        if geom_instance.controller != None:
            skinned = 1

        mesh_data.append(struct.pack("i", int(skinned)))

        if skinned:
            helpers.pack_corrected_4x4matrix(mesh_data, geom_instance.controller.bind_shape_matrix)
            mesh_data.append(struct.pack("i", len(geom_instance.controller.joint_bind_matrix)))
            helpers.pack_corrected_4x4matrix(mesh_data, geom_instance.controller.joint_bind_matrix)
        for vertexfloat in mesh.vertex_elements[0].float_values:
            # position only buffer
            mesh_data.append(struct.pack("f", (float(vertexfloat))))
        for vertexfloat in mesh.vertex_buffer:
            mesh_data.append(struct.pack("f", (float(vertexfloat))))

        data_size += len(mesh_data)*4

        for index in mesh.index_buffer:
            mesh_data.append(struct.pack(index_type, (int(index))))

        data_size += len(mesh.index_buffer) * 2

        for vertexfloat in mesh.collision_vertices:
            mesh_data.append(struct.pack("f", (float(vertexfloat))))

        data_size += len(mesh.collision_vertices) * 4

        for m in mesh_data:
            geometry_data.append(m)

    helpers.output_file.geometry_names.append(geom_instance.name)
    helpers.output_file.geometry.append(geometry_data)
    helpers.output_file.geometry_sizes.append(data_size)

