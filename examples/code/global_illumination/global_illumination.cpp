#include "../example_common.h"

using namespace put;
using namespace ecs;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "global_illumination";
        p.window_sample_count = 4;
        p.user_thread_function = user_entry;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

struct forward_lit_material
{
    vec4f albedo;
    f32   roughness;
    f32   reflectivity;
};

namespace
{
    u32 pillar_start;
}

void example_setup(ecs_scene* scene, camera& cam)
{
    //pmfx::set_view_set("editor_gi");

    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* box_resource = get_geometry_resource(PEN_HASH("cube"));

    // add lights
    u32 light;

    // directional
    light = get_new_entity(scene);
    instantiate_light(scene, light);
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = e_light_type::dir;
    scene->lights[light].flags |= e_light_flags::global_illumination;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    // add ground
    f32 ground_size = 100.0f;
    u32 ground = get_new_entity(scene);
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(ground_size, 1.0f, ground_size);
    scene->transforms[ground].translation = vec3f::zero();
    scene->parents[ground] = ground;
    scene->entities[ground] |= e_cmp::transform;

    instantiate_geometry(box_resource, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);

    forward_lit_material* m = (forward_lit_material*)&scene->material_data[ground].data[0];
    m->albedo = vec4f::one() * 0.7f;
    m->roughness = 1.0f;
    m->reflectivity = 0.0f;

    // add some pillars for shadow casters
    
    // deterministic results
    srand(10);

    // the old classic
    f32   num_pillar_rows = 3;
    f32   pillar_size = 8.0f;
    f32   d = ground_size * 0.5f;
    vec3f start_pos = vec3f(-d, pillar_size, -d);
    vec3f pos = start_pos;
    for (s32 i = 0; i < num_pillar_rows; ++i)
    {
        pos.z = start_pos.z;

        for (s32 j = 0; j < num_pillar_rows; ++j)
        {
            u32 pillar = get_new_entity(scene);
            if (i == 0 && j == 0)
                pillar_start = pillar;

            scene->transforms[pillar].rotation = quat();
            scene->transforms[pillar].scale = vec3f(8.0f, 8.0f, 8.0f);
            scene->transforms[pillar].translation = pos;
            scene->parents[pillar] = pillar;
            scene->entities[pillar] |= e_cmp::transform;

            instantiate_geometry(box_resource, scene, pillar);
            instantiate_material(default_material, scene, pillar);
            instantiate_model_cbuffer(scene, pillar);

            forward_lit_material* m = (forward_lit_material*)&scene->material_data[pillar].data[0];
            m->albedo = vec4f::one() * 0.7f;
            m->roughness = 1.0f;
            m->reflectivity = 0.0f;

            pos.z += d / 2;
        }

        pos.x += d / 2;
    }

    // add spinning pillars
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
