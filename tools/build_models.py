import os
import xml.etree.ElementTree as ET
import struct
import parse_meshes
import parse_materials
import parse_animations
import helpers
import json
import dependencies
import time
stats_start = time.time()

#win32 / collada
print("--------------------------------------------------------------------------------")
print("pmtech model and animation conversion ------------------------------------------")
print("--------------------------------------------------------------------------------")

root_dir = os.getcwd()

config = open("build_config.json")
build_config = json.loads(config.read())

model_dir = helpers.correct_path(build_config["models_dir"])

schema = "{http://www.collada.org/2005/11/COLLADASchema}"
transform_types = ["translate", "rotate", "matrix"]

print("processing directory: " + model_dir)

# create models dir
if not os.path.exists(helpers.build_dir):
    os.makedirs(helpers.build_dir)

geom_attach_data_list = []
material_attach_data_list = []
material_symbol_list = []
type_list = []
joint_list = []
transform_list = []
parent_list = []
animations = []
geometries = []
materials = []


def parse_visual_scenes(root):
    for scene in root.iter(schema+'visual_scene'):
        for child in scene:
            if child.tag.find(schema+'node') != -1:
                parse_node(child, child)
    write_scene_file()


def axis_swap_transform(text):
    splitted = text.split()
    if len(splitted) >= 3:
        val_x = float(splitted[0])
        val_y = float(splitted[1])
        val_z = float(splitted[2])
        cval_x = val_x
        cval_y = val_y
        cval_z = val_z
        if helpers.author == "Max":
            cval_x = val_x
            cval_y = val_z
            cval_z = val_y * -1.0
        out_str = str(cval_x) + " " + str(cval_y) + " " + str(cval_z)
        if len(splitted) == 4:
            out_str += " " + splitted[3]
    return out_str


def parse_node(node, parent_node):
    node_type = 0
    geom_attach_data = "none"
    material_attach_data = []
    material_symbol_data = []

    for child in node:
        if child.tag.find(schema+'instance_geometry') != -1 or child.tag.find(schema+'instance_controller') != -1:
            geom_attach_data = child.get('url')
            geom_attach_data = geom_attach_data.replace("-skin1", "")
            geom_attach_data = geom_attach_data.replace("-skin", "")
            geom_attach_data = geom_attach_data.replace("#geom-", "")
            geom_attach_data = geom_attach_data.replace("#", "")
            node_type = 2
            for mat_node in child.iter(schema+'instance_material'):
                material_target_corrected = mat_node.get('target').replace("#","")
                material_target_corrected = material_target_corrected.replace("-material","")
                material_attach_data.append(material_target_corrected)
                material_symbol_data.append(mat_node.get('symbol'))
            break

    if node.get('type') == "JOINT":
        geom_attach_data = "joint"
        node_type = 1

    type_list.append(node_type)
    parent_list.append(parent_node.get("name"))
    joint_list.append(node.get("name"))
    geom_attach_data_list.append(geom_attach_data)
    material_attach_data_list.append(material_attach_data)
    material_symbol_list.append(material_symbol_data)

    sub_transform_list = []
    written_transforms = 0
    for child in node:
        if len(sub_transform_list) < 4:
            if child.tag.find(schema+'matrix') != -1:
                sub_transform_list.append("matrix " + helpers.correct_4x4matrix(child.text))
            if child.tag.find(schema+'translate') != -1:
                sub_transform_list.append("translate " + axis_swap_transform(child.text))
            if child.tag.find(schema+'rotate') != -1:
                sub_transform_list.append("rotate " + axis_swap_transform(child.text))
        if child.tag.find(schema+'node') != -1:
            if len(sub_transform_list) > 0:
                written_transforms = 1
                transform_list.append(sub_transform_list)
                sub_transform_list = []
            if child.get('type') == parent_node.get('type') or child.get('type') == "JOINT":
                parse_node(child, node)

    if len(sub_transform_list) > 0:
        written_transforms = 1
        transform_list.append(sub_transform_list)

    if written_transforms != 1:
        sub_transform_list.append("translate 0 0 0")
        sub_transform_list.append("rotate 0 0 0 0")
        transform_list.append(sub_transform_list)


def parse_dae():
    file_path = f
    tree = ET.parse(file_path)
    root = tree.getroot()

    for author_node in root.iter(schema+'authoring_tool'):
        helpers.author = "Max"
        if author_node.text.find("Maya") != -1:
            helpers.author = "Maya"

    for upaxis in root.iter(schema+'up_axis'):
        if upaxis.text == "Y_UP":
            helpers.author = "Maya"
        elif upaxis.text == "Z_UP":
            helpers.author = "Max"

        print("Author = " + helpers.author)

    lib_controllers = None
    # pre requisites
    for child in root:
        if child.tag.find("library_images") != -1:
            parse_materials.parse_library_images(child)
        if child.tag.find("library_controllers") != -1:
            lib_controllers = child
        if child.tag.find("library_visual_scenes") != -1:
            parse_visual_scenes(child)

    for child in root:
        if child.tag.find("library_geometries") != -1:
            parse_meshes.parse_geometry(child, lib_controllers)
        if child.tag.find("library_materials") != -1:
            parse_materials.parse_materials(root, child)
        if child.tag.find("library_animations") != -1:
            parse_animations.parse_animations(child, animations, joint_list)


