#include "../example_common.h"

#include "forward_render.h"
#include "maths/vec.h"

using namespace put;
using namespace put::ecs;
using namespace forward_render;

pen::window_creation_params pen_window{
    1280, // width
    720,  // height
    4,    // MSAA samples
    "sss" // window title / process name
};

namespace physics
{
    extern PEN_TRV physics_thread_main(void* params);
}

void example_setup(ecs_scene* scene, camera& cam)
{
    pmfx::init("data/configs/sss_demo.jsn");

    clear_scene(scene);

    // add light
    u32 light = get_new_entity(scene);
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

    // load head model
    u32 head_model = load_pmm("data/models/head_smooth.pmm", scene) + 1; // node 0 in the model is environment ambient light
    PEN_ASSERT(is_valid(head_model));

    // set character scale and pos
    scene->transforms[head_model].translation = vec3f(0.0f, 0.0f, 0.0f);
    scene->transforms[head_model].scale = vec3f(10.0f);
    scene->entities[head_model] |= CMP_TRANSFORM;

    // set textures
    scene->samplers[head_model].sb[0].handle = put::load_texture("data/textures/head/albedo.dds");
    scene->samplers[head_model].sb[0].sampler_unit = 0;

    scene->samplers[head_model].sb[1].handle = put::load_texture("data/textures/head/normal.dds");
    scene->samplers[head_model].sb[1].sampler_unit = 1;

    // set material to sss
    scene->material_resources[head_model].id_technique = PEN_HASH("forward_lit");
    scene->material_resources[head_model].shader_name = "forward_render";
    scene->material_resources[head_model].id_shader = PEN_HASH(scene->material_resources[head_model].shader_name);

    scene->material_permutation[head_model] |= FORWARD_LIT_SSS;

    forward_lit_sss mat_data;
    mat_data.m_albedo = float4::one();
    mat_data.m_roughness = 0.5f;
    mat_data.m_reflectivity = 0.22f;
    mat_data.m_sss_scale = 370.0f;

    memcpy(scene->material_data[head_model].data, &mat_data, sizeof(forward_lit_sss));

    bake_material_handles();

    cam.focus = vec3f(0.0, 10.0, 0.0);
    cam.zoom = 27.0f;
    cam.flags |= e_camera_flags::invalidated;
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    // rotate light
    cmp_light& snl = scene->lights[0];
    snl.azimuth += dt * 100.0f;
    snl.altitude = maths::deg_to_rad(108.0f);
    snl.direction = maths::azimuth_altitude_to_xyz(snl.azimuth, snl.altitude);
}
