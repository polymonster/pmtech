#include "ces/ces_editor.h"
#include "ces/ces_resources.h"
#include "ces/ces_scene.h"
#include "ces/ces_utilities.h"
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

using namespace put;
using namespace put::ces;

pen::window_creation_params pen_window{
    1280,                           // width
    720,                            // height
    4,                              // MSAA samples
    "signed_distance_field_shadows" // window title / process name
};

namespace physics
{
    extern PEN_TRV physics_thread_main(void* params);
}

vec3f random_vel(f32 min, f32 max)
{
    f32 x = min + (((f32)(rand() % RAND_MAX) / RAND_MAX) * (max - min));
    f32 y = min + (((f32)(rand() % RAND_MAX) / RAND_MAX) * (max - min));
    f32 z = min + (((f32)(rand() % RAND_MAX) / RAND_MAX) * (max - min));

    return vec3f(x, y, z);
}

void animate_lights(entity_scene* scene, f32 dt)
{
    if (scene->flags & PAUSE_UPDATE)
        return;

    extents e = scene->renderable_extents;
    e.min -= vec3f(10.0f, 2.0f, 10.0f);
    e.max += vec3f(10.0f, 10.0f, 10.0f);

    static f32 t = 0.0f;
    t += dt * 0.001f;

    static vec3f s_velocities[MAX_FORWARD_LIGHTS];
    static bool  s_initialise = true;
    if (s_initialise)
    {
        s_initialise = false;
        srand(pen::get_time_us());

        for (u32 i = 0; i < MAX_FORWARD_LIGHTS; ++i)
            s_velocities[i] = random_vel(-1.0f, 1.0f);
    }

    u32 vel_index = 0;

    for (u32 n = 0; n < scene->num_nodes; ++n)
    {
        if (!(scene->entities[n] & CMP_LIGHT))
            continue;

        if (scene->lights[n].type == LIGHT_TYPE_DIR)
            continue;

        if (vel_index == 0)
        {
            f32 tx = sin(t);
            scene->transforms[n].translation = vec3f(tx * -20.0f, 4.0f, 15.0f);
            scene->entities[n] |= CMP_TRANSFORM;
        }

        if (vel_index == 1)
        {
            f32 tz = cos(t);
            scene->transforms[n].translation = vec3f(-15.0f, 3.0f, tz * 20.0f);
            scene->entities[n] |= CMP_TRANSFORM;
        }

        if (vel_index == 2)
        {
            f32 tx = sin(t * 0.5);
            f32 tz = cos(t * 0.5);
            scene->transforms[n].translation = vec3f(tx * 40.0f, 1.0f, tz * 30.0f);
            scene->entities[n] |= CMP_TRANSFORM;
        }

        if (vel_index == 3)
        {
            f32 tx = cos(t * 0.25);
            f32 tz = sin(t * 0.25);
            scene->transforms[n].translation = vec3f(tx * 30.0f, 6.0f, tz * 30.0f);
            scene->entities[n] |= CMP_TRANSFORM;
        }

        ++vel_index;
    }
}

PEN_TRV pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);

    pen::thread_create_job(physics::physics_thread_main, 1024 * 10, nullptr, pen::THREAD_START_DETACHED);

    put::dev_ui::init();
    put::dbg::init();

    // create main camera and controller
    put::camera main_camera;
    put::camera_create_perspective(&main_camera, 60.0f, put::k_use_window_aspect, 0.1f, 1000.0f);

    put::scene_controller cc;
    cc.camera = &main_camera;
    cc.update_function = &ces::update_model_viewer_camera;
    cc.name = "model_viewer_camera";
    cc.id_name = PEN_HASH(cc.name.c_str());

    // create the main scene and controller
    put::ces::entity_scene* main_scene;
    main_scene = put::ces::create_scene("main_scene");

    put::scene_controller sc;
    sc.scene = main_scene;
    sc.update_function = &ces::update_model_viewer_scene;
    sc.name = "main_scene";
    sc.camera = &main_camera;
    sc.id_name = PEN_HASH(sc.name.c_str());

    // create view renderers
    put::scene_view_renderer svr_main;
    svr_main.name = "ces_render_scene";
    svr_main.id_name = PEN_HASH(svr_main.name.c_str());
    svr_main.render_function = &ces::render_scene_view;

    put::scene_view_renderer svr_editor;
    svr_editor.name = "ces_render_editor";
    svr_editor.id_name = PEN_HASH(svr_editor.name.c_str());
    svr_editor.render_function = &ces::render_scene_editor;

    pmfx::register_scene_view_renderer(svr_main);
    pmfx::register_scene_view_renderer(svr_editor);

    pmfx::register_scene_controller(sc);
    pmfx::register_scene_controller(cc);

    // volume rasteriser tool
    put::ces::editor_init(main_scene);
    put::vgt::init(main_scene);

    pmfx::init("data/configs/editor_renderer.jsn");

    f32 frame_time = 0.0f;

    // load scene
    put::ces::load_scene("data/scene/sdf.pms", main_scene);

    while (1)
    {
        static u32 frame_timer = pen::timer_create("frame_timer");
        pen::timer_start(frame_timer);

        put::dev_ui::new_frame();

        pmfx::update();
        animate_lights(main_scene, frame_time);

        pmfx::render();

        pmfx::show_dev_ui();

        put::vgt::show_dev_ui();

        put::dev_ui::render();

        frame_time = pen::timer_elapsed_ms(frame_timer);

        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        put::vgt::post_update();
        pmfx::poll_for_changes();
        put::poll_hot_loader();

        // msg from the engine we want to terminate
        if (pen::thread_semaphore_try_wait(p_thread_info->p_sem_exit))
            break;
    }

    ces::destroy_scene(main_scene);
    ces::editor_shutdown();

    // clean up mem here
    put::pmfx::shutdown();
    put::dbg::shutdown();
    put::dev_ui::shutdown();

    pen::renderer_consume_cmd_buffer();

    // signal to the engine the thread has finished
    pen::thread_semaphore_signal(p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}
