#include "pen.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "dev_ui.h"
#include "camera.h"
#include "debug_render.h"
#include "component_entity.h"

pen::window_creation_params pen_window
{
    1280,					//width
    720,					//height
    4,						//MSAA samples
    "model_viewer"		    //window title / process name
};

struct vertex
{
    float x, y, z, w;
};

struct textured_vertex
{
    float x, y, z, w;
    float u, v;
};

struct render_handles
{
    //states
    u32 clear_state;
    u32 raster_state;
    u32 ds_state;
    
    //buffers
    u32 vb;
    u32 ib;
    
    //samplers
    u32 sampler_linear_clamp;
    u32 sampler_linear_wrap;
    u32 sampler_point_clamp;
    u32 sampler_point_wrap;
    
    //cbuffers
    u32 view_cbuffer;
    u32 tweakable_cbuffer;
    
    pen::viewport vp;
    pen::rect r;
    
    void release()
    {
        pen::renderer_release_clear_state(clear_state);
        pen::renderer_release_raster_state(raster_state);
        
        pen::renderer_release_buffer(vb);
        pen::renderer_release_buffer(ib);
        
        pen::renderer_release_sampler(sampler_linear_clamp);
        pen::renderer_release_sampler(sampler_linear_wrap);
        pen::renderer_release_sampler(sampler_point_clamp);
        pen::renderer_release_sampler(sampler_point_wrap);
    }
};

static render_handles k_render_handles;

void init_renderer( )
{
    //clear state
    static pen::clear_state cs =
    {
        0.5f, 0.5, 0.5f, 1.0f, 1.0f, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };
    
    k_render_handles.clear_state = pen::renderer_create_clear_state( cs );
    
    //raster state
    pen::rasteriser_state_creation_params rcp;
    pen::memory_zero( &rcp, sizeof( pen::rasteriser_state_creation_params ) );
    rcp.fill_mode = PEN_FILL_SOLID;
    rcp.cull_mode = PEN_CULL_NONE;
    rcp.depth_bias_clamp = 0.0f;
    rcp.sloped_scale_depth_bias = 0.0f;
    
    k_render_handles.raster_state = pen::renderer_create_rasterizer_state( rcp );
    
    //viewport
    k_render_handles.vp =
    {
        0.0f, 0.0f,
        1280.0f, 720.0f,
        0.0f, 1.0f
    };
    
    k_render_handles.r = { 0.0f, 1280.0f, 0.0f, 720.0f };
    
    //buffers
    //create vertex buffer for a quad
    textured_vertex quad_vertices[] =
    {
        0.0f, 0.0f, 0.5f, 1.0f,         //p1
        0.0f, 0.0f,                     //uv1
        
        0.0f, 1.0f, 0.5f, 1.0f,         //p2
        0.0f, 1.0f,                     //uv2
        
        1.0f, 1.0f, 0.5f, 1.0f,         //p3
        1.0f, 1.0f,                     //uv3
        
        1.0f, 0.0f, 0.5f, 1.0f,         //p4
        1.0f, 0.0f,                     //uv4
    };
    
    pen::buffer_creation_params bcp;
    bcp.usage_flags = PEN_USAGE_DEFAULT;
    bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
    bcp.cpu_access_flags = 0;
    
    bcp.buffer_size = sizeof( textured_vertex ) * 4;
    bcp.data = ( void* ) &quad_vertices[ 0 ];
    
    k_render_handles.vb = pen::renderer_create_buffer( bcp );
    
    //create index buffer
    u16 indices[] =
    {
        0, 1, 2,
        2, 3, 0
    };
    
    bcp.usage_flags = PEN_USAGE_IMMUTABLE;
    bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
    bcp.cpu_access_flags = 0;
    bcp.buffer_size = sizeof( u16 ) * 6;
    bcp.data = ( void* ) &indices[ 0 ];
    
    k_render_handles.ib = pen::renderer_create_buffer( bcp );
    
    //sampler states
    //create a sampler object so we can sample a texture
    pen::sampler_creation_params scp;
    pen::memory_zero( &scp, sizeof( pen::sampler_creation_params ) );
    scp.filter = PEN_FILTER_MIN_MAG_MIP_LINEAR;
    scp.address_u = PEN_TEXTURE_ADDRESS_CLAMP;
    scp.address_v = PEN_TEXTURE_ADDRESS_CLAMP;
    scp.address_w = PEN_TEXTURE_ADDRESS_CLAMP;
    scp.comparison_func = PEN_COMPARISON_ALWAYS;
    scp.min_lod = 0.0f;
    scp.max_lod = 4.0f;
    
    k_render_handles.sampler_linear_clamp = pen::renderer_create_sampler( scp );
    
    scp.filter = PEN_FILTER_MIN_MAG_MIP_POINT;
    k_render_handles.sampler_point_clamp = pen::renderer_create_sampler( scp );
    
    scp.address_u = PEN_TEXTURE_ADDRESS_WRAP;
    scp.address_v = PEN_TEXTURE_ADDRESS_WRAP;
    scp.address_w = PEN_TEXTURE_ADDRESS_WRAP;
    k_render_handles.sampler_point_wrap = pen::renderer_create_sampler( scp );
    
    scp.filter = PEN_FILTER_MIN_MAG_MIP_LINEAR;
    k_render_handles.sampler_linear_wrap = pen::renderer_create_sampler( scp );
    
    //depth stencil state
    pen::depth_stencil_creation_params depth_stencil_params = { 0 };
    
    // Depth test parameters
    depth_stencil_params.depth_enable = true;
    depth_stencil_params.depth_write_mask = 1;
    depth_stencil_params.depth_func = PEN_COMPARISON_LESS;
    
    k_render_handles.ds_state = pen::renderer_create_depth_stencil_state(depth_stencil_params);
    
    //constant buffer
    bcp.usage_flags = PEN_USAGE_DYNAMIC;
    bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
    bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
    bcp.buffer_size = sizeof( float ) * 16;
    bcp.data = ( void* )nullptr;
    
    k_render_handles.view_cbuffer = pen::renderer_create_buffer( bcp );
}

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
	put::ces::component_entity_scene* scene;

	put::camera		main_camera;
	e_camera_mode	camera_mode = CAMERA_MODELLING;
	
};
model_view_controller k_model_view_controller;

