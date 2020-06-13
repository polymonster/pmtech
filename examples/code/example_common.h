#include "camera.h"
#include "console.h"
#include "data_struct.h"
#include "debug_render.h"
#include "dev_ui.h"
#include "ecs/ecs_editor.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "pmfx.h"

#include "file_system.h"
#include "hash.h"
#include "input.h"
#include "loader.h"
#include "pen.h"
#include "pen_json.h"
#include "pen_string.h"
#include "renderer.h"
#include "str_utilities.h"
#include "timer.h"

using namespace put;
using namespace ecs;

void example_setup(ecs::ecs_scene* scene, camera& cam);
void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt);

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
    put::init_hot_loader();
    put::dbg::init();
    
    put::ecs::init();
    put::ecs::editor_init(main_scene, &main_camera);

    pmfx::register_scene(main_scene, "main_scene");
    pmfx::register_camera(&main_camera, "model_viewer_camera");

    pmfx::init("data/configs/editor_renderer.jsn");

    // for most demos we want to start with no debug / dev stuff so they look nice, but for others we can enable to flags
    main_scene->view_flags |= e_scene_view_flags::hide_debug;
    put::dev_ui::enable(false);

    example_setup(main_scene, main_camera);

    f32 dt = 0.0f;
    pen::timer* frame_timer = pen::timer_create();
    pen::timer_start(frame_timer);

    while (1)
    {
        dt = pen::timer_elapsed_ms(frame_timer)/1000.0f;
        pen::timer_start(frame_timer);

        pen::renderer_new_frame();
        
        put::dev_ui::new_frame();
        
        example_update(main_scene, main_camera, dt);
                
        ecs::update(dt);

        pmfx::render();

        pmfx::show_dev_ui();
        put::dev_ui::render();

        pen::renderer_present();
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
