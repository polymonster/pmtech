#include "pen.h"
#include "threads.h"
#include "memory.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "debug_render.h"

pen::window_creation_params pen_window
{
    1280,					//width
    720,					//height
    4,						//MSAA samples
    "debug_text"		    //window title / process name
};

typedef struct vertex
{
    float x, y, z, w;
} vertex;

typedef struct textured_vertex
{
    float x, y, z, w;
    float u, v;
} textured_vertex;

PEN_TRV pen::user_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job_thread* p_thread_info = job_params->job_thread_info;
    pen::threads_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
    u32 timer_test = pen::timer_create("test");
    
    //initialise the debug render system
    put::dbg::init();

    //create 2 clear states one for the render target and one for the main screen, so we can see the difference
    static pen::clear_state cs =
    {
        0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };

    u32 clear_state = pen::renderer_create_clear_state( cs );

    //raster state
    pen::rasteriser_state_creation_params rcp;
    pen::memory_zero( &rcp, sizeof( rasteriser_state_creation_params ) );
    rcp.fill_mode = PEN_FILL_SOLID;
    rcp.cull_mode = PEN_CULL_BACK;
    rcp.depth_bias_clamp = 0.0f;
    rcp.sloped_scale_depth_bias = 0.0f;
    rcp.depth_clip_enable = true;

    u32 raster_state = pen::renderer_create_rasterizer_state( rcp );
    
    //cb
    pen::buffer_creation_params bcp;
    bcp.usage_flags = PEN_USAGE_DYNAMIC;
    bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
    bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
    bcp.buffer_size = sizeof(float) * 16;
    bcp.data = (void*)nullptr;
    
    u32 cb_2d_view = pen::renderer_create_buffer(bcp);

    while( 1 )
    {
        static u32 frame_timer = pen::timer_create("frame_timer");
        pen::timer_start(frame_timer);
        
        //viewport
        pen::viewport vp =
        {
            0.0f, 0.0f,
            (f32)pen_window.width, (f32)pen_window.height,
            0.0f, 1.0f
        };
        
        pen::timer_start(timer_test);
        
        pen::renderer_set_rasterizer_state( raster_state );

        //bind back buffer and clear
        pen::renderer_set_viewport( vp );
        pen::renderer_set_targets( PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH );
        pen::renderer_clear( clear_state );

		put::dbg::add_text_2f( 10.0f, 10.0f, vp, vec4f( 0.0f, 1.0f, 0.0f, 1.0f ), "%s", "Debug Text" );
		put::dbg::add_text_2f( 10.0f, 20.0f, vp, vec4f( 1.0f, 0.0f, 1.0f, 1.0f ), "%s", "Magenta" );
		put::dbg::add_text_2f( 10.0f, 30.0f, vp, vec4f( 0.0f, 1.0f, 1.0f, 1.0f ), "%s", "Cyan" );
		put::dbg::add_text_2f( 10.0f, 40.0f, vp, vec4f( 1.0f, 1.0f, 0.0f, 1.0f ), "%s", "Yellow" );
        
        //create 2d view proj matrix
        float W = 2.0f / vp.width;
        float H = 2.0f / vp.height;
        float mvp[4][4] =
        {
            { W, 0.0, 0.0, 0.0 },
            { 0.0, H, 0.0, 0.0 },
            { 0.0, 0.0, 1.0, 0.0 },
            { -1.0, -1.0, 0.0, 1.0 }
        };
        pen::renderer_update_buffer(cb_2d_view, mvp, sizeof(mvp), 0);

		put::dbg::render_2d(cb_2d_view);

        //present 
        pen::renderer_present();

        pen::renderer_consume_cmd_buffer();
        
        f32 time_ms = pen::timer_elapsed_ms(frame_timer);
		put::dbg::add_text_2f( 10.0f, 50.0f, vp, vec4f( 0.0f, 0.0f, 1.0f, 1.0f ), "%s%f", "Timer", time_ms );
        
        //msg from the engine we want to terminate
        if( pen::threads_semaphore_try_wait( p_thread_info->p_sem_exit ) )
        {
            break;
        }
    }
    
    //clean up mem here
    
    //signal to the engine the thread has finished
    pen::threads_semaphore_signal( p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}