void show_ui()
{   
    put::dev_ui::new_frame();
    
    bool open = true;
    ImGui::Begin( "Model Viewer", &open );

	ImGui::Combo( "Camera Mode", (s32*)&k_model_view_controller.camera_mode, (const c8**)&camera_mode_names, 2 );
    
    f32 cur_time = pen::timer_get_time();
    static f32 prev_time = cur_time;
    f32 dt = (cur_time - prev_time) / 1000.0f;
    prev_time = cur_time;
    
    ImGui::Text("dt %f", dt);
    

	//put::ces::enumerate_scene(k_model_view_controller.scene);
        
    ImGui::End();
}

void init_model_view()
{
	put::camera_create_projection(&k_model_view_controller.main_camera, 60.0f, (f32)pen_window.width / (f32)pen_window.height, 0.1f, 1000.0f);

	k_model_view_controller.scene = put::ces::create_scene("main_scene");

	put::ces::import_model_scene("1210_scene", k_model_view_controller.scene);
}

void update_model_view()
{
	//dev ui
	show_ui();
	
	//update
	switch (k_model_view_controller.camera_mode)
	{
		case CAMERA_MODELLING:
			put::camera_update_modelling(&k_model_view_controller.main_camera);
			break;
		case CAMERA_FLY:
			put::camera_update_fly(&k_model_view_controller.main_camera);
			break;
	}

	put::camera_update_shader_constants(&k_model_view_controller.main_camera);

	put::ces::update_scene_matrices(k_model_view_controller.scene);
}

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job_thread* p_thread_info = job_params->job_thread_info;
    pen::threads_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
	//init systems
	put::dev_ui::init();
	put::dbg::init();

    init_renderer();
	init_model_view();
    
    while( 1 )
    {
		update_model_view();
        
		//render
        pen::renderer_set_rasterizer_state( k_render_handles.raster_state );
                        
        //bind back buffer and clear
        pen::renderer_set_viewport( k_render_handles.vp );
        pen::renderer_set_scissor_rect( rect{ k_render_handles.vp.x, k_render_handles.vp.y, k_render_handles.vp.width, k_render_handles.vp.height} );
        pen::renderer_set_depth_stencil_state(k_render_handles.ds_state);
        pen::renderer_set_targets( PEN_DEFAULT_RT, PEN_DEFAULT_DS );
        pen::renderer_clear( k_render_handles.clear_state );

		put::ces::scene_view view =
		{
			k_model_view_controller.main_camera.cbuffer,
			PEN_DEFAULT_RT,
			PEN_DEFAULT_DS,
			k_render_handles.ds_state
		};
        
		put::ces::render_scene_view(k_model_view_controller.scene, view);

		put::ces::render_scene_debug(k_model_view_controller.scene, view);

		put::dbg::add_grid( vec3f::zero(), vec3f(100.0f), 100 );

		put::dbg::render_3d(k_model_view_controller.main_camera.cbuffer );

		put::dev_ui::render();
        
        pen::renderer_present();

        pen::renderer_consume_cmd_buffer();
        
		put::loader_poll_for_changes();

        //msg from the engine we want to terminate
        if( pen::threads_semaphore_try_wait( p_thread_info->p_sem_exit ) )
        {
            break;
        }
    }
    
    //clean up mem here    
    k_render_handles.release();
    
    pen::renderer_consume_cmd_buffer();
    
    //signal to the engine the thread has finished
    pen::threads_semaphore_signal( p_thread_info->p_sem_terminated, 1);
    

    return PEN_THREAD_OK;
}
