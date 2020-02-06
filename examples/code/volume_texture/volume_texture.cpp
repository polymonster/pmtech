#include "../example_common.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,            // width
    720,             // height
    4,               // MSAA samples
    "volume_texture" // window title / process name
};

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    clear_scene(scene);

    geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

    // create a simple 3d texture
    u32 block_size = 4;
    u32 volume_dimension = 64;
    u32 data_size = volume_dimension * volume_dimension * volume_dimension * block_size;

    u8* volume_data = (u8*)pen::memory_alloc(data_size);
    u32 row_pitch = volume_dimension * block_size;
    u32 slice_pitch = volume_dimension * row_pitch;

    for (u32 z = 0; z < volume_dimension; ++z)
    {
        for (u32 y = 0; y < volume_dimension; ++y)
        {
            for (u32 x = 0; x < volume_dimension; ++x)
            {
                u32 offset = z * slice_pitch + y * row_pitch + x * block_size;

                u8 r, g, b, a;
                r = 255;
                g = 255;
                b = 255;
                a = 255;

                u32 black = 0;
                if (x < volume_dimension / 3 || x > volume_dimension - volume_dimension / 3)
                    black++;

                if (y < volume_dimension / 3 || y > volume_dimension - volume_dimension / 3)
                    black++;

                if (z < volume_dimension / 3 || z > volume_dimension - volume_dimension / 3)
                    black++;

                if (black == 2)
                {
                    a = 0;
                }

                volume_data[offset + 0] = b;
                volume_data[offset + 1] = g;
                volume_data[offset + 2] = r;
                volume_data[offset + 3] = a;
            }
        }
    }

    pen::texture_creation_params tcp;
    tcp.collection_type = pen::TEXTURE_COLLECTION_VOLUME;

    tcp.width = volume_dimension;
    tcp.height = volume_dimension;
    tcp.format = PEN_TEX_FORMAT_BGRA8_UNORM;
    tcp.num_mips = 1;
    tcp.num_arrays = volume_dimension;
    tcp.sample_count = 1;
    tcp.sample_quality = 0;
    tcp.usage = PEN_USAGE_DEFAULT;
    tcp.bind_flags = PEN_BIND_SHADER_RESOURCE;
    tcp.cpu_access_flags = 0;
    tcp.flags = 0;
    tcp.block_size = block_size;
    tcp.pixels_per_block = 1;
    tcp.data = volume_data;
    tcp.data_size = data_size;

    u32 volume_texture = pen::renderer_create_texture(tcp);

    // create material for volume ray trace
    material_resource* volume_material = new material_resource;
    volume_material->material_name = "volume_material";
    volume_material->shader_name = "pmfx_utility";
    volume_material->id_shader = PEN_HASH("pmfx_utility");
    volume_material->id_technique = PEN_HASH("volume_texture");
    add_material_resource(volume_material);

    // create scene node
    u32 new_prim = get_new_entity(scene);
    scene->names[new_prim] = "sphere";
    scene->names[new_prim].appendf("%i", new_prim);
    scene->transforms[new_prim].rotation = quat();
    scene->transforms[new_prim].scale = vec3f(10.0f);
    scene->transforms[new_prim].translation = vec3f::zero();
    scene->entities[new_prim] |= e_cmp::transform;
    scene->parents[new_prim] = new_prim;
    scene->samplers[new_prim].sb[0].handle = volume_texture;
    scene->samplers[new_prim].sb[0].sampler_unit = e_texture::volume;
    scene->samplers[new_prim].sb[0].sampler_state = pmfx::get_render_state(PEN_HASH("clamp_point"), pmfx::e_render_state::sampler);

    instantiate_geometry(cube, scene, new_prim);
    instantiate_material(volume_material, scene, new_prim);
    instantiate_model_cbuffer(scene, new_prim);

    bake_material_handles();
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
