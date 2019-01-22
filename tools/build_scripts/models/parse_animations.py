import struct
import models.helpers as helpers

schema = "{http://www.collada.org/2005/11/COLLADASchema}"

animation_channels = []
animation_source_semantics = ["TIME", "TRANSFORM", "X", "Y", "Z", "ANGLE", "INTERPOLATION"]
animation_source_types = ["float", "float4x4", "Name"]

animation_targets = ["translate",
                     "transform",
                     "rotate",
                     "scale",
                     "translate.X",
                     "translate.Y",
                     "translate.Z",
                     "rotateX.ANGLE",
                     "rotateY.ANGLE",
                     "rotateZ.ANGLE",
                     "scale.X",
                     "scale.Y",
                     "scale.Z"]

interpolation_types = ["LINEAR", "BEZIER", "CARDINAL", "HERMITE", "BSPLINE", "STEP"]


class animation_source:
    semantic = ""
    type = ""
    data = []
    stride = 0
    count = 0


class animation_sampler:
    id = ""
    name = ""
    sources = []


class animation_channel:
    target_bone = ""
    sampler = animation_sampler()


def parse_animation_source(root, source_id):
    new_sources = []
    for src in root.iter(schema+'source'):
        if "#"+src.get("id") == source_id:
            for a in src.iter(schema+'accessor'):
                offset = 0
                for p in a.iter(schema+'param'):
                    new_source = animation_source()
                    new_source.data = []
                    new_source.count = a.get("count")
                    new_source.stride = a.get("stride")
                    new_source.semantic = p.get("name")
                    new_source.type = p.get('type')
                    new_source.offset = offset
                    offset += 1
                    for data_node in src.iter(schema+'float_array'):
                        split_floats = data_node.text.split()
                        for f in split_floats:
                            new_source.data.append(f)
                    new_sources.append(new_source)
    return new_sources


def consolidate_channels_to_targets():
    global animation_channels
    packed_targets = dict()
    for channel in animation_channels:
        info = channel.target_bone.split('/')
        bone_target = info[0]
        anim_target = info[1]
        if bone_target not in packed_targets.keys():
            packed_targets[bone_target] = []
        target_source = {"anim_target": anim_target, "channel": channel}
        packed_targets[bone_target].append(target_source)
    for target in packed_targets.keys():
        max_keys = 0
        for channel_target in packed_targets[bone_target]:
            for src in channel_target["channel"].sampler.sources:
                if src.semantic == "TIME":
                    max_keys = max(len(src.data), max_keys)


def parse_animations(root, anims_out, joints_in):
    global animation_channels
    animation_channels = []
    for animation in root.iter(schema+'animation'):
        samplers = []
        for sampler in animation.iter(schema+'sampler'):
            a_sampler = animation_sampler()
            a_sampler.id = sampler.get("id")
            a_sampler.sources = []
            for input in sampler.iter(schema+'input'):
                sampler_sources = parse_animation_source(root, input.get("source"))
                for src in sampler_sources:
                    a_sampler.sources.append(src)
            samplers.append(a_sampler)
        for channel in animation.iter(schema+'channel'):
            a_channel = animation_channel()
            for s in samplers:
                if channel.get("source") == "#"+s.id:
                    a_channel.target_bone = channel.get("target")
                    a_channel.sampler = s
                    animation_channels.append(a_channel)
        # consolidate_channels_to_targets()


def write_animation_file(filename):
    global animation_channels
    num_channels = len(animation_channels)
    if num_channels > 0:
        print("writing: " + filename)
        output = open(filename, 'wb+')
        output.write(struct.pack("i", helpers.anim_version_number))
        output.write(struct.pack("i", num_channels))
        for channel in animation_channels:
            bone = channel.target_bone.split('/')
            target = -1
            if len(bone) > 1:
                target = animation_targets.index(bone[1])
            helpers.write_parsable_string(output, bone[0])
            output.write(struct.pack("i", len(channel.sampler.sources)))
            for src in channel.sampler.sources:
                semantic_index = animation_source_semantics.index(src.semantic)
                type_index = animation_source_types.index(src.type)
                output.write(struct.pack("i", semantic_index))
                output.write(struct.pack("i", type_index))
                output.write(struct.pack("i", target))
                if src.type == "float":
                    num_floats = len(src.data) / int(src.stride)
                    output.write(struct.pack("i", int(num_floats)))
                    for f in range(src.offset, len(src.data), int(src.stride)):
                        output.write(struct.pack("f", float(src.data[f])))
                elif src.type == "float4x4":
                    output.write(struct.pack("i", len(src.data)))
                    for f in range(src.offset, len(src.data), int(src.stride)):
                        helpers.write_corrected_4x4matrix(output, src.data[f:f + 16])
                elif src.semantic == "INTERPOLATION" and src.type == "Name":
                    output.write(struct.pack("i", len(src.data)))
                    for i in range(src.offset, len(src.data), int(src.stride)):
                        output.write(struct.pack("i", interpolation_types.index(src.data[i])))
