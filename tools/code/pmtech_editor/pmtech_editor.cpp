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

// default live_lib linking, supply LIVE_LIB in project preprocessor to use a different dll
#ifndef LIVE_LIB
#if PEN_PLATFORM_WIN32
#if NDEBUG
#define LIVE_LIB "live_lib.dll"
#else
#define LIVE_LIB "live_lib_d.dll"
#endif
#elif PEN_PLATFORM_OSX
#if NDEBUG
#define LIVE_LIB "liblive_lib.dylib"
#else
#define LIVE_LIB "liblive_lib_d.dylib"
#endif
#elif PEN_PLATFORM_LINUX
#if NDEBUG
#define LIVE_LIB "liblive_lib.so"
#else
#define LIVE_LIB "liblive_lib_d.so"
#endif
#endif
#endif

// setup for cr live linking
#define CR_HOST
#define CR_DEBUG
#include "cr/cr.h"
#include "ecs/ecs_live.h"

// allows linking with cr as dll or the same code path statically linked in for final executables
#if CR_STATIC
extern int cr_main(struct cr_plugin *ctx, enum cr_op operation);
#endif

using namespace put;
using namespace put::ecs;

namespace
{
    void*   editor_setup(void* params);
    loop_t  editor_update();
    void    editor_shutdown();
}

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "pmtech_editor";
        p.window_sample_count = 4;
        p.user_thread_function = editor_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        p.max_renderer_commands = 1 << 20;
        return p;
    }
} // namespace pen

namespace physics
{
    extern void* physics_thread_main(void* params);
}

namespace
{
    put::camera             main_camera;
    put::ecs::ecs_scene*    main_scene = nullptr;
    pen::job*               s_thread_info = nullptr;
    pen::timer*             frame_timer = nullptr;
    bool                    live_lib = false;
    f32                     dt = 0.0f;
    cr_plugin               ctx;
    live_context*           lc = nullptr;
    
    void setup_live_lib()
    {
        // cr
        Str ll = pen::os_path_for_resource(LIVE_LIB);
        ll = pen::str_replace_string(ll, "\"", "");
        ll = pen::str_replace_string(ll, "\"", "");

        live_lib = cr_plugin_open(ctx, ll.c_str());
        
        lc = new live_context();
        lc->scene = main_scene;
        lc->main_camera = &main_camera;
        
#if CR_STATIC
        ctx.userdata = lc;
        cr_main(&ctx, CR_LOAD);
#endif
    }
    
    void* editor_setup(void* params)
    {
        // unpack the params passed to the thread and signal to the engine it ok to proceed
        pen::job_thread_params* job_params = (pen::job_thread_params*)params;
        s_thread_info = job_params->job_info;
        pen::semaphore_post(s_thread_info->p_sem_continue, 1);
        
        pen::jobs_create_job(physics::physics_thread_main, 1024 * 10, nullptr, pen::e_thread_start_flags::detached);

        // create the main scene and camera
        main_scene = put::ecs::create_scene("main_scene");
        camera_create_perspective(&main_camera, 60.0f, put::k_use_window_aspect, 0.1f, 1000.0f);

        // init systems
        dev_ui::init();
        dbg::init();
        ecs::init();
        ecs::editor_init(main_scene, &main_camera);
        put::init_hot_loader();

        pmfx::register_scene(main_scene, "main_scene");
        pmfx::register_camera(&main_camera, "model_viewer_camera");
        pmfx::init("data/configs/editor_renderer.jsn");
        
        // live liv may override pmfx::init
        setup_live_lib();

        frame_timer = pen::timer_create();
        pen::timer_start(frame_timer);
        
        pen_main_loop(editor_update);
        return PEN_THREAD_OK;
    }

    void editor_shutdown()
    {
        pen::renderer_new_frame();
        
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

        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        // signal to the engine the thread has finished
        pen::semaphore_post(s_thread_info->p_sem_terminated, 1);
    }
    
    void update_live_lib()
    {
        lc->dt = dt;
        ctx.userdata = (void*)lc;
            
#if CR_STATIC
        cr_main(&ctx, CR_STEP);
#else
        if(live_lib)
        {
            cr_plugin_update(ctx);
        }
#endif

        lc->load_index++;
    }

    loop_t editor_update()
    {
        dt = pen::timer_elapsed_ms(frame_timer)/1000.0f;
        pen::timer_start(frame_timer);

        pen::renderer_new_frame();
        put::dev_ui::new_frame();
        
        update_live_lib();
        
        ecs::update(dt);

        pmfx::render();

        pmfx::show_dev_ui();
        put::vgt::show_dev_ui();
        put::dev_ui::render();

        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        put::vgt::post_update();
        pmfx::poll_for_changes();
        put::poll_hot_loader();

        if (pen::semaphore_try_wait(s_thread_info->p_sem_exit))
        {
            editor_shutdown();
            pen_main_loop_exit();
        }
        
        pen_main_loop_continue();
    }
}
