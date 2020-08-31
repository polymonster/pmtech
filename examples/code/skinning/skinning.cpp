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
        p.window_title = "skinning";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

void example_setup(ecs_scene* scene, camera& cam)
{
    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));

    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));

    // add light
    u32 light = get_new_entity(scene);
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = e_light_type::dir;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

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

    // load an animation
    anim_handle ah = load_pma("data/models/characters/testcharacter/anims/testcharacter_idle.pma");
    bind_animation_to_rig(scene, ah, skinned_char);

    scene->anim_controller_v2[skinned_char].blend.anim_a = 0;
    scene->anim_controller_v2[skinned_char].blend.anim_b = 0;
    scene->anim_controller_v2[skinned_char].blend.ratio = 0.0f;
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
