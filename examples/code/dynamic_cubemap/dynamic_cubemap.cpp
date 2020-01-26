#include "../example_common.h"
#include "forward_render.h"
#include "maths/vec.h"

using namespace put;
using namespace ecs;
using namespace forward_render;

pen::window_creation_params pen_window{
    1280,             // width
    720,              // height
    4,                // MSAA samples
    "dynamic_cubemap" // window title / process name
};

namespace
{
    f32 zpos[] = {3.0f, -3.0f};

    s32 chorme_ball = 0;
    s32 chrome2_ball = 0;

    put::camera chrome_camera;
    put::camera chrome2_camera;

} // namespace

void example_setup(ecs_scene* scene, camera& cam)
{
    // chrome cameras

    put::camera_create_cubemap(&chrome_camera, 0.01f, 1000.0f);
    chrome_camera.pos = vec3f(0.0f, 0.0f, 0.0f);

    put::camera_create_cubemap(&chrome2_camera, 0.01f, 1000.0f);
    chrome2_camera.pos = vec3f(0.0f, 0.0f, 0.0f);

    pmfx::register_camera(&chrome_camera, "chrome_camera");
    pmfx::register_camera(&chrome2_camera, "chrome2_camera");

    pmfx::init("data/configs/dynamic_cubemap.jsn");

    // setup scene
    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));

    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));
    geometry_resource* sphere = get_geometry_resource(PEN_HASH("sphere"));

    // front light
    u32 light = get_new_entity(scene);
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

    // back light
    light = get_new_entity(scene);
    scene->names[light] = "back_light";
    scene->id_name[light] = PEN_HASH("back_light");
    scene->lights[light].colour = vec3f(0.6f, 0.6f, 0.6f);
    scene->lights[light].direction = -vec3f::one();
    scene->lights[light].type = LIGHT_TYPE_DIR;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;

    // ground
    u32 ground = get_new_entity(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].translation = vec3f(0.0f, -2.0f, 0.0f);
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(50.0f, 1.0f, 20.0f);
    scene->entities[ground] |= CMP_TRANSFORM;
    scene->parents[ground] = ground;
    instantiate_geometry(box, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);
    scene->physics_data[ground].rigid_body.shape = physics::BOX;
    scene->physics_data[ground].rigid_body.mass = 0.0f;
    instantiate_rigid_body(scene, ground);

    scene->material_permutation[ground] |= FORWARD_LIT_UV_SCALE;

    forward_lit_uv_scale* fluvs = (forward_lit_uv_scale*)&scene->material_data[ground].data;
    fluvs->m_uv_scale = vec2f(0.125f) * 0.5f;

    scene->samplers[ground].sb[0].handle = put::load_texture("data/textures/BlueChecker01.dds");
    scene->samplers[ground].sb[0].sampler_unit = 0;
    scene->samplers[ground].sb[0].sampler_state = pmfx::get_render_state(PEN_HASH("clamp_linear"), pmfx::e_render_state::sampler);
    bake_material_handles(scene, ground);

    f32 ramp1_angle[] = {(f32)M_PI * 0.125f, (f32)M_PI * -0.125f};
    f32 ramp2_angle[] = { (f32)M_PI * -0.125f, (f32)M_PI * 0.125f};
    f32 ramp1_x[] = {20.0f, -20.0f};
    f32 ramp2_x[] = {-10.0f, 10.0f};
    f32 block1_x[] = {30.0f, -30.0f};
    f32 block2_x[] = {10.0f, -10.0f};

    for (u32 i = 0; i < 2; ++i)
    {
        // ramp1
        quat ramp_rot;
        ramp_rot.euler_angles(ramp1_angle[i], 0.0f, 0.0f);
        u32 ramp = get_new_entity(scene);
        scene->names[ramp] = "ramp";
        scene->transforms[ramp].translation = vec3f(ramp1_x[i], 25.0f, zpos[i]);
        scene->transforms[ramp].rotation = ramp_rot;
        scene->transforms[ramp].scale = vec3f(20.0f, 1.0f, 2.0f);
        scene->entities[ramp] |= CMP_TRANSFORM;
        scene->parents[ramp] = ramp;
        instantiate_geometry(box, scene, ramp);
        instantiate_material(default_material, scene, ramp);
        instantiate_model_cbuffer(scene, ramp);
        scene->physics_data[ramp].rigid_body.shape = physics::BOX;
        scene->physics_data[ramp].rigid_body.mass = 0.0f;
        instantiate_rigid_body(scene, ramp);

        // ramp2
        ramp_rot.euler_angles(ramp2_angle[i], 0.0f, 0.0f);
        ramp = get_new_entity(scene);
        scene->names[ramp] = "ramp";
        scene->transforms[ramp].translation = vec3f(ramp2_x[i], 10.0f, zpos[i]);
        scene->transforms[ramp].rotation = ramp_rot;
        scene->transforms[ramp].scale = vec3f(20.0f, 1.0f, 2.0f);
        scene->entities[ramp] |= CMP_TRANSFORM;
        scene->parents[ramp] = ramp;
        instantiate_geometry(box, scene, ramp);
        instantiate_material(default_material, scene, ramp);
        instantiate_model_cbuffer(scene, ramp);
        scene->physics_data[ramp].rigid_body.shape = physics::BOX;
        scene->physics_data[ramp].rigid_body.mass = 0.0f;
        instantiate_rigid_body(scene, ramp);

        // end block
        u32 block = get_new_entity(scene);
        scene->names[block] = "block";
        scene->transforms[block].translation = vec3f(block1_x[i], 1.0f, zpos[i]);
        scene->transforms[block].rotation = quat();
        scene->transforms[block].scale = vec3f(2.0f, 2.0f, 2.0f);
        scene->entities[block] |= CMP_TRANSFORM;
        scene->parents[block] = block;
        instantiate_geometry(box, scene, block);
        instantiate_material(default_material, scene, block);
        instantiate_model_cbuffer(scene, block);
        scene->physics_data[block].rigid_body.shape = physics::BOX;
        scene->physics_data[block].rigid_body.mass = 0.0f;
        instantiate_rigid_body(scene, block);

        // back block
        block = get_new_entity(scene);
        scene->names[block] = "block";
        scene->transforms[block].translation = vec3f(block2_x[i], 0.0f, zpos[i]);
        scene->transforms[block].rotation = quat();
        scene->transforms[block].scale = vec3f(1.0f, 1.0f, 1.0f);
        scene->entities[block] |= CMP_TRANSFORM;
        scene->parents[block] = block;
        instantiate_geometry(box, scene, block);
        instantiate_material(default_material, scene, block);
        instantiate_model_cbuffer(scene, block);
        scene->physics_data[block].rigid_body.shape = physics::BOX;
        scene->physics_data[block].rigid_body.mass = 0.0f;
        instantiate_rigid_body(scene, block);
    }

    vec4f cols[] = {vec4f(1.0f, 0.1f, 0.1f, 1.0f), vec4f(0.1f, 1.0f, 0.3f, 1.0f), vec4f(0.2f, 0.1f, 1.0f, 1.0f),

                    vec4f(1.0f, 0.2f, 1.0f, 1.0f), vec4f(0.2f, 1.0f, 1.0f, 1.0f), vec4f(1.0f, 0.6f, 0.1f, 1.0f),

                    vec4f(0.5f, 0.1f, 1.0f, 1.0f), vec4f(0.5f, 1.0f, 0.2f, 1.0f)};

    // assign colours
    u32 col_index = 0;
    for (u32 i = 3; i < 11; ++i)
    {
        forward_lit* fl = (forward_lit*)&scene->material_data[i].data;
        fl->m_albedo = cols[col_index];
        col_index++;
    }

    // chrome ball
    chorme_ball = get_new_entity(scene);
    scene->names[chorme_ball] = "chrome_ball";
    scene->transforms[chorme_ball].rotation = quat();
    scene->transforms[chorme_ball].scale = vec3f(2.0f, 2.0f, 2.0f);
    scene->transforms[chorme_ball].translation = vec3f(30.0f, 30.0f, zpos[0]);
    scene->entities[chorme_ball] |= CMP_TRANSFORM;
    scene->parents[chorme_ball] = chorme_ball;

    instantiate_geometry(sphere, scene, chorme_ball);
    instantiate_model_cbuffer(scene, chorme_ball);
    instantiate_material(default_material, scene, chorme_ball);

    u32 chrome_cubemap_handle = pmfx::get_render_target(PEN_HASH("chrome"))->handle;

    // set material to cubemap
    scene->material_resources[chorme_ball].id_technique = PEN_HASH("cubemap");
    scene->material_resources[chorme_ball].shader_name = "pmfx_utility";
    scene->material_resources[chorme_ball].id_shader = PEN_HASH("pmfx_utility");

    scene->state_flags[chorme_ball] &= ~SF_SAMPLERS_INITIALISED;
    bake_material_handles(scene, chorme_ball);

    for (u32 s = 1; s < e_pmfx_constants::max_technique_sampler_bindings; ++s)
        scene->samplers[chorme_ball].sb[s].handle = 0;

    scene->samplers[chorme_ball].sb[0].handle = chrome_cubemap_handle;
    scene->samplers[chorme_ball].sb[0].sampler_unit = 3;
    scene->samplers[chorme_ball].sb[0].sampler_state = pmfx::get_render_state(PEN_HASH("clamp_linear"), pmfx::e_render_state::sampler);

    // add physics
    scene->physics_data[chorme_ball].rigid_body.shape = physics::SPHERE;
    scene->physics_data[chorme_ball].rigid_body.mass = 1.0f;
    instantiate_rigid_body(scene, chorme_ball);

    // chrome2 ball
    chrome2_ball = get_new_entity(scene);
    scene->names[chrome2_ball] = "chrome2_ball";
    scene->transforms[chrome2_ball].rotation = quat();
    scene->transforms[chrome2_ball].scale = vec3f(2.0f, 2.0f, 2.0f);
    scene->transforms[chrome2_ball].translation = vec3f(-30.0f, 30.0f, zpos[1]);
    scene->entities[chrome2_ball] |= CMP_TRANSFORM;
    scene->parents[chrome2_ball] = chrome2_ball;

    instantiate_geometry(sphere, scene, chrome2_ball);
    instantiate_model_cbuffer(scene, chrome2_ball);
    instantiate_material(default_material, scene, chrome2_ball);

    u32 chrome2_cubemap_handle = pmfx::get_render_target(PEN_HASH("chrome2"))->handle;

    // set material to cubemap
    scene->material_resources[chrome2_ball].id_technique = PEN_HASH("cubemap");
    scene->material_resources[chrome2_ball].shader_name = "pmfx_utility";
    scene->material_resources[chrome2_ball].id_shader = PEN_HASH("pmfx_utility");
    scene->state_flags[chrome2_ball] &= ~SF_SAMPLERS_INITIALISED;

    bake_material_handles(scene, chrome2_ball);

    scene->samplers[chrome2_ball].sb[0].handle = chrome2_cubemap_handle;
    scene->samplers[chrome2_ball].sb[0].sampler_unit = 3;
    scene->samplers[chrome2_ball].sb[0].sampler_state = pmfx::get_render_state(PEN_HASH("clamp_linear"), pmfx::e_render_state::sampler);

    for (u32 s = 1; s < e_pmfx_constants::max_technique_sampler_bindings; ++s)
        scene->samplers[chrome2_ball].sb[s].handle = 0;

    // add physics
    scene->physics_data[chrome2_ball].rigid_body.shape = physics::SPHERE;
    scene->physics_data[chrome2_ball].rigid_body.mass = 1.0f;
    instantiate_rigid_body(scene, chrome2_ball);

    // load physics stuff before calling update
    physics::physics_consume_command_buffer();
    pen::thread_sleep_ms(16);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    chrome_camera.pos = scene->world_matrices[chorme_ball].get_translation();
    chrome2_camera.pos = scene->world_matrices[chrome2_ball].get_translation();

    // reset
    static bool debounce = false;
    if (pen::input_key(PK_R))
    {
        if (!debounce)
        {
            scene->transforms[chorme_ball].translation = vec3f(30.0f, 30.0f, zpos[0]);
            scene->transforms[chrome2_ball].translation = vec3f(-30.0f, 30.0f, zpos[1]);

            scene->entities[chorme_ball] |= CMP_TRANSFORM;
            scene->entities[chrome2_ball] |= CMP_TRANSFORM;

            debounce = true;
        }
    }
    else
    {
        debounce = false;
    }
}
