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

namespace
{
  u32 pillar_start;
}

void example_setup(ecs_scene* scene, camera& cam)
{
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
    scene->lights[light].colour = vec3f::one() * 0.3f;
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = LIGHT_TYPE_DIR;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;
    
    // point
    light = get_new_entity(scene);
    instantiate_light(scene, light);
    scene->names[light] = "point_light1";
    scene->id_name[light] = PEN_HASH("point_light1");
    scene->lights[light].colour = vec3f(1.0f, 0.0f, 0.0f);
    scene->lights[light].radius = 50.0f;
    scene->lights[light].type = LIGHT_TYPE_POINT;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f(-16.0f, 30.0f, -16.0f);
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;
    
    light = get_new_entity(scene);
    instantiate_light(scene, light);
    scene->names[light] = "point_light2";
    scene->id_name[light] = PEN_HASH("point_light2");
    scene->lights[light].colour = vec3f(0.0f, 0.0f, 1.0f);
    scene->lights[light].radius = 50.0f;
    scene->lights[light].type = LIGHT_TYPE_POINT;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f(16.0f, 30.0f, 16.0f);
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;
    
    // spot
    light = get_new_entity(scene);
    instantiate_light(scene, light);
    scene->names[light] = "spot_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f(0.0f, 1.0f, 0.5f) * 0.3f;
    scene->lights[light].cos_cutoff = 0.3f;
    scene->lights[light].radius = 30.0f; // range
    scene->lights[light].spot_falloff = 0.2f;
    scene->lights[light].type = LIGHT_TYPE_SPOT;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f(75.0f, 30.0f, -50.0f);
    scene->transforms[light].rotation = quat(-45.0f, 0.0f, 0.0f);
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;
    
    light = get_new_entity(scene);
    instantiate_light(scene, light);
    scene->names[light] = "spot_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f(1.0f, 0.5f, 0.0f) * 0.3f;
    scene->lights[light].cos_cutoff = 0.3f;
    scene->lights[light].radius = 30.0f; // range
    scene->lights[light].spot_falloff = 0.2f;
    scene->lights[light].type = LIGHT_TYPE_SPOT;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f(-75.0f, 30.0f, 50.0f);
    scene->transforms[light].rotation = quat(45.0f, 0.0f, 0.0f);
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
    
    // the old classic
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
            if(i == 0 && j == 0)
                pillar_start = pillar;
                
            scene->transforms[pillar].rotation = quat();
            scene->transforms[pillar].scale = vec3f(2.0f, pillar_size * 0.75f, 2.0f);
            scene->transforms[pillar].translation = pos;
            scene->parents[pillar] = pillar;
            scene->entities[pillar] |= CMP_TRANSFORM;
            
            // random rotation offset
            f32 x = maths::deg_to_rad(rand() % 360);
            f32 y = maths::deg_to_rad(rand() % 360);
            f32 z = maths::deg_to_rad(rand() % 360);
            scene->transforms[pillar].rotation.euler_angles(z, y, x);

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
    // animating lights
    quat q;

    static f32 lr[] = {0.0f, M_PI * 0.5f, M_PI, M_PI * 1.5f};

    // rotate directional light
    for (u32 i = 0; i < 1; ++i)
    {
        // animate lights
        f32 dirsign = 1.0f;
        if (i % 2 == 0)
            dirsign = -1.0f;

        lr[i] += (dt * 5.0f * dirsign * (i + 1));

        vec2f xz = vec2f(cos(lr[i]), sin(lr[i]));

        vec3f dir = vec3f(xz.x, 1.0f, xz.y);
        scene->lights[i].direction = dir;
        
        scene->transforms.data[i].rotation = quat(xz.x, 0.0f, xz.y);
        scene->entities.data[i] |= CMP_TRANSFORM;
    }

    static f32 t = 0.0;
    t += dt;
    f32 tx = sin(t * 0.5);
    f32 tz = cos(t * 0.5);
    scene->transforms[1].translation = vec3f(tx * 30.0f, 30.0f, tz * 30.0f);
    scene->entities[1] |= CMP_TRANSFORM;

    static f32 t2 = dt;
    t2 += dt;
    tx = sin(t2 * 0.5);
    tz = cos(t2 * 0.5);
    scene->transforms[2].translation = vec3f(tz * 30.0f, 30.0f, tx * 30.0f);
    scene->entities[2] |= CMP_TRANSFORM;
    
    // animating pillars
    q.euler_angles(0.01f, 0.01f, 0.01f);

    static pen::timer* timer = pen::timer_create();

    pen::timer_start(timer);
    for (s32 i = pillar_start; i < scene->num_entities; ++i)
    {
        scene->transforms.data[i].rotation = scene->transforms.data[i].rotation * q;
        scene->entities.data[i] |= CMP_TRANSFORM;
    }
}
