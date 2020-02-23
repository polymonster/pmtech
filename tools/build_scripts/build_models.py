import os
import xml.etree.ElementTree as ET
import struct
import dependencies
import time
import util
import sys
import models.helpers as helpers
import models.parse_meshes as parse_meshes
import models.parse_materials as parse_materials
import models.parse_animations as parse_animations
import models.parse_obj as parse_obj
import subprocess

stats_start = time.time()
root_dir = os.getcwd()
model_dir = ""
schema = "{http://www.collada.org/2005/11/COLLADASchema}"
transform_types = ["translate", "rotate", "matrix"]

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


file_list = []
if "-i" in sys.argv and "-o" in sys.argv:
    helpers.bin_dir = os.path.join(os.getcwd(), "bin", helpers.platform, "")
    # pmbuild v3 path
    for a in range(0, len(sys.argv)):
        if sys.argv[a] == "-i":
            file_list.append(sys.argv[a+1])
        if sys.argv[a] == "-o":
            helpers.build_dir = sys.argv[a+1]
            # create models dir
            if not os.path.exists(helpers.build_dir):
                os.makedirs(helpers.build_dir)


mesh_opt = ""
if "-mesh_opt" in sys.argv:
    for a in range(0, len(sys.argv)):
        if sys.argv[a] == "-mesh_opt":
            mesh_opt = sys.argv[a+1]


# build list of files
for file in file_list:
    deps = dict()
    deps["files"] = []

    # file path stuff, messy!
    f = file
    root = os.path.dirname(f)
    [fnoext, fext] = os.path.splitext(file)

    out_dir = helpers.build_dir
    current_filename = os.path.basename(file)
    helpers.current_filename = current_filename
    helpers.build_dir = out_dir
    helpers.output_file = helpers.pmm_file()
    base_out_file = os.path.join(out_dir, os.path.basename(fnoext))
    depends_dest = base_out_file

    deps["dir"] = out_dir
    util.create_dir(out_dir)

    if file.endswith(".obj"):
        dependency_inputs = [os.path.join(os.getcwd(), f)]
        dependency_outputs = [depends_dest + ".pmm"]
        # add dependency to the scripts for obj
        main_file = os.path.realpath(__file__)
        dependency_inputs.append(os.path.realpath(__file__))
        dependency_inputs.append(main_file.replace("build_models.py", os.path.join("models", "parse_obj.py")))
        file_info = dependencies.create_dependency_info(dependency_inputs, dependency_outputs)
        deps["files"].append(file_info)
        if dependencies.check_up_to_date_single(depends_dest + ".pmm"):
            continue
        parse_obj.write_geometry(os.path.basename(file), root)
        helpers.output_file.write(base_out_file + ".pmm")
    elif file.endswith(".dae"):
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

        dependency_inputs = [os.path.join(os.getcwd(), f)]
        dependency_outputs = [depends_dest + ".pmm", depends_dest + ".pma"]

        # add dependency to the build scripts dae
        main_file = os.path.realpath(__file__)
        dependency_inputs.append(os.path.realpath(__file__))
        models_lib = ["parse_materials.py",
                      "parse_animations.py",
                      "parse_meshes.py",
                      "parse_scene.py"]

        for lib_file in models_lib:
            dependency_inputs.append(main_file.replace("build_models.py", os.path.join("models", lib_file)))

        if len(mesh_opt) > 0:
            dependency_inputs.append(mesh_opt)

        file_info = dependencies.create_dependency_info(dependency_inputs, dependency_outputs)

        deps["files"].append(file_info)

        up_to_date = False
        up_to_date = dependencies.check_up_to_date_single(depends_dest + ".pma")

        if up_to_date:
            print(f + " already up to date")
            continue

        util.create_dir(base_out_file + ".pmm")
        parse_dae()
        helpers.output_file.write(base_out_file + ".pmm")
        parse_animations.write_animation_file(base_out_file + ".pma")

    if len(deps["files"]) > 0:
        dependencies.write_to_file_single(deps, depends_dest + ".json")

    # apply optimisation
    if len(mesh_opt) > 0:
        cmd = " -i " + base_out_file + ".pmm"
        p = subprocess.Popen(mesh_opt + cmd, shell=True)
        p.wait()