# File Writers
def write_scene_file():
    # write out nodes and transforms
    numjoints = len(joint_list)
    if numjoints == 0:
        return

    print("packing scene")
    scene_data = [struct.pack("i", (int(helpers.version_number))),
                  struct.pack("i", (int(numjoints)))]

    for j in range(numjoints):
        if joint_list[j] is None:
            joint_list[j] = "no_name"
        scene_data.append(struct.pack("i", (int(type_list[j]))))
        helpers.pack_parsable_string(scene_data, joint_list[j])
        helpers.pack_parsable_string(scene_data, geom_attach_data_list[j])
        scene_data.append(struct.pack("i", (int(len(material_attach_data_list[j])))))
        for i in range(0, len(material_attach_data_list[j])):
            helpers.pack_parsable_string(scene_data, material_attach_data_list[j][i])
            helpers.pack_parsable_string(scene_data, material_symbol_list[j][i])
        parentindex = joint_list.index(parent_list[j])
        scene_data.append(struct.pack("i", (int(parentindex))))

        scene_data.append(struct.pack("i", (int(len(transform_list[j])))))
        for t in transform_list[j]:
            splitted = t.split()
            transform_type_index = transform_types.index(splitted[0])
            scene_data.append(struct.pack("i", (int(transform_type_index))))
            for val in range(1, len(splitted)):
                scene_data.append(struct.pack("f", (float(splitted[val]))))

    helpers.output_file.scene.append(scene_data)


def write_joint_file():
    # write out joints
    if len(joint_list) == 0:
        return

    numjoints = len(joint_list)

    print("packing " + str(numjoints) + " joints")
    joint_data = [struct.pack("i", (int(helpers.version_number))),
                  struct.pack("i", (int(len(animations))))]

    # write out anims
    for animation_instance in animations:
        num_times = len(animation_instance.inputs)
        bone_index = int(animation_instance.bone_index)
        joint_data.append(struct.pack("i", bone_index))
        joint_data.append(struct.pack("i", (int(num_times))))
        joint_data.append(struct.pack("i", (int(len(animation_instance.translation_x)))))
        joint_data.append(struct.pack("i", (int(len(animation_instance.rotation_x)))))
        for t in range(len(animation_instance.inputs)):
            joint_data.append(struct.pack("f", (float(animation_instance.inputs[t]))))
            if len(animation_instance.translation_x) == num_times:
                joint_data.append(struct.pack("f", (float(animation_instance.translation_x[t]))))
                joint_data.append(struct.pack("f", (float(animation_instance.translation_y[t]))))
                joint_data.append(struct.pack("f", (float(animation_instance.translation_z[t]))))
            if len(animation_instance.rotation_x) == num_times:
                joint_data.append(struct.pack("f", (float(animation_instance.rotation_x[t]))))
                joint_data.append(struct.pack("f", (float(animation_instance.rotation_y[t]))))
                joint_data.append(struct.pack("f", (float(animation_instance.rotation_z[t]))))

    helpers.output_file.joints.append(joint_data)


# entry
for root, dirs, files in os.walk(model_dir):
    dependencies_directory = dict()
    dependencies_directory["files"] = []
    for file in files:
        if file.endswith(".dae"):
            joint_list = []
            transform_list = []
            parent_list = []
            geometries = []
            type_list = []
            geom_attach_data_list = []
            material_attach_data_list = []
            material_symbol_list = []
            node_name_list = []
            animations = []
            image_list = []

            [fnoext, fext] = os.path.splitext(file)

            helpers.bin_dir = os.path.join(os.getcwd(), "bin", helpers.platform, "")
            helpers.build_dir = os.path.join(os.getcwd(), "bin", helpers.platform, "data", "models")

            assets_pos = root.find(model_dir)
            assets_pos += len(model_dir) + 1
            sub_dir = root[int(assets_pos):int(len(root))]
            out_dir = os.path.join(helpers.build_dir, sub_dir)

            if not os.path.exists(out_dir):
                os.makedirs(out_dir)

            f = os.path.join(root, file)

            current_filename = file
            helpers.current_filename = file
            helpers.build_dir = out_dir

            dependencies_directory["dir"] = out_dir

            helpers.output_file = helpers.pmm_file()

            base_out_file = os.path.join(out_dir, fnoext)

            depends_dest = base_out_file.replace(helpers.bin_dir, "")

            # add dependency to these scripts
            dependency_inputs = [os.path.join(os.getcwd(), f)]
            dependency_outputs = [depends_dest + ".pmm", depends_dest + ".pma"]

            main_file = os.path.realpath(__file__)
            dependency_inputs.append(os.path.realpath(__file__))
            models_lib = ["parse_materials.py",
                          "parse_animations.py",
                          "parse_meshes.py",
                          "parse_scene.py"]

            for lib_file in models_lib:
                print(main_file.replace("build_models.py", lib_file))
                dependency_inputs.append(main_file.replace("build_models.py", lib_file))

            file_info = dependencies.create_dependency_info(dependency_inputs, dependency_outputs)

            dependencies_directory["files"].append(file_info)

            up_to_date = False
            up_to_date = dependencies.check_up_to_date(dependencies_directory, depends_dest + ".pma")

            if up_to_date:
                print(f + " already up to date")
                continue

            print("building " + f)
            parse_dae()
            helpers.output_file.write(base_out_file + ".pmm")
            parse_animations.write_animation_file(base_out_file + ".pma")

    if len(dependencies_directory["files"]) > 0:
        dependencies.write_to_file(dependencies_directory)

stats_end = time.time()
millis = int((stats_end - stats_start) * 1000)
print("Done (" + str(millis) + "ms)")

