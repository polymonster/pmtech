#include "ecs/ecs_editor.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "volume_generator.h"

#include "camera.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "loader.h"
#include "pmfx.h"

#include "file_system.h"
#include "hash.h"
#include "input.h"
#include "pen.h"
#include "pen_json.h"
#include "pen_string.h"
#include "renderer.h"
#include "str_utilities.h"
#include "timer.h"

#include "forward_render.h"
#include "maths/vec.h"

using namespace put;
using namespace put::ecs;

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

void create_scene(ecs_scene* scene)
{
    clear_scene(scene);

    // add light
    u32 light = get_new_node(scene);
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
}

void shadow_map_update(put::scene_controller* sc)
{
    put::camera_update_shadow_frustum(sc->camera, -sc->scene->lights[0].direction, sc->scene->renderable_extents.min,
                                      sc->scene->renderable_extents.max);

    mat4 shadow_vp = sc->camera->proj * sc->camera->view;

    static u32 cbuffer = PEN_INVALID_HANDLE;
    if (!is_valid(cbuffer))
    {
        pen::buffer_creation_params bcp;
        bcp.usage_flags = PEN_USAGE_DYNAMIC;
        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
        bcp.buffer_size = sizeof(mat4);
        bcp.data = nullptr;

        cbuffer = pen::renderer_create_buffer(bcp);
    }

    pen::renderer_update_buffer(cbuffer, &shadow_vp, sizeof(mat4));

    // unbind
    pen::renderer_set_texture(0, 0, 15, pen::TEXTURE_BIND_PS);
    pen::renderer_set_constant_buffer(cbuffer, 4, pen::CBUFFER_BIND_PS);
}

PEN_TRV pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::semaphore_post(p_thread_info->p_sem_continue, 1);

    pen::jobs_create_job(physics::physics_thread_main, 1024 * 10, nullptr, pen::THREAD_START_DETACHED);

    put::dev_ui::init();
    put::dbg::init();

    // create main camera and controller
    put::camera main_camera;
    put::camera_create_perspective(&main_camera, 60.0f, put::k_use_window_aspect, 0.1f, 1000.0f);

    put::scene_controller cc;
    cc.camera = &main_camera;
    cc.update_function = &ecs::update_model_viewer_camera;
    cc.name = "model_viewer_camera";
    cc.id_name = PEN_HASH(cc.name.c_str());

    // create the main scene and controller
    put::ecs::ecs_scene* main_scene;
    main_scene = put::ecs::create_scene("main_scene");
    put::ecs::editor_init(main_scene);

    put::scene_controller sc;
    sc.scene = main_scene;
    sc.update_function = &ecs::update_model_viewer_scene;
    sc.name = "main_scene";
    sc.camera = &main_camera;
    sc.id_name = PEN_HASH(sc.name.c_str());

    // create view renderers
    put::scene_view_renderer svr_main;
    svr_main.name = "ces_render_scene";
    svr_main.id_name = PEN_HASH(svr_main.name.c_str());
    svr_main.render_function = &ecs::render_scene_view;

    put::scene_view_renderer svr_editor;
    svr_editor.name = "ces_render_editor";
    svr_editor.id_name = PEN_HASH(svr_editor.name.c_str());
    svr_editor.render_function = &ecs::render_scene_editor;

    put::camera           shadow_camera;
    put::scene_controller shadow_cc;
    shadow_cc.scene = main_scene;
    shadow_cc.camera = &shadow_camera;
    shadow_cc.update_function = &shadow_map_update;
    shadow_cc.name = "shadow_camera";
    shadow_cc.id_name = PEN_HASH(shadow_cc.name.c_str());

    pmfx::register_scene_view_renderer(svr_main);
    pmfx::register_scene_view_renderer(svr_editor);

    pmfx::register_scene_controller(shadow_cc);
    pmfx::register_scene_controller(sc);
    pmfx::register_scene_controller(cc);

    pmfx::init("data/configs/sss_demo.jsn");
    create_scene(main_scene);

    f32 frame_time = 0.0f;

    // focus on the head
    main_camera.focus = vec3f(0.0f, 8.5f, 0.0f);
    main_camera.zoom = 20.0f;

    while (1)
    {
        static u32 frame_timer = pen::timer_create("frame_timer");
        pen::timer_start(frame_timer);

        put::dev_ui::new_frame();

        pmfx::update();

        pmfx::render();

        pmfx::show_dev_ui();

        put::dev_ui::render();

        frame_time = pen::timer_elapsed_ms(frame_timer);

        // rotate light
        cmp_light& snl = main_scene->lights[0];
        snl.azimuth += frame_timer * 0.02f;
        snl.altitude = maths::deg_to_rad(108.0f);
        snl.direction = maths::azimuth_altitude_to_xyz(snl.azimuth, snl.altitude);

        main_camera.pos += vec3f(1.0, 0.0, 0.0);
        main_camera.flags |= CF_INVALIDATED;

        pen::renderer_present();

        // for unit test
        pen::renderer_test_run();

        pen::renderer_consume_cmd_buffer();

        put::vgt::post_update();
        pmfx::poll_for_changes();
        put::poll_hot_loader();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(p_thread_info->p_sem_exit))
            break;
    }

    ecs::destroy_scene(main_scene);
    ecs::editor_shutdown();

    // clean up mem here
    put::pmfx::shutdown();
    put::dbg::shutdown();
    put::dev_ui::shutdown();

    pen::renderer_consume_cmd_buffer();

    // signal to the engine the thread has finished
    pen::semaphore_post(p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}
