#include "pen.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "debug_render.h"
#include "input.h"
#include <string>

pen::window_creation_params pen_window
{
    1280,					//width
    720,					//height
    4,						//MSAA samples
    "input"                 //window title / process name
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

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job_thread* p_thread_info = job_params->job_thread_info;
    pen::threads_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
    //initialise the debug render system
    dbg::initialise();

    //create 2 clear states one for the render target and one for the main screen, so we can see the difference
    static pen::clear_state cs =
    {
        0.5f, 0.5f, 0.5f, 0.5f, 1.0f, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
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

    u32 raster_state = pen::defer::renderer_create_rasterizer_state( rcp );

    //viewport
    pen::viewport vp =
    {
        0.0f, 0.0f,
        1280.0f, 720.0f,
        0.0f, 1.0f
    };

    while( 1 )
    {
        pen::defer::renderer_set_rasterizer_state( raster_state );

        //bind back buffer and clear
        pen::defer::renderer_set_viewport( vp );
        pen::defer::renderer_set_targets( PEN_DEFAULT_RT, PEN_DEFAULT_DS );
        pen::defer::renderer_clear( clear_state );

        dbg::print_text( 10.0f, 10.0f, vp, vec4f( 0.0f, 1.0f, 0.0f, 1.0f ), "%s", "Input Test" );
        
        const pen::mouse_state& ms = pen::input_get_mouse_state( );
        
        //mouse
        vec2f mouse_pos = vec2f( (f32)ms.x, vp.height - (f32)ms.y );
        vec2f mouse_quad_size = vec2f( 5.0f, 5.0f );
        dbg::add_quad_2f( mouse_pos, mouse_quad_size, vec3f::cyan() );
        
        dbg::print_text( 10.0f, 20.0f, vp, vec4f( 1.0f, 1.0f, 1.0f, 1.0f ),
                        "mouse down : left %i, middle %i, right %i: mouse_wheel %i",
                        ms.buttons[PEN_MOUSE_L],
                        ms.buttons[PEN_MOUSE_M],
                        ms.buttons[PEN_MOUSE_R],
                        ms.wheel);
        
        //key down
        std::string key_msg = "key down: ";
        for( s32 key = 0; key < PENK_ARRAY_SIZE; ++key )
        {
            if( INPUT_PKEY(key) )
            {
                key_msg += "[";
                key_msg += pen::input_get_key_str(key);
                key_msg += "]";
            }
        }
        key_msg += "\n";
        
        dbg::print_text( 10.0f, 30.0f, vp, vec4f( 1.0f, 1.0f, 1.0f, 1.0f ), "%s", key_msg.c_str() );
        
        std::string ascii_msg = "character down: ";
        for( s32 key = 0; key < PENK_ARRAY_SIZE; ++key )
        {
            if( pen::input_get_unicode_key(key) )
            {
                ascii_msg += "[";
                ascii_msg += key;
                ascii_msg += "]";
            }
        }
        ascii_msg += "\n";
        
        dbg::print_text( 10.0f, 40.0f, vp, vec4f( 1.0f, 1.0f, 1.0f, 1.0f ), "%s", ascii_msg.c_str() );
        
        dbg::render_2d();

        //present 
        pen::defer::renderer_present();
        pen::defer::renderer_consume_cmd_buffer();
        
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
