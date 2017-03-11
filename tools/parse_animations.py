schema = "{http://www.collada.org/2005/11/COLLADASchema}"
animations = []
joint_list = []

#Animations
class animation:
    bone_name = ""
    bone_index = -1
    inputs = []
    translation_x = []
    translation_y = []
    translation_z = []
    rotation_x = []
    rotation_y = []
    rotation_z = []

    def __init__(self):
        self.bone_name = ""
        self.bone_index = -1
        self.inputs = []
        self.translation_x = []
        self.translation_y = []
        self.translation_z = []
        self.rotation_x = []
        self.rotation_y = []
        self.rotation_z = []

def parse_animations(root,anims_out,joints_in):
    global animations
    global joint_list
    animations = anims_out
    joint_list = joints_in
    animations.clear()
    for node in root.iter(schema+'source'):
        anim_id = node.get("id")
        if anim_id.find('input') != -1:
            add_animation_input(anim_id,node)
        if anim_id.find('output') != -1:
            add_animation_output(anim_id,node)

def get_animation(anim_id):
    for joint_name in joint_list:
        if( anim_id.find( "-" + joint_name + "_") != -1 ):
            anim_bone_index = joint_list.index(joint_name)
            for animation_instance in animations:
                if(animation_instance.bone_index == anim_bone_index):
                    return animation_instance
            outval = animation()
            outval.bone_index = anim_bone_index
            outval.bone_name = joint_name
            animations.append(outval)
            return outval
    outval = animation()
    outval.bone_index = -1
    return outval

def add_animation_output(anim_id,source_node):
    anim_instance = get_animation(anim_id)
    if( anim_instance.bone_index == -1 ):
        return
    for data_node in source_node.iter(schema+'float_array'):
        splitted = data_node.text.split()
        for f in splitted:
            if(anim_id.find("_translation.X") != -1):
                anim_instance.translation_x.append(f)
            elif(anim_id.find("_translation.Y") != -1):
                anim_instance.translation_y.append(f)
            elif(anim_id.find("_translation.Z") != -1):
                anim_instance.translation_z.append(f)
            elif(anim_id.find("_rotationX.ANGLE") != -1):
                anim_instance.rotation_x.append(f)
            elif(anim_id.find("_rotationY.ANGLE") != -1):
                anim_instance.rotation_y.append(f)
            elif(anim_id.find("_rotationZ.ANGLE") != -1):
                anim_instance.rotation_z.append(f)
        return

def add_animation_input(anim_id,source_node):
    anim_instance = get_animation(anim_id)
    for data_node in source_node.iter(schema+'float_array'):
         if(len(anim_instance.inputs) == 0):
            splitted = data_node.text.split()
            for f in splitted:
                anim_instance.inputs.append(f)
