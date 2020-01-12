#include "../example_common.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,           // width
    720,            // height
    4,              // MSAA samples
    "shadow_maps"   // window title / process name
};

struct forward_lit_material
{
    vec4f albedo;
    f32   roughness;
    f32   reflectivity;
};

void example_setup(ecs_scene* scene, camera& cam)
{
    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* box_resource = get_geometry_resource(PEN_HASH("cube"));

    // add light
    u32 light;
    
    light = get_new_entity(scene);
    instantiate_light(scene, light);
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = LIGHT_TYPE_DIR;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;
    
    light = get_new_entity(scene);
    instantiate_light(scene, light);
    scene->names[light] = "point_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].radius = 100.0f;
    scene->lights[light].type = LIGHT_TYPE_POINT;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f(4.0f, 4.0f, 4.0f);
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;
    
    // add ground
    f32 ground_size = 100.0f;
    u32 ground = get_new_entity(scene);
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(ground_size, 1.0f, ground_size);
    scene->transforms[ground].translation = vec3f::zero();
    scene->parents[ground] = ground;
    scene->entities[ground] |= CMP_TRANSFORM;

    instantiate_geometry(box_resource, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);

    forward_lit_material* m = (forward_lit_material*)&scene->material_data[ground].data[0];
    m->albedo = vec4f::one() * 0.7f;
    m->roughness = 1.0f;
    m->reflectivity = 0.0f;

    // add some pillars for shadow casters
    f32   num_pillar_rows = 5;
    f32   pillar_size = 20.0f;
    f32   d = ground_size * 0.5f;
    vec3f start_pos = vec3f(-d, pillar_size, -d);
    vec3f pos = start_pos;
    for (s32 i = 0; i < num_pillar_rows; ++i)
    {
        pos.z = start_pos.z;

        for (s32 j = 0; j < num_pillar_rows; ++j)
        {
            u32 pillar = get_new_entity(scene);
            scene->transforms[pillar].rotation = quat();
            scene->transforms[pillar].scale = vec3f(2.0f, pillar_size, 2.0f);
            scene->transforms[pillar].translation = pos;
            scene->parents[pillar] = pillar;
            scene->entities[pillar] |= CMP_TRANSFORM;

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
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
