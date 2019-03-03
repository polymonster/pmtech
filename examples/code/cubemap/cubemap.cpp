#include "camera.h"
#include "ecs/ecs_editor.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "file_system.h"
#include "hash.h"
#include "input.h"
#include "loader.h"
#include "pen.h"
#include "pen_json.h"
#include "pen_string.h"
#include "pmfx.h"
#include "renderer.h"
#include "str_utilities.h"
#include "timer.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,     // width
    720,      // height
    4,        // MSAA samples
    "cubemap" // window title / process name
};

namespace physics
{
    extern PEN_TRV physics_thread_main(void* params);
}

void create_scene_objects(ecs::ecs_scene* scene)
{
    clear_scene(scene);

    // create material for cubemap
    material_resource* cubemap_material = new material_resource;
    cubemap_material->material_name = "volume_material";
    cubemap_material->shader_name = "pmfx_utility";
    cubemap_material->id_shader = PEN_HASH("pmfx_utility");
    cubemap_material->id_technique = PEN_HASH("cubemap");
    add_material_resource(cubemap_material);

    geometry_resource* sphere = get_geometry_resource(PEN_HASH("sphere"));

    u32 new_prim = get_new_node(scene);
    scene->names[new_prim] = "sphere";
    scene->names[new_prim].appendf("%i", new_prim);
    scene->transforms[new_prim].rotation = quat();
    scene->transforms[new_prim].scale = vec3f(10.0f);
    scene->transforms[new_prim].translation = vec3f::zero();
    scene->entities[new_prim] |= CMP_TRANSFORM;
    scene->parents[new_prim] = new_prim;
    instantiate_geometry(sphere, scene, new_prim);
    instantiate_material(cubemap_material, scene, new_prim);
    instantiate_model_cbuffer(scene, new_prim);

    scene->samplers[new_prim].sb[0].handle = put::load_texture("data/textures/cubemap.dds");
    scene->samplers[new_prim].sb[0].sampler_unit = 3;
    scene->samplers[new_prim].sb[0].sampler_state = pmfx::get_render_state(PEN_HASH("clamp_linear"), pmfx::RS_SAMPLER);
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
    put::ecs::ecs_scene* main_scene = put::ecs::create_scene("main_scene");
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

    pmfx::register_scene_view_renderer(svr_main);

    pmfx::register_scene_controller(sc);
    pmfx::register_scene_controller(cc);

    pmfx::init("data/configs/basic_renderer.jsn");

    create_scene_objects(main_scene);

    f32 frame_time = 0.0f;

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

        pen::renderer_present();

        // for unit test
        pen::renderer_test_run();

        pen::renderer_consume_cmd_buffer();

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
