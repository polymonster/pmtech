#include "pen.h"
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

PEN_THREAD_RETURN pen::game_entry( void* params )
{
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
    rcp.cull_mode = PEN_CULL_NONE;
    rcp.depth_bias_clamp = 0.0f;
    rcp.sloped_scale_depth_bias = 0.0f;

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

        dbg::print_text( 10.0f, 10.0f, vec4f( 0.0f, 1.0f, 0.0f, 1.0f ), "%s", "Debug Text" );
        dbg::print_text( 10.0f, 20.0f, vec4f( 1.0f, 0.0f, 1.0f, 1.0f ), "%s", "Magenta" );
        dbg::print_text( 10.0f, 30.0f, vec4f( 0.0f, 1.0f, 1.0f, 1.0f ), "%s", "Cyan" );
        dbg::print_text( 10.0f, 40.0f, vec4f( 1.0f, 1.0f, 0.0f, 1.0f ), "%s", "Yellow" );

        dbg::render_text();

        //present 
        pen::defer::renderer_present();

        pen::defer::renderer_consume_cmd_buffer();
    }

    while( 1 )
    {

    }

    return PEN_THREAD_OK;
}
