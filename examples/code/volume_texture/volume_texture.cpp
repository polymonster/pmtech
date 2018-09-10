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
    1280,            // width
    720,             // height
    4,               // MSAA samples
    "volume_texture" // window title / process name
};

namespace physics
{
    extern PEN_TRV physics_thread_main(void* params);
}

void create_scene_objects(ces::entity_scene* scene)
{
    clear_scene(scene);

    geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));

    // create a simple 3d texture
    u32 block_size       = 4;
    u32 volume_dimension = 64;
    u32 data_size        = volume_dimension * volume_dimension * volume_dimension * block_size;

    u8* volume_data = (u8*)pen::memory_alloc(data_size);
    u32 row_pitch   = volume_dimension * block_size;
    u32 slice_pitch = volume_dimension * row_pitch;

    for (u32 z = 0; z < volume_dimension; ++z)
    {
        for (u32 y = 0; y < volume_dimension; ++y)
        {
            for (u32 x = 0; x < volume_dimension; ++x)
            {
                u32 offset = z * slice_pitch + y * row_pitch + x * block_size;

                u8 r, g, b, a;
                r = 255;
                g = 255;
                b = 255;
                a = 255;

                u32 black = 0;
                if (x < volume_dimension / 3 || x > volume_dimension - volume_dimension / 3)
                    black++;

                if (y < volume_dimension / 3 || y > volume_dimension - volume_dimension / 3)
                    black++;

                if (z < volume_dimension / 3 || z > volume_dimension - volume_dimension / 3)
                    black++;

                if (black == 2)
                {
                    a = 0;
                }

                volume_data[offset + 0] = b;
                volume_data[offset + 1] = g;
                volume_data[offset + 2] = r;
                volume_data[offset + 3] = a;
            }
        }
    }

    pen::texture_creation_params tcp;
    tcp.collection_type = pen::TEXTURE_COLLECTION_VOLUME;

    tcp.width            = volume_dimension;
    tcp.height           = volume_dimension;
    tcp.format           = PEN_TEX_FORMAT_BGRA8_UNORM;
    tcp.num_mips         = 1;
    tcp.num_arrays       = volume_dimension;
    tcp.sample_count     = 1;
    tcp.sample_quality   = 0;
    tcp.usage            = PEN_USAGE_DEFAULT;
    tcp.bind_flags       = PEN_BIND_SHADER_RESOURCE;
    tcp.cpu_access_flags = 0;
    tcp.flags            = 0;
    tcp.block_size       = block_size;
    tcp.pixels_per_block = 1;
    tcp.data             = volume_data;
    tcp.data_size        = data_size;

    u32 volume_texture = pen::renderer_create_texture(tcp);

    // create material for volume ray trace
    material_resource* volume_material                   = new material_resource;
    volume_material->material_name                       = "volume_material";
    volume_material->shader_name                         = "pmfx_utility";
    volume_material->id_shader                           = PEN_HASH("pmfx_utility");
    volume_material->id_technique                        = PEN_HASH("volume_texture");
    volume_material->id_sampler_state[SN_VOLUME_TEXTURE] = PEN_HASH("clamp_point_sampler_state");
    volume_material->texture_handles[SN_VOLUME_TEXTURE]  = volume_texture;
    add_material_resource(volume_material);

    // create scene node
    u32 new_prim           = get_new_node(scene);
    scene->names[new_prim] = "sphere";
    scene->names[new_prim].appendf("%i", new_prim);
    scene->transforms[new_prim].rotation    = quat();
    scene->transforms[new_prim].scale       = vec3f(10.0f);
    scene->transforms[new_prim].translation = vec3f::zero();
    scene->entities[new_prim] |= CMP_TRANSFORM;
    scene->parents[new_prim] = new_prim;
    instantiate_geometry(cube, scene, new_prim);
    instantiate_material(volume_material, scene, new_prim);
    instantiate_model_cbuffer(scene, new_prim);
}

PEN_TRV pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params    = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);

    pen::thread_create_job(physics::physics_thread_main, 1024 * 10, nullptr, pen::THREAD_START_DETACHED);

    put::dev_ui::init();
    put::dbg::init();

    // create main camera and controller
    put::camera main_camera;
    put::camera_create_perspective(&main_camera, 60.0f, (f32)pen_window.width / (f32)pen_window.height, 0.1f, 1000.0f);

    put::scene_controller cc;
    cc.camera          = &main_camera;
    cc.update_function = &ces::update_model_viewer_camera;
    cc.name            = "model_viewer_camera";
    cc.id_name         = PEN_HASH(cc.name.c_str());

    // create the main scene and controller
    put::ces::entity_scene* main_scene = put::ces::create_scene("main_scene");
    put::ces::editor_init(main_scene);

    put::scene_controller sc;
    sc.scene           = main_scene;
    sc.update_function = &ces::update_model_viewer_scene;
    sc.name            = "main_scene";
    sc.camera          = &main_camera;
    sc.id_name         = PEN_HASH(sc.name.c_str());

    // create view renderers
    put::scene_view_renderer svr_main;
    svr_main.name            = "ces_render_scene";
    svr_main.id_name         = PEN_HASH(svr_main.name.c_str());
    svr_main.render_function = &ces::render_scene_view;

    put::scene_view_renderer svr_editor;
    svr_editor.name            = "ces_render_editor";
    svr_editor.id_name         = PEN_HASH(svr_editor.name.c_str());
    svr_editor.render_function = &ces::render_scene_editor;

    pmfx::register_scene_view_renderer(svr_main);

    pmfx::register_scene_controller(sc);
    pmfx::register_scene_controller(cc);

    pmfx::init("data/configs/basic_renderer.yaml");

    create_scene_objects(main_scene);

    bool enable_dev_ui = true;
    f32  frame_time    = 0.0f;

    while (1)
    {
        static u32 frame_timer = pen::timer_create("frame_timer");
        pen::timer_start(frame_timer);

        put::dev_ui::new_frame();

        pmfx::update();

        pmfx::render();

        pmfx::show_dev_ui();

        if (enable_dev_ui)
        {
            put::dev_ui::console();
            put::dev_ui::render();
        }

        if (pen::input_is_key_held(PK_MENU) && pen::input_is_key_pressed(PK_D))
            enable_dev_ui = !enable_dev_ui;

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
