#include "../example_common.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,               // width
    720,                // height
    4,                  // MSAA samples
    "vertex_stream_out" // window title / process name
};

void example_setup(ecs_scene* scene, camera& cam)
{
    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));

    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));

    // add lights
    vec3f light_cols[] = {vec3f(143.0f, 45.0f, 86.0f) / 255.0f, vec3f(255.0f, 149.0f, 0.0f) / 255.0f,
                          vec3f(255.0f, 102.0f, 0.0f) / 255.0f, vec3f(216.0f, 17.0f, 89.0f) / 255.0f};

    vec3f light_pos[] = {vec3f(-50.0f, 20.0f, -50.0f), vec3f(50.0f, 20.0f, -50.0f), vec3f(50.0f, 20.0f, 50.0f),
                         vec3f(-50.0f, 20.0f, 50.0f)};

    for (u32 l = 0; l < 4; ++l)
    {
        u32 light = get_new_entity(scene);
        scene->names[light] = "front_light";
        scene->id_name[light] = PEN_HASH("front_light");
        scene->lights[light].colour = light_cols[l];
        scene->lights[light].direction = vec3f::one();
        scene->lights[light].radius = 70.0f;
        scene->lights[light].type = e_light_type::point;
        scene->transforms[light].translation = light_pos[l];
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
    }

    // ground
    u32 ground = get_new_entity(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].translation = vec3f::zero();
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(50.0f, 1.0f, 50.0f);
    scene->entities[ground] |= e_cmp::transform;
    scene->parents[ground] = ground;
    instantiate_geometry(box, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);

    // load a skinned character
    u32 skinned_char = load_pmm("data/models/characters/testcharacter/testcharacter.pmm", scene);
    PEN_ASSERT(is_valid(skinned_char));

    // set character scale and pos
    scene->transforms[skinned_char].translation = vec3f(0.0f, 1.0f, 0.0f);
    scene->transforms[skinned_char].scale = vec3f(0.25f);
    scene->entities[skinned_char] |= e_cmp::transform;

    instantiate_model_pre_skin(scene, skinned_char);

    // load an animation
    anim_handle ah = load_pma("data/models/characters/testcharacter/anims/testcharacter_idle.pma");
    bind_animation_to_rig(scene, ah, skinned_char);

    // remove the geometry flag from the skinned character as we just want to use it as vertex stream out
    scene->entities[skinned_char] &= ~e_cmp::geometry;

    // in order to instance stuff we must have a contiguous list of nodes.
    // this node aliases the geometry and materials from the skinned_char root node
    u32 master_node = get_new_entity(scene);
    scene->names[master_node] = "master skinned instance";
    scene->transforms[master_node].translation = vec3f::zero();
    scene->transforms[master_node].rotation = quat();
    scene->transforms[master_node].scale = vec3f::one();
    scene->parents[master_node] = master_node;

    scene->entities[master_node] |= (e_cmp::transform | e_cmp::geometry | e_cmp::material);

    scene->geometries[master_node] = scene->geometries[skinned_char];
    scene->materials[master_node] = scene->materials[skinned_char];
    scene->material_resources[master_node] = scene->material_resources[skinned_char];
    scene->cbuffer[master_node] = scene->cbuffer[skinned_char];

    s32 num = 20;

    f32 spacing = 20.0f;
    f32 start = (spacing * (num - 1)) * 0.5f;

    vec3f start_pos = vec3f(-start, 0, -start);

    vec3f cur_pos = start_pos;

    u32 total = 20 * 20;

    f32 roughness = 0.0f;
    f32 roughness_step = 1.0f / (f32)total;

    for (s32 i = 0; i < num; ++i)
    {
        cur_pos.x = start_pos.x;

        for (s32 j = 0; j < num; ++j)
        {
            u32 new_prim = get_new_entity(scene);
            scene->names[new_prim] = "skinned instance";
            scene->names[new_prim].appendf("%i", new_prim);

            scene->transforms[new_prim].rotation = quat();
            scene->transforms[new_prim].scale = vec3f::one();
            scene->transforms[new_prim].translation = cur_pos;
            scene->parents[new_prim] = skinned_char;

            scene->entities[new_prim] |= e_cmp::transform;
            scene->entities[new_prim] |= e_cmp::geometry;
            scene->entities[new_prim] |= e_cmp::material;
            scene->entities[new_prim] |= e_cmp::sub_instance;

            scene->draw_call_data[new_prim].v2 = vec4f(0.5f, 0.5f, 0.5f, 1.0f - roughness);

            roughness += roughness_step;
            cur_pos.x += spacing;
        }

        cur_pos.z += spacing;
    }

    instance_entity_range(scene, master_node, pow(num, 2));

    bake_material_handles();
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
