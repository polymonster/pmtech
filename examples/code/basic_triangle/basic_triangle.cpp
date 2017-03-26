#include "pen.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"

pen::window_creation_params pen_window
{
	1280,					//width
	720,					//height
	4,						//MSAA samples
	"basic_triangle"		//window title / process name
};

typedef struct vertex
{
    float x, y, z, w;
} vertex;

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    f32 prev_time = pen::timer_get_time();

    //create clear state
    static pen::clear_state cs =
    {
        1.0f, 0.0, 0.0f, 1.0f, 1.0f, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
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

    //create shaders
    pen::shader_load_params vs_slp;
    vs_slp.type = PEN_SHADER_TYPE_VS;

    pen::shader_load_params ps_slp;
    ps_slp.type = PEN_SHADER_TYPE_PS;

    c8 shader_file_buf[256];

    pen::string_format(shader_file_buf, 256, "data/shaders/%s/%s", pen::renderer_get_shader_platform(), "basictri.vsc");
    pen::filesystem_read_file_to_buffer( shader_file_buf, &vs_slp.byte_code, vs_slp.byte_code_size );

    pen::string_format(shader_file_buf, 256, "data/shaders/%s/%s", pen::renderer_get_shader_platform(), "basictri.psc");
    pen::filesystem_read_file_to_buffer( shader_file_buf, &ps_slp.byte_code, ps_slp.byte_code_size );

    u32 vertex_shader = pen::defer::renderer_load_shader( vs_slp );
    u32 pixel_shader = pen::defer::renderer_load_shader( ps_slp );

    //create input layout
    pen::input_layout_creation_params ilp;
    ilp.vs_byte_code = vs_slp.byte_code;
    ilp.vs_byte_code_size = vs_slp.byte_code_size;

    ilp.num_elements = 1;

    ilp.input_layout = ( pen::input_layout_desc* )pen::memory_alloc( sizeof( pen::input_layout_desc ) * ilp.num_elements );

    c8 buf[ 16 ];
    pen::string_format( &buf[ 0 ], 16, "POSITION" );

    ilp.input_layout[ 0 ].semantic_name = ( c8* ) &buf[ 0 ];
    ilp.input_layout[ 0 ].semantic_index = 0;
    ilp.input_layout[ 0 ].format = PEN_FORMAT_R32G32B32A32_FLOAT;
    ilp.input_layout[ 0 ].input_slot = 0;
    ilp.input_layout[ 0 ].aligned_byte_offset = 0;
    ilp.input_layout[ 0 ].input_slot_class = PEN_INPUT_PER_VERTEX;
    ilp.input_layout[ 0 ].instance_data_step_rate = 0;

    u32 input_layout = pen::defer::renderer_create_input_layout( ilp );

    //create vertex buffer
    vertex vertices[] =
    {
        0.0f, 0.5f, 0.5f, 1.0f,
        0.5f, -0.5f, 0.5f, 1.0f,
        -0.5f, -0.5f, 0.5f, 1.0f
    };

    pen::buffer_creation_params bcp;
    bcp.usage_flags = PEN_USAGE_DEFAULT;
    bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
    bcp.cpu_access_flags = 0;

    bcp.buffer_size = sizeof( vertex ) * 3;
    bcp.data = ( void* ) &vertices[ 0 ];

    u32 vertex_buffer = pen::defer::renderer_create_buffer( bcp );

    //free byte code loaded from file
    pen::memory_free( vs_slp.byte_code );
    pen::memory_free( ps_slp.byte_code );

    while( 1 )
    {
        //clear screen
        pen::defer::renderer_set_viewport( vp );
        pen::defer::renderer_set_rasterizer_state( raster_state );

        pen::defer::renderer_set_targets( PEN_DEFAULT_RT, PEN_DEFAULT_DS );

        pen::defer::renderer_clear( clear_state );

        //bind vertex layout
        pen::defer::renderer_set_input_layout( input_layout );

        //bind vertex buffer
        u32 stride = sizeof( vertex );
        u32 offset = 0;
        pen::defer::renderer_set_vertex_buffer( vertex_buffer, 0, 1, &stride, &offset );

        //bind shaders
        pen::defer::renderer_set_shader( vertex_shader, PEN_SHADER_TYPE_VS );
        pen::defer::renderer_set_shader( pixel_shader, PEN_SHADER_TYPE_PS );

        //draw
        pen::defer::renderer_draw( 3, 0, PEN_PT_TRIANGLELIST );

        //present 
        pen::defer::renderer_present();
        
        pen::defer::renderer_consume_cmd_buffer();
    }

    return PEN_THREAD_OK;
}
