#include "pen.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "dev_ui.h"
#include "camera.h"
#include "debug_render.h"
#include "layer_controller.h"
#include "render_controller.h"
#include "pmfx.h"
#include "pen_json.h"
#include "hash.h"
#include "str_utilities.h"
#include "input.h"
#include "ces/ces_scene.h"
#include "ces/ces_resources.h"
#include "ces/ces_editor.h"

pen::window_creation_params pen_window
{
    1280,					//width
    720,					//height
    4,						//MSAA samples
    "model_viewer"		    //window title / process name
};

enum e_camera_mode : s32
{
	CAMERA_MODELLING = 0,
	CAMERA_FLY = 1
};

const c8* camera_mode_names[] =
{
	"Modelling",
	"Fly"
};

struct model_view_controller
{
	put::camera		main_camera;
	e_camera_mode	camera_mode = CAMERA_MODELLING;
	
};
model_view_controller k_model_view_controller;

using namespace put;

void update_model_viewer_camera(put::camera_controller* cc)
{
    //update camera
    if( !(dev_ui::want_capture() & dev_ui::MOUSE) )
    {
        switch (k_model_view_controller.camera_mode)
        {
            case CAMERA_MODELLING:
                put::camera_update_modelling(cc->camera);
                break;
            case CAMERA_FLY:
                put::camera_update_fly(cc->camera);
                break;
        }
    }
}

static hash_id ID_PICKING_BUFFER = PEN_HASH("picking");

struct picking_info
{
    c8 result[4];
    a_u8 ready;
    u32 offset = 0;
};
static picking_info k_picking_info;

void picking_read_back( void* p_data )
{
    u8* val = ((u8*)p_data) + k_picking_info.offset;
    
    for(s32 i = 0; i < 4; ++i)
        k_picking_info.result[i] = val[i];
    
    k_picking_info.ready = 1;
    
    delete[] (c8*)p_data;
}

bool get_picking_result( u8* result )
{
    if( k_picking_info.ready )
    {
        for(s32 i = 0; i < 4; ++i)
            result[i] = k_picking_info.result[i];
        
        return true;
    }
    
    return false;
}

void perform_picking( u32 x, u32 y )
{
    const put::render_target* rt = render_controller::get_render_target(ID_PICKING_BUFFER);
    
    f32 w, h;
    render_controller::get_render_target_dimensions(rt, w, h);
    
    u32 pitch = (u32)w*4;
    u32 data_size = (u32)h*pitch;
    c8* p_data = new c8[data_size];
    
    pen::resource_read_back_params rrbp =
    {
        rt->handle,
        (void*)p_data,
        rt->format,
        data_size,
        &picking_read_back
    };
    
    pen::renderer_read_back_resource( rrbp );
    
    k_picking_info.ready = 0;
    k_picking_info.offset = y * pitch + x * 4;
}

