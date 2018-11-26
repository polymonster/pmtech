#include "camera.h"
#include "ces/ces_editor.h"
#include "ces/ces_resources.h"
#include "ces/ces_scene.h"
#include "ces/ces_utilities.h"
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
using namespace ces;

pen::window_creation_params pen_window{
    1280,           // width
    720,            // height
    4,              // MSAA samples
    "pmfx_renderer" // window title / process name
};

namespace physics
{
    extern PEN_TRV physics_thread_main(void* params);
}

struct forward_lit_material
{
    vec4f albedo;
    f32   roughness;
    f32   reflectivity;
};

void create_scene_objects(ces::entity_scene* scene)
{
    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* box_resource = get_geometry_resource(PEN_HASH("cube"));

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

    // add ground
    f32 ground_size = 200.0f;
    u32 ground = get_new_node(scene);
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(ground_size, 1.0f, ground_size);
    scene->transforms[ground].translation = vec3f::zero();
    scene->parents[ground] = ground;
    scene->entities[ground] |= CMP_TRANSFORM;

    instantiate_geometry(box_resource, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);

    forward_lit_material* m = (forward_lit_material*)&scene->material_data[ground].data[0];
    m->albedo = vec4f::one() * 0.7f;
    m->roughness = 1.0f;
    m->reflectivity = 0.0f;

    // add some pillars for shadow casters
    f32   num_pillar_rows = 20;
    f32   pillar_size = 20.0f;
    f32   d = ground_size / num_pillar_rows;
    f32   s = -d * (f32)num_pillar_rows;
    vec3f start_pos = vec3f(s, pillar_size, s);
    vec3f pos = start_pos;
    for (s32 i = 0; i < num_pillar_rows; ++i)
    {
        pos.z = start_pos.z;

        for (s32 j = 0; j < num_pillar_rows; ++j)
        {
            f32 rx = 0.1f + (f32)(rand()%255) / 255.0f * pillar_size;
            f32 ry = 0.1f + (f32)(rand()%255) / 255.0f * pillar_size * 4.0f;
            f32 rz = 0.1f + (f32)(rand()%255) / 255.0f * pillar_size;
            
            pos.y = ry;
            
            u32 pillar = get_new_node(scene);
            scene->transforms[pillar].rotation = quat();
            scene->transforms[pillar].scale = vec3f(rx, ry, rz);
            scene->transforms[pillar].translation = pos;
            scene->parents[pillar] = pillar;
            scene->entities[pillar] |= CMP_TRANSFORM;

            instantiate_geometry(box_resource, scene, pillar);
            instantiate_material(default_material, scene, pillar);
            instantiate_model_cbuffer(scene, pillar);

            forward_lit_material* m = (forward_lit_material*)&scene->material_data[pillar].data[0];
            m->albedo = vec4f::one() * 0.7f;
            m->roughness = 1.0f;
            m->reflectivity = 0.0f;

            pos.z += d * 2.0f;
        }

        pos.x += d * 2.0f;
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
    put::ces::entity_scene* main_scene = put::ces::create_scene("main_scene");
    put::ces::editor_init(main_scene);

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

    pmfx::init("data/configs/pmfx_demo.jsn");

    create_scene_objects(main_scene);

    f32  frame_time = 0.0f;

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
        pen::renderer_consume_cmd_buffer();

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
