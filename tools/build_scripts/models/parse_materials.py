import models.helpers as helpers
import os
import struct
import dependencies

schema = "{http://www.collada.org/2005/11/COLLADASchema}"
image_list = []


class library_image:
    id = ""
    filename = ""

    def __init__(self):
        id = ""
        filename = ""


class texture_map:
    filename = ""
    wrap_u = ""
    wrap_v = ""

    def __init__(self):
        self.filename = ""
        self.wrap_u = ""
        self.wrap_v = ""


class material:
    id = ""
    diffuse_colour = ""
    specular_colour = ""
    shininess = ""
    reflectivity = ""
    diffuse_map = None
    specular_map = None
    normal_map = None

    def __init__(self):
        self.id = ""
        self.diffuse_colour = "0 0 0 0"
        self.specular_colour = "1 1 1 1"
        self.shininess = "0"
        self.reflectivity = ""
        self.diffuse_map = None
        self.specular_map = None
        self.normal_map = None


def parse_library_images(library_node):
    for img_node in library_node.iter(schema+'image'):
        lib_img = library_image()
        lib_img.id = img_node.get("id")
        for file_node in img_node.iter(schema+'init_from'):
            corrected = file_node.text.replace('\\', '/')
            identifier = dependencies.get_build_config_setting("textures_dir")
            identifier = "assets/exported/textures/"
            texture_src = corrected.find(identifier)
            # check for relative path
            if texture_src == -1:
                identifier = "../textures/"
                texture_src = corrected.find(identifier)
            if texture_src != -1:
                texture_sub_dir = corrected[texture_src + len(identifier):len(corrected)]
                texture_bin = os.path.join("data", "textures", texture_sub_dir)
                lib_img.filename = texture_bin
                image_list.append(lib_img)
                break
            else:
                corrected = corrected.replace("/models/images/", "/textures/")
                corrected = corrected.replace("file://", "assets/textures/")
            split_dirs = corrected.split('/')
            filename_split = len(split_dirs)-1
            if helpers.author == "Max":
                split_dirs[filename_split] = split_dirs[filename_split].lstrip("0123456789_")
            cur_dir = 0
            lib_img.filename = ""
            while split_dirs[cur_dir] != "assets" and cur_dir < filename_split:
                cur_dir += 1
            if cur_dir == len(split_dirs)-1:
                print("warning: texture not in assets folder " + lib_img.filename)
                lib_img.filename = "data/textures/" + split_dirs[filename_split]
            else:
                split_dirs[cur_dir] = "data"
                while cur_dir < len(split_dirs):
                    lib_img.filename += split_dirs[cur_dir]
                    if cur_dir != filename_split:
                        lib_img.filename += '/'
                    cur_dir += 1
            image_list.append(lib_img)
            break


def parse_texture(tex_node):
    tex = texture_map()
    for lib_img in image_list:
        if lib_img.id + "-sampler" == tex_node.get("texture") or lib_img.id + "-image" == tex_node.get("texture") or lib_img.id == tex_node.get("texture"):
            tex.filename = lib_img.filename
            break
    return tex


def parse_light_component(component_node):
    tex = None
    col = "1.0 1.0 1.0 1.0"
    for col_node in component_node.iter(schema+'color'):
        col = col_node.text

    for tex_node in component_node.iter(schema+'texture'):
        tex = parse_texture(tex_node)

    return col, tex


def parse_effect(effect):
    output_material = material()

    for param in effect.iter(schema+'diffuse'):
        col_out, tex_out = parse_light_component(param)
        output_material.diffuse_colour = col_out
        output_material.diffuse_map = tex_out

    for param in effect.iter(schema+'specular'):
        col_out, tex_out = parse_light_component(param)
        output_material.specular_colour = col_out
        output_material.specular_map = tex_out

    output_material.shininess = "1.0"
    for param in effect.iter(schema+'shininess'):
        for f in param.iter(schema+'float'):
            output_material.shininess = f.text

    output_material.reflectivity = "1.0"
    for param in effect.iter(schema+'reflectivity'):
        for f in param.iter(schema+'float'):
            output_material.reflectivity = f.text

    for bump in effect.iter(schema+'bump'):
        for tex_node in bump.iter(schema+'texture'):
            output_material.normal_map = parse_texture(tex_node)

    return output_material

def parse_materials(dae_root, materials_root):

    # find effects library
    library_effects = None
    for child in dae_root:
        if child.tag.find("library_effects") != -1:
            library_effects = child
            break
    if(library_effects == None):
        return

    for mat in materials_root.iter(schema+'material'):
        effect_id = None
        for fx_instance in mat.iter(schema+'instance_effect'):
            effect_id = fx_instance.get("url").replace("#", "")
        for effect in library_effects.iter(schema+'effect'):
            if effect.get("id").find(effect_id) != -1:
                new_mat = parse_effect(effect)
                new_mat.id = effect_id.replace("-fx","")
                write_material_file(new_mat)
                break


def write_material_file(mat):
    # write out materials
    material_data = [struct.pack("i", (int(helpers.version_number)))]

    # float values
    helpers.pack_split_floats(material_data, mat.diffuse_colour)
    helpers.pack_split_floats(material_data, mat.specular_colour)
    helpers.pack_split_floats(material_data, mat.shininess)
    helpers.pack_split_floats(material_data, mat.reflectivity)

    # texture maps
    maps = []
    types = []
    if mat.diffuse_map:
        maps.append(mat.diffuse_map.filename)
        types.append(0)
    if mat.normal_map:
        maps.append(mat.normal_map.filename)
        types.append(1)
    if mat.specular_map:
        maps.append(mat.specular_map.filename)
        types.append(2)

    num_maps = len(maps)
    material_data.append(struct.pack("i", num_maps))
    for i in range(0, num_maps):
        material_data.append(struct.pack("i", types[i]))
        pack_texture(material_data, maps[i])

    helpers.output_file.material_names.append(mat.id)
    helpers.output_file.materials.append(material_data)


def pack_texture(output, tex):
    [fnoext, fext] = os.path.splitext(tex)
    tex = fnoext + ".dds"
    helpers.pack_parsable_string(output, tex)