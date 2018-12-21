import struct
import models.helpers as helpers

schema = "{http://www.collada.org/2005/11/COLLADASchema}"

animation_channels = []
animation_source_semantics = ["TIME", "TRANSFORM", "INTERPOLATION"]
animation_source_types = ["float", "float4x4", "Name"]


class animation_source:
    semantic = ""
    type = ""
    data = []


class animation_sampler:
    id = ""
    name = ""
    sources = []


class animation_channel:
    target_bone = ""
    sampler = animation_sampler()


def parse_animation_source(root, source_id):
    new_source = animation_source()
    new_source.data = []
    for src in root.iter(schema+'source'):
        if "#"+src.get("id") == source_id:
            for a in src.iter(schema+'accessor'):
                count = a.get("count")
                stride = a.get("stride")
                for p in a.iter(schema+'param'):
                    name = p.get("name")
                    param_type = p.get('type')
            new_source.type = param_type
            new_source.semantic = name
            for data_node in src.iter(schema+'float_array'):
                split_floats = data_node.text.split()
                for f in split_floats:
                    new_source.data.append(f)
    return new_source


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
                sampler_source = parse_animation_source(root, input.get("source"))
                a_sampler.sources.append(sampler_source)
            samplers.append(a_sampler)
        for channel in animation.iter(schema+'channel'):
            a_channel = animation_channel()
            for s in samplers:
                if channel.get("source") == "#"+s.id:
                    a_channel.target_bone = channel.get("target")
                    a_channel.sampler = s
                    animation_channels.append(a_channel)


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
            helpers.write_parsable_string(output, bone[0])
            num_sources = 0
            for src in channel.sampler.sources:
                semantic_index = animation_source_semantics.index(src.semantic)
                if semantic_index < 2:
                    num_sources += 1
            output.write(struct.pack("i", num_sources))
            for src in channel.sampler.sources:
                semantic_index = animation_source_semantics.index(src.semantic)
                if semantic_index < 2:
                    output.write(struct.pack("i", semantic_index))
                    type_index = animation_source_types.index(src.type)
                    output.write(struct.pack("i", type_index))
                    output.write(struct.pack("i", len(src.data)))
                    if src.type == "float":
                        for f in range(0, len(src.data)):
                            output.write(struct.pack("f", f))
                    elif src.type == "float4x4":
                        for f in range(0, len(src.data), 16):
                            helpers.write_corrected_4x4matrix(output, src.data[f:f+16])