void update_model_viewer_scene(put::scene_controller* sc)
{
    static bool open_scene_browser = false;
    static bool open_import = false;
    static bool open_save = false;
    static bool open_camera_menu = false;
    static bool open_resource_menu = false;
    static bool dev_open = false;
    static bool set_project_dir = false;
    static bool picking_mode = false;
    static Str project_dir_str = dev_ui::get_program_preference("project_dir").as_str();
    
    ImGui::BeginMainMenuBar();
    
    if (ImGui::BeginMenu(ICON_FA_LEMON_O))
    {
        ImGui::MenuItem("Save");
        ImGui::MenuItem("Import", NULL, &open_import);
        
        if( ImGui::BeginMenu("Project Directory") )
        {
            ImGui::MenuItem("Set..", NULL, &set_project_dir);
            ImGui::Text("Dir: %s", project_dir_str.c_str());
            
            ImGui::EndMenu();
        }
        
        ImGui::MenuItem("Dev", NULL, &dev_open);
        
        ImGui::EndMenu();
    }
    
    if (ImGui::Button(ICON_FA_FLOPPY_O))
    {
        open_save = true;
    }
    
    if (ImGui::Button(ICON_FA_FOLDER_OPEN))
    {
        open_import = true;
    }
    
    if (ImGui::Button(ICON_FA_SEARCH))
    {
        open_scene_browser = true;
    }
    
    if (ImGui::Button(ICON_FA_VIDEO_CAMERA))
    {
        open_camera_menu = true;
    }
    
    if (ImGui::Button(ICON_FA_CUBES))
    {
        open_resource_menu = true;
    }
    
    if (ImGui::Button(ICON_FA_MOUSE_POINTER))
    {
        picking_mode = true;
    }
    
    ImGui::EndMainMenuBar();
    
    if( open_import )
    {
        const c8* import = put::dev_ui::file_browser(open_import, 2, dev_ui::FB_OPEN, "**.pmm", "**.pms" );
        
        if( import )
        {
            u32 len = pen::string_length( import );
            
            if( import[len-1] == 'm' )
            {
                put::ces::load_pmm( import, sc->scene );
            }
            else if( import[len-1] == 's' )
            {
                put::ces::load_scene( import, sc->scene );
            }
        }
    }
    
    if (open_scene_browser)
    {
        ces::scene_browser_ui(sc->scene, &open_scene_browser);
    }
    
    if( open_camera_menu )
    {
        if( ImGui::Begin("Camera", &open_camera_menu) )
        {
            ImGui::Combo("Camera Mode", (s32*)&k_model_view_controller.camera_mode, (const c8**)&camera_mode_names, 2);
            
            ImGui::End();
        }
    }
    
    if( open_resource_menu )
    {
        put::ces::enumerate_resources( &open_resource_menu );
    }
    
    if( set_project_dir )
    {
        const c8* set_proj = put::dev_ui::file_browser(set_project_dir, dev_ui::FB_OPEN, 1, "**." );
        
        if(set_proj)
        {
            project_dir_str = set_proj;
            dev_ui::set_program_preference("project_dir", project_dir_str);
        }
    }
    
    if( open_save )
    {
        const c8* save_file = put::dev_ui::file_browser(open_save, dev_ui::FB_SAVE, 1, "**.pms" );
        
        if(save_file)
        {
            put::ces::save_scene(save_file, sc->scene);
        }
    }
    
    if( dev_open )
    {
        if( ImGui::Begin("Dev", &dev_open) )
        {
            if( ImGui::CollapsingHeader("Icons") )
            {
                debug_show_icons();
            }
            
            ImGui::End();
        }
    }
    
    static bool picking_debug = false;
    static u8 pick_res[4];
    
    if( picking_mode )
    {
        static u32 picking_state = 0;
        
        if( picking_state == 1 )
        {
            if( get_picking_result(pick_res) )
            {
                picking_state = 0;
                picking_debug = true;
            }
        }
        else
        {
            if( !(dev_ui::want_capture() & dev_ui::MOUSE) )
            {
                pen::mouse_state ms = pen::input_get_mouse_state();
                
                if (ms.buttons[PEN_MOUSE_L] && pen::mouse_coords_valid( ms.x, ms.y ) )
                {
                    perform_picking(ms.x, ms.y);
                    picking_state = 1;
                }
            }
        }
    }
    
    if( picking_debug )
    {
        if( ImGui::Begin("picking") )
        {
            f32 col[4];
            for( s32 i = 0; i < 4; ++i )
                col[i] = (f32)pick_res[i] / (255.0f);
            
            ImGui::ColorEdit4("pick result", col );
            ImGui::End();
        }
    }
    
    static u32 timer_index = pen::timer_create("scene_update_timer");
    
    pen::timer_accum(timer_index);
    f32 dt_ms = pen::timer_get_ms(timer_index);
    pen::timer_reset(timer_index);
    pen::timer_start(timer_index);
    
    //update render data
    put::ces::update_scene(sc->scene, dt_ms);
}

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job_thread* p_thread_info = job_params->job_thread_info;
    pen::threads_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
	//init systems
	put::layer_controller_init();
    
	put::dev_ui::init();
	put::dbg::init();
    
	//create main camera and controller
	put::camera main_camera;
	put::camera_create_perspective( &main_camera, 60.0f, (f32)pen_window.width / (f32)pen_window.height, 0.1f, 1000.0f );
    
    put::camera_controller cc;
    cc.camera = &main_camera;
    cc.update_function = &update_model_viewer_camera;
    cc.name = "model_viewer_camera";
    cc.id_name = PEN_HASH(cc.name.c_str());
    
    //create the main scene and controller
    put::ces::entity_scene* main_scene = put::ces::create_scene("main_scene");
    
    put::scene_controller sc;
    sc.scene = main_scene;
    sc.update_function = &update_model_viewer_scene;
    sc.name = "main_scene";
    sc.id_name = PEN_HASH(sc.name.c_str());

    put::render_controller::register_scene(sc);
    put::render_controller::register_camera(cc);
    
    put::render_controller::init("data/configs/renderer.json");
    
    put::built_in_handles handles = put::layer_controller_built_in_handles();

	//create model viewer layer
	put::layer main_layer;

	//layer scene view and update
	main_layer.view.scene = main_scene;
	main_layer.camera = main_camera;
	main_layer.update_function = nullptr;

	//render targets and states
	main_layer.clear_state = handles.default_clear_state;
	main_layer.colour_targets[0] = PEN_DEFAULT_RT;
	main_layer.depth_target = PEN_DEFAULT_DS;
	main_layer.viewport = handles.back_buffer_vp;
	main_layer.scissor_rect = handles.back_buffer_scissor_rect;
	main_layer.depth_stencil_state = handles.depth_stencil_state_write_less;
	main_layer.raster_state = handles.raster_state_fill_cull_back;
	main_layer.blend_state = handles.blend_disabled;
    
	//add main layer to the viewer
	put::layer_controller_add_layer(main_layer);
    
    while( 1 )
    {
		put::dev_ui::new_frame();
        
		put::layer_controller_update();
        
        put::render_controller::update();
        
        put::render_controller::render();
        
        put::render_controller::show_dev_ui();
        
		put::dev_ui::render();
        
        pen::renderer_present();

        pen::renderer_consume_cmd_buffer();
        
		pmfx::poll_for_changes();

        //msg from the engine we want to terminate
        if( pen::threads_semaphore_try_wait( p_thread_info->p_sem_exit ) )
        {
            break;
        }
    }
    
    //clean up mem here 
	put::layer_controller_shutdown();
	put::dbg::shutdown();
	put::dev_ui::shutdown();

    pen::renderer_consume_cmd_buffer();
    
    //signal to the engine the thread has finished
    pen::threads_semaphore_signal( p_thread_info->p_sem_terminated, 1);
    
    
    return PEN_THREAD_OK;
}
