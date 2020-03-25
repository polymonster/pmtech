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

#include "maths/vec.h"

#define CR_HOST // required in the host only and before including cr.h
#include "cr/cr.h"
#include "ecs/ecs_live.h"

using namespace put;
using namespace put::ecs;

void* pen::user_entry(void* params);

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "pmtech_editor";
        p.window_sample_count = 4;
        p.user_thread_function = user_entry;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace physics
{
    extern void* physics_thread_main(void* params);
}

void* pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::semaphore_post(p_thread_info->p_sem_continue, 1);

    pen::jobs_create_job(physics::physics_thread_main, 1024 * 10, nullptr, pen::e_thread_start_flags::detached);

    // create the main scene and camera
    put::ecs::ecs_scene* main_scene = put::ecs::create_scene("main_scene");

    put::camera main_camera;
    put::camera_create_perspective(&main_camera, 60.0f, put::k_use_window_aspect, 0.1f, 1000.0f);

    // init systems
    put::dev_ui::init();
    put::dbg::init();
    put::ecs::editor_init(main_scene, &main_camera);

    // create view renderers
    put::scene_view_renderer svr_main;
    svr_main.name = "ces_render_scene";
    svr_main.id_name = PEN_HASH(svr_main.name.c_str());
    svr_main.render_function = &ecs::render_scene_view;

    put::scene_view_renderer svr_light_volumes;
    svr_light_volumes.name = "ces_render_light_volumes";
    svr_light_volumes.id_name = PEN_HASH(svr_light_volumes.name.c_str());
    svr_light_volumes.render_function = &ecs::render_light_volumes;

    put::scene_view_renderer svr_editor;
    svr_editor.name = "ces_render_editor";
    svr_editor.id_name = PEN_HASH(svr_editor.name.c_str());
    svr_editor.render_function = &ecs::render_scene_editor;

    put::scene_view_renderer svr_shadow_maps;
    svr_shadow_maps.name = "ces_render_shadow_maps";
    svr_shadow_maps.id_name = PEN_HASH(svr_shadow_maps.name.c_str());
    svr_shadow_maps.render_function = &ecs::render_shadow_views;

    put::scene_view_renderer svr_area_light_textures;
    svr_area_light_textures.name = "ces_render_area_light_textures";
    svr_area_light_textures.id_name = PEN_HASH(svr_area_light_textures.name.c_str());
    svr_area_light_textures.render_function = &ecs::render_area_light_textures;

    put::scene_view_renderer svr_omni_shadow_maps;
    svr_omni_shadow_maps.name = "ces_render_omni_shadow_maps";
    svr_omni_shadow_maps.id_name = PEN_HASH(svr_omni_shadow_maps.name.c_str());
    svr_omni_shadow_maps.render_function = &ecs::render_omni_shadow_views;

    pmfx::register_scene_view_renderer(svr_light_volumes);
    pmfx::register_scene_view_renderer(svr_main);
    pmfx::register_scene_view_renderer(svr_editor);
    pmfx::register_scene_view_renderer(svr_shadow_maps);
    pmfx::register_scene_view_renderer(svr_omni_shadow_maps);
    pmfx::register_scene_view_renderer(svr_area_light_textures);

    pmfx::register_scene(main_scene, "main_scene");
    pmfx::register_camera(&main_camera, "model_viewer_camera");

    pmfx::init("data/configs/editor_renderer.jsn");
    put::init_hot_loader();
        
    // cr
    cr_plugin ctx;
    bool live_lib = cr_plugin_open(ctx, "liblive_lib_d.dylib");
    live_context* lc = nullptr;
    if(live_lib)
    {
        lc = new live_context;
        lc->scene = main_scene;
        lc->render = pen::renderer_get_main_context();
        lc->dbg_funcs = new dbg::__dbg;
        lc->ecs_funcs = new ecs::__ecs;
        put::dbg::generate_bindings(lc->dbg_funcs);
        put::ecs::generate_bindings(lc->ecs_funcs);
    }

    f32 dt = 0.0f;
    pen::timer* frame_timer = pen::timer_create();
    pen::timer_start(frame_timer);

    while (1)
    {
        dt = pen::timer_elapsed_ms(frame_timer)/1000.0f;
        pen::timer_start(frame_timer);

        put::dev_ui::new_frame();

        ecs::update(dt);
        
        if(live_lib)
        {
            ctx.userdata = (void*)lc;
            cr_plugin_update(ctx);
        }

        pmfx::render();

        pmfx::show_dev_ui();
        put::vgt::show_dev_ui();
        put::dev_ui::render();

        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        put::vgt::post_update();
        pmfx::poll_for_changes();
        put::poll_hot_loader();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(p_thread_info->p_sem_exit))
            break;
    }
    
    if(live_lib)
    {
        cr_plugin_close(ctx);
        delete lc;
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
