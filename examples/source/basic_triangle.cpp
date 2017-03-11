#include "pen.h"

pen::window_creation_params pen_window
{
	1280,					//width
	720,					//height
	4,						//MSAA samples
	"basic_triangle"		//window title / process name
};

PEN_THREAD_RETURN pen::game_entry( void* params )
{
	//clear state
	pen::clear_state cs =
	{
		1.0f, 0.0, 0.0f, 1.0f, 1.0f, PEN_CLEAR_COLOUR_BUFFER
	};

	u32 clear_state_handle = pen::renderer_create_clear_state( cs );

	//raster state
	pen::rasteriser_state_creation_params rcp;
	pen::memory_zero( &rcp, sizeof( rasteriser_state_creation_params ) );
	rcp.fill_mode = PEN_FILL_SOLID;
	rcp.cull_mode = PEN_CULL_BACK;
	rcp.depth_bias_clamp = 0.0f;
	rcp.sloped_scale_depth_bias = 0.0f;

	u32 raster_state = pen::defer::renderer_create_rasterizer_state( rcp );

	//viewport
	pen::viewport vp =
	{
		0.0f, 0.0f,
		//( f32 ) pen_window.width, ( f32 ) pen_window.height,
		( f32 ) 1280, ( f32 ) 720,
		0.0f, 1.0f
	};

	while( 1 )
	{
		//set viewport and raster state
		pen::defer::renderer_set_colour_buffer( 0 );
		pen::defer::renderer_set_viewport( vp );
		pen::defer::renderer_set_rasterizer_state( raster_state );

		//clear screen
		pen::defer::renderer_clear( clear_state_handle );

		//swap buffers
		pen::defer::renderer_present( );

		//submit
		pen::defer::renderer_consume_cmd_buffer( );
	}

	return 0;
}