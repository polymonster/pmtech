#include "../example_common.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,      // width
    720,       // height
    4,         // MSAA samples
    "skinning" // window title / process name
};

void example_setup(ecs::ecs_scene* scene)
{
    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));

    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));

    // add light
    u32 light = get_new_node(scene);
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = LIGHT_TYPE_DIR;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;

    // ground
    u32 ground = get_new_node(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].translation = vec3f::zero();
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(50.0f, 1.0f, 50.0f);
    scene->entities[ground] |= CMP_TRANSFORM;
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
    scene->entities[skinned_char] |= CMP_TRANSFORM;

    // instantiate anim controller
    instantiate_anim_controller(scene, skinned_char);

    // load an animation
    anim_handle ah = load_pma("data/models/characters/testcharacter/anims/testcharacter_idle.pma");
    bind_animation_to_rig(scene, ah, skinned_char);

    scene->anim_controller[skinned_char].current_frame = 0;
    scene->anim_controller[skinned_char].current_time = 1.0f;
    scene->anim_controller[skinned_char].current_animation = ah;
    scene->anim_controller[skinned_char].play_flags = cmp_anim_controller::PLAY;

    scene->anim_controller_v2[skinned_char].blend.anim_a = 0;
    scene->anim_controller_v2[skinned_char].blend.anim_b = 0;
    scene->anim_controller_v2[skinned_char].blend.ratio = 0.0f;
}

void example_update(ecs::ecs_scene* scene)
{

}