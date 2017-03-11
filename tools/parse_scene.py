schema = "{http://www.collada.org/2005/11/COLLADASchema}"
author = ""

#Geometries
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

class geometry_container:
    id = ""
    name = ""
    materials = []
    meshes = []

def add_vertex_input(input_node, vertex_input_instance):
    vertex_input_instance.semantic_ids.append(input_node.get("semantic"))
    src_str = input_node.get("source")
    src_str = src_str.replace("#", "")
    vertex_input_instance.source_ids.append(src_str)
    vertex_input_instance.offsets.append(input_node.get("offset"))
    vertex_input_instance.offsets.append(input_node.get("set"))

def write_source_float(p, src, mesh):
    base_p = int(p) * int(src.stride)
    #always write 4 values per source / semantic so they occupy 1 vector register
    if int(src.stride) == 3:
        #correct cordspace
        val_x = float(src.float_values[base_p+0])
        val_y = float(src.float_values[base_p+1])
        val_z = float(src.float_values[base_p+2])
        cval_x = val_x
        cval_y = val_y
        cval_z = val_z
        if author == "Max":
            cval_x = val_x
            cval_y = val_z
            cval_z = val_y * -1.0
        mesh.vertex_buffer.append(cval_x)
        mesh.vertex_buffer.append(cval_y)
        mesh.vertex_buffer.append(cval_z)
        mesh.vertex_buffer.append(1.0)
    else:
        for i in range(0, 4, 1):
            if( i < int(src.stride) ):
                mesh.vertex_buffer.append(src.float_values[base_p+i])
            elif( i == 2 ):
                mesh.vertex_buffer.append("0")
            else:
                mesh.vertex_buffer.append("1.0")

def grow_extents(p, src, mesh):
    base_p = int(p) * int(src.stride)

    v = [float(src.float_values[base_p+0]), float(src.float_values[base_p+1]), float(src.float_values[base_p+2])]

    if author == "Max":
        v = [float(src.float_values[base_p+0]), float(src.float_values[base_p+2]), float(src.float_values[base_p+1]) * -1.0]

    mesh.collision_vertices.append(v[0])
    mesh.collision_vertices.append(v[1])
    mesh.collision_vertices.append(v[2])

    if len(mesh.min_extents) == 0:
        mesh.min_extents.append(v[0])
        mesh.min_extents.append(v[1])
        mesh.min_extents.append(v[2])
        mesh.max_extents.append(v[0])
        mesh.max_extents.append(v[1])
        mesh.max_extents.append(v[2])
    else:
        for i in range(0, 3, 1):
            if( v[i] <= float(mesh.min_extents[i])):
                mesh.min_extents[i] = v[i]
            if( v[i] >= float(mesh.max_extents[i])):
                mesh.max_extents[i] = v[i]

def write_vertex_data(p, src_id, mesh):

    source_index = mesh.triangle_indices[int(p)]

    #find source
    for src in mesh.sources:
        if( src.id == src_id ):
            write_source_float(source_index, src, mesh)
            #grow_extents(source_index, src, mesh)
            return

    #look in vertex list
    for v in mesh.vertices:
        if( v.id == src_id ):
            for v_src in v.source_ids:
                for src in mesh.sources:
                    if( v_src == src.id ):
                        write_source_float(source_index, src, mesh)
                        grow_extents(source_index, src, mesh)

def generate_vertex_buffer(mesh):
    index_stride = 0
    for v in mesh.triangle_inputs:
        for o in v.offsets:
            if( o != None ):
                index_stride = max(int(o)+1, index_stride)

    p = 0
    while p < len(mesh.triangle_indices):
        for v in mesh.triangle_inputs:
            for s in range( 0, len(v.source_ids), 1):
                write_vertex_data(p+int(v.offsets[s]), v.source_ids[s], mesh)
        p += index_stride

def generate_index_buffer(mesh):
    mesh.index_buffer = []
    vert_count = int(len(mesh.vertex_buffer)) / ( int(4) * int(5) )
    for i in range(0, int(vert_count), 3):
        #mesh.index_buffer.append(i)
        mesh.index_buffer.append(i + 2)
        mesh.index_buffer.append(i + 1)
        mesh.index_buffer.append(i + 0)

def parse_mesh(node,tris):
    mesh_instance = geometry_mesh()

    #find geometry sources
    for src in node.iter(schema+'source'):
        geom_src = geometry_source()
        geom_src.float_values = []
        geom_src.id = src.get("id")
        for accessor in src.iter(schema+'accessor'):
            geom_src.stride = accessor.get("stride")
        for data in src.iter(schema+'float_array'):
            splitted = data.text.split()
            for vf in splitted:
                geom_src.float_values.append(vf)
        mesh_instance.sources.append(geom_src)

    #find vertex struct
    for v in node.iter(schema+'vertices'):
        vertex_instance = vertex_input()
        vertex_instance.id = v.get("id")
        for i in v.iter(schema+'input'):
            add_vertex_input(i, vertex_instance)
        mesh_instance.vertices.append(vertex_instance)

    #find triangles (multi stream index buffers)
    mesh_instance.triangle_count = tris.get("count")
    for i in tris.iter(schema+'input'):
        vertex_instance = vertex_input()
        vertex_instance.id = i.get("id")
        add_vertex_input(i, vertex_instance)
        mesh_instance.triangle_inputs.append(vertex_instance)

    #find indices
    for indices in tris.iter(schema+'p'):
        splitted = indices.text.split()
        for vi in splitted:
            mesh_instance.triangle_indices.append(vi)

    #roll the multi stream vertex buffer into 1
    generate_vertex_buffer(mesh_instance)

    #wind triangles the other way
    generate_index_buffer(mesh_instance)

    return mesh_instance

def parse_geometry(node,author_in):
    global author
    author = author_in
    for geom in node.iter(schema+'geometry'):
        geom_container = geometry_container()
        geom_container.id = geom.get("id")
        geom_container.name = geom.get("name")
        geom_container.meshes = []
        geom_container.materials = []
        for mesh in geom.iter(schema+'mesh'):
            submesh = 0
            for tris in mesh.iter(schema+'polylist'):
                geom_container.materials.append(tris.get("material"))
                geom_container.meshes.append(parse_mesh(mesh,tris))
                submesh = submesh+1

            for tris in mesh.iter(schema+'triangles'):
                geom_container.materials.append(tris.get("material"))
                geom_container.meshes.append(parse_mesh(mesh,tris))
                submesh = submesh+1

        #write_geometry_file(geom_container)
