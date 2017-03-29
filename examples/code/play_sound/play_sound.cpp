#include "pen.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "debug_render.h"
#include "audio.h"

pen::window_creation_params pen_window
{
    1280,					//width
    720,					//height
    4,						//MSAA samples
    "play_sound"		    //window title / process name
};

u32 clear_state_grey;
u32 raster_state_cull_back;

pen::viewport vp =
{
    0.0f, 0.0f,
    1280.0f, 720.0f,
    0.0f, 1.0f
};

void renderer_state_init( )
{
    //initialise the debug render system
    dbg::initialise();

    //create 2 clear states one for the render target and one for the main screen, so we can see the difference
    static pen::clear_state cs =
    {
        0.5f, 0.5f, 0.5f, 0.5f, 1.0f, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };

    clear_state_grey = pen::renderer_create_clear_state( cs );

    //raster state
    pen::rasteriser_state_creation_params rcp;
    pen::memory_zero( &rcp, sizeof( pen::rasteriser_state_creation_params ) );
    rcp.fill_mode = PEN_FILL_SOLID;
    rcp.cull_mode = PEN_CULL_BACK;
    rcp.depth_bias_clamp = 0.0f;
    rcp.sloped_scale_depth_bias = 0.0f;
    rcp.depth_clip_enable = true;

    raster_state_cull_back = pen::defer::renderer_create_rasterizer_state( rcp );
}

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    renderer_state_init();

    u32 sound_index = pen::audio_create_sound("data/audio/t1.mp3");
    u32 channel_index = pen::audio_create_channel_for_sound( sound_index );

    while( 1 )
    {
        pen::defer::renderer_set_rasterizer_state( raster_state_cull_back );

        //bind back buffer and clear
        pen::defer::renderer_set_viewport( vp );
        pen::defer::renderer_set_targets( PEN_DEFAULT_RT, PEN_DEFAULT_DS );
        pen::defer::renderer_clear( clear_state_grey );

        dbg::print_text( 10.0f, 10.0f, vp, vec4f( 0.0f, 1.0f, 0.0f, 1.0f ), "%s", "Debug Text" );

        dbg::render_text();

        //present 
        pen::defer::renderer_present();

        pen::defer::renderer_consume_cmd_buffer();
        pen::audio_consume_command_buffer();
    }

    return PEN_THREAD_OK;
}
