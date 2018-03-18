#include "pen.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "dev_ui.h"
#include "camera.h"
#include "debug_render.h"
#include "pmfx.h"
#include "pen_json.h"
#include "hash.h"
#include "str_utilities.h"
#include "input.h"
#include "ces/ces_scene.h"
#include "ces/ces_resources.h"
#include "ces/ces_editor.h"

using namespace put;

pen::window_creation_params pen_window
{
    1280,					//width
    720,					//height
    4,						//MSAA samples
    "scene_editor"		    //window title / process name
};

namespace physics
{
    extern PEN_THREAD_RETURN physics_thread_main( void* params );
}

#if 0
texture_info info;
get_texture_info(k_textures[1].handle, info);

pen::resource_read_back_params rrbp;
rrbp.block_size = 4;
rrbp.row_pitch = info.width * rrbp.block_size;
rrbp.depth_pitch = info.height * rrbp.row_pitch;
rrbp.data_size = rrbp.depth_pitch;
rrbp.resource_index = k_textures[1].handle;
rrbp.format = PEN_TEX_FORMAT_BGRA8_UNORM;
rrbp.call_back_function = image_read_back;

void image_read_back(void* p_data, u32 row_pitch, u32 depth_pitch, u32 block_size)
{
	u32 w = row_pitch / block_size;
	u32 h = depth_pitch / row_pitch;

	stbi_write_bmp("bmp_tester_file.bmp", w, h, 4, p_data);
}
pen::renderer_read_back_resource(rrbp);
#endif

put::ces::entity_scene* main_scene;
put::camera volume_raster_ortho;
void volume_rasteriser_update(put::camera_controller* cc)
{
	vec3f min = main_scene->renderable_extents.min;
	vec3f max = main_scene->renderable_extents.max;

	f32 slice_thickness = (max.z - min.z) / 32.0f;
	static f32 cur_slice_pos = 50.0;

	static int slice = 0;
	ImGui::InputInt("slice", &slice);

	f32 near_slice = min.z + slice_thickness * slice;

	//put::camera_create_orthographic(&volume_raster_ortho, min.x, max.x, min.y, max.y, near_slice, near_slice + slice_thickness);

	put::camera_create_orthographic(&volume_raster_ortho, min.x, max.x, min.y, max.y, min.z, max.z);
}

void volume_rasteriser_init()
{
	//create ortho camera and controller
	//put::camera_create_orthographic(&volume_raster_ortho, -50, 50, -50, 50, 50, -50);

	put::camera_controller cc;
	cc.camera = &volume_raster_ortho;
	cc.update_function = &volume_rasteriser_update;
	cc.name = "volume_rasteriser_camera";
	cc.id_name = PEN_HASH(cc.name.c_str());

	pmfx::register_camera(cc);
}

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job_thread* p_thread_info = job_params->job_thread_info;
    pen::threads_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
    pen::threads_create_job( physics::physics_thread_main, 1024*10, nullptr, pen::THREAD_START_DETACHED );
    
	put::dev_ui::init();
	put::dbg::init();
    
	//create main camera and controller
	put::camera main_camera;
	put::camera_create_perspective( &main_camera, 60.0f, (f32)pen_window.width / (f32)pen_window.height, 0.1f, 1000.0f );
    
    put::camera_controller cc;
    cc.camera = &main_camera;
    cc.update_function = &ces::update_model_viewer_camera;
    cc.name = "model_viewer_camera";
    cc.id_name = PEN_HASH(cc.name.c_str());
    
    //create the main scene and controller
    main_scene = put::ces::create_scene("main_scene");
    put::ces::editor_init( main_scene );
    
    put::scene_controller sc;
    sc.scene = main_scene;
    sc.update_function = &ces::update_model_viewer_scene;
    sc.name = "main_scene";
	sc.camera = &main_camera;
    sc.id_name = PEN_HASH(sc.name.c_str());
    
    //create view renderers
    put::scene_view_renderer svr_main;
    svr_main.name = "ces_render_scene";
    svr_main.id_name = PEN_HASH(svr_main.name.c_str());
    svr_main.render_function = &ces::render_scene_view;
    
    put::scene_view_renderer svr_editor;
    svr_editor.name = "ces_render_editor";
    svr_editor.id_name = PEN_HASH(svr_editor.name.c_str());
    svr_editor.render_function = &ces::render_scene_editor;
    
	volume_rasteriser_init();

    pmfx::register_scene_view_renderer(svr_main);
    pmfx::register_scene_view_renderer(svr_editor);

    pmfx::register_scene(sc);
    pmfx::register_camera(cc);
    
    pmfx::init("data/configs/editor_renderer.json");
    
    bool enable_dev_ui = true;
    
    f32 frame_time = 0.0f;
    
    while( 1 )
    {
        static u32 frame_timer = pen::timer_create("frame_timer");
        pen::timer_start(frame_timer);
        
		put::dev_ui::new_frame();
        
        pmfx::update();
        
        pmfx::render();
        
        pmfx::show_dev_ui();
        
        if( enable_dev_ui )
        {
            put::dev_ui::console();
            put::dev_ui::render();
        }
        
        if( pen::input_is_key_held(PENK_MENU) && pen::input_is_key_pressed(PENK_D) )
            enable_dev_ui = !enable_dev_ui;
        
        frame_time = pen::timer_elapsed_ms(frame_timer);
        
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();
        
		pmfx::poll_for_changes();
		put::poll_hot_loader();

        //msg from the engine we want to terminate
        if( pen::threads_semaphore_try_wait( p_thread_info->p_sem_exit ) )
            break;
    }

	ces::destroy_scene(main_scene);
	ces::editor_shutdown();
    
    //clean up mem here
	put::pmfx::shutdown();
	put::dbg::shutdown();
	put::dev_ui::shutdown();

    pen::renderer_consume_cmd_buffer();
    
    //signal to the engine the thread has finished
    pen::threads_semaphore_signal( p_thread_info->p_sem_terminated, 1);
    
    return PEN_THREAD_OK;
}
