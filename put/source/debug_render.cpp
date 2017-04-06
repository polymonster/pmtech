#include "debug_render.h"
#include "loader.h"
#include "camera.h"
#include "input.h"
#include "memory.h"
#include "stb_easy_font.h"
#include "pen.h"

//extern u32 depth_state_disabled;
//extern u32 depth_state_enabled;

extern pen::window_creation_params pen_window;

namespace dbg
{
#define NUM_VERTEX_BUFFERS 2
	u32 debug_lines_backbuffer_index = 0;
	u32 debug_font_backbuffer_index = 0;

#define MAX_DEBUG_LINES_VERTS 2048
	typedef struct vertex_debug_lines
	{
		float x, y, z, w;
		float r, g, b, a;

	} vertex_debug_lines;

	u32 vb_lines;
	u32 line_vert_count = 0;

	vertex_debug_lines  debug_lines_buffers[NUM_VERTEX_BUFFERS][MAX_DEBUG_LINES_VERTS];
	vertex_debug_lines *debug_lines_verts = NULL;

	put::shader_program debug_lines_program;

#define MAX_DEBUG_FONT_VERTS 4096 * 8
	typedef struct vertex_debug_font
	{
		float x, y;
		float r, g, b, a;
	}vertex_debug_font;

	u32 vb_font;
	u32 font_vert_count = 0;

	vertex_debug_font  debug_font_buffers[NUM_VERTEX_BUFFERS][MAX_DEBUG_FONT_VERTS];
	vertex_debug_font *debug_font_verts = NULL;

	put::shader_program debug_font_program;

	void create_shaders( )
	{
		debug_lines_program = put::loader_load_shader_program( "debug_lines" );
		debug_font_program = put::loader_load_shader_program( "debug_font" );
	}
	void create_buffers( )
	{
		//debug lines buffer
		pen::buffer_creation_params bcp;
		bcp.usage_flags = PEN_USAGE_DYNAMIC;
		bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
		bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
		bcp.buffer_size = sizeof( vertex_debug_lines ) * MAX_DEBUG_LINES_VERTS;
		bcp.data = NULL;

		vb_lines = pen::defer::renderer_create_buffer( bcp );

		debug_lines_verts = &debug_lines_buffers[ debug_lines_backbuffer_index ][ 0 ];

		bcp.buffer_size = sizeof(vertex_debug_font) * MAX_DEBUG_FONT_VERTS;

		vb_font = pen::defer::renderer_create_buffer(bcp);

		debug_font_verts = &debug_font_buffers[debug_font_backbuffer_index][0];
	}

	void initialise( )
	{
		create_buffers( );
		create_shaders( );
	}

	void render_3d( u32 cb_3dview )
	{
		pen::defer::renderer_update_buffer( vb_lines, &debug_lines_verts[0], sizeof( vertex_debug_lines ) * MAX_DEBUG_LINES_VERTS );

		//bind vertex layout
		pen::defer::renderer_set_input_layout( debug_lines_program.input_layout );

		//bind vertex buffer
		u32 stride = sizeof(vertex_debug_lines);
		u32 offset = 0;
		pen::defer::renderer_set_vertex_buffer( vb_lines, 0, 1, &stride, &offset );

		//bind shaders
		pen::defer::renderer_set_shader( debug_lines_program.vertex_shader, PEN_SHADER_TYPE_VS );
		pen::defer::renderer_set_shader( debug_lines_program.pixel_shader, PEN_SHADER_TYPE_PS );

		//shader constants and textures
		pen::defer::renderer_set_constant_buffer( cb_3dview, 0, PEN_SHADER_TYPE_VS );

		//pen::defer::renderer_set_depth_stencil_state( depth_state_enabled );

		//draw
		pen::defer::renderer_draw( line_vert_count, 0, PEN_PT_LINELIST );

		pen::defer::renderer_consume_cmd_buffer( );

		//reset 
		line_vert_count = 0;

		debug_lines_backbuffer_index = (debug_lines_backbuffer_index + 1) & NUM_VERTEX_BUFFERS;
		
		debug_lines_verts = &debug_lines_buffers[debug_lines_backbuffer_index][0];
		
	}

	void render_text()
	{
		pen::defer::renderer_update_buffer(vb_font, &debug_font_verts[0], sizeof(vertex_debug_font) * MAX_DEBUG_FONT_VERTS );

		//bind vertex layout
		pen::defer::renderer_set_input_layout(debug_font_program.input_layout);

		//bind vertex buffer
		u32 stride = sizeof(vertex_debug_font);
		u32 offset = 0;
		pen::defer::renderer_set_vertex_buffer(vb_font, 0, 1, &stride, &offset);

		//bind shaders
		pen::defer::renderer_set_shader(debug_font_program.vertex_shader, PEN_SHADER_TYPE_VS);
		pen::defer::renderer_set_shader(debug_font_program.pixel_shader, PEN_SHADER_TYPE_PS);

		//pen::defer::renderer_set_depth_stencil_state(depth_state_disabled);

		//draw
		pen::defer::renderer_draw(font_vert_count, 0, PEN_PT_TRIANGLELIST);
        
		//reset
		font_vert_count = 0;

		debug_font_backbuffer_index = (debug_font_backbuffer_index + 1) & NUM_VERTEX_BUFFERS;

		debug_font_verts = &debug_font_buffers[debug_font_backbuffer_index][0];
	}

	void print_text(f32 x, f32 y, const pen::viewport& vp, vec4f colour, const c8* format, ... )
	{
		va_list va;
		va_start( va, format );

		c8 expanded_buffer[512];
        
        pen::string_format_va(expanded_buffer, 512, format, va );

		va_end( va );

		static c8 buffer[99999]; // ~500 chars
		u32 num_quads;
		num_quads = stb_easy_font_print(x, y, expanded_buffer, nullptr, buffer, sizeof(buffer));

		f32* vb = (f32*)&buffer[0];

        u32 start_vertex = font_vert_count;

        if( font_vert_count + num_quads * 6 >= MAX_DEBUG_FONT_VERTS )
        {
            //bail out as we have ran out of buffer
            return;
        }
        
        f32 recipricol_width = 1.0f / (vp.width - vp.x);
        f32 recipricol_height = 1.0f / (vp.height - vp.y);

		for (u32 i = 0; i < num_quads; ++i)
		{
			f32 x[4];
			f32 y[4];

			for (u32 v = 0; v < 4; ++v)
			{
				vec2f ndc_pos = vec2f( vb[0] + vp.x, vb[1] + vp.y );
				
                ndc_pos.x *= recipricol_width;
				ndc_pos.y *= recipricol_height;
                
				ndc_pos = ( ndc_pos * vec2f( 2.0f, 2.0f ) ) - vec2f( 1.0f, 1.0f );

				x[v] = ndc_pos.x;
				y[v] = ndc_pos.y * -1.0f;

				vb += 4;
			}

			//t1
			debug_font_verts[font_vert_count].x = x[0];
			debug_font_verts[font_vert_count].y = y[0];
			font_vert_count++;

			debug_font_verts[font_vert_count].x = x[1];
			debug_font_verts[font_vert_count].y = y[1];
			font_vert_count++;

			debug_font_verts[font_vert_count].x = x[2];
			debug_font_verts[font_vert_count].y = y[2];
			font_vert_count++;

			//2
			debug_font_verts[font_vert_count].x = x[2];
			debug_font_verts[font_vert_count].y = y[2];
			font_vert_count++;

			debug_font_verts[font_vert_count].x = x[3];
			debug_font_verts[font_vert_count].y = y[3];
			font_vert_count++;

			debug_font_verts[font_vert_count].x = x[0];
			debug_font_verts[font_vert_count].y = y[0];
			font_vert_count++;
		}

        for( u32 i = start_vertex; i < font_vert_count; ++i )
        {
            debug_font_verts[ i ].r = colour.x;
            debug_font_verts[ i ].g = colour.y;
            debug_font_verts[ i ].b = colour.z;
            debug_font_verts[ i ].a = colour.w;
        }
	}

	void add_line( vec3f start, vec3f end, vec3f col )
	{
		debug_lines_verts[line_vert_count].x = start.x;
		debug_lines_verts[line_vert_count].y = start.y;
		debug_lines_verts[line_vert_count].z = start.z;
		debug_lines_verts[line_vert_count].w = 1.0f;

		debug_lines_verts[line_vert_count + 1].x = end.x;
		debug_lines_verts[line_vert_count + 1].y = end.y;
		debug_lines_verts[line_vert_count + 1].z = end.z;
		debug_lines_verts[line_vert_count + 1].w = 1.0f;

		for (u32 j = 0; j < 2; ++j)
		{
			debug_lines_verts[line_vert_count + j].r = col.x;
			debug_lines_verts[line_vert_count + j].g = col.y;
			debug_lines_verts[line_vert_count + j].b = col.z;
			debug_lines_verts[line_vert_count + j].a = 1.0f;
		}

		line_vert_count += 2;
	}

	void add_line_transform( vec3f &start, vec3f &end, const vec3f &col, mat4 *matrix )
	{
		f32 w = 1.0f;
		vec3f transformed_s = matrix->homogeneous_multiply( start, &w );

		w = 1.0f;
		vec3f transformed_e = matrix->homogeneous_multiply( end, &w );

		dbg::add_line( transformed_s, transformed_e, col );

		start = transformed_s;
		end = transformed_e;
	}

	void add_coord_space( mat4 mat, f32 size )
	{
		vec3f pos = mat.get_translation( );

		for (u32 i = 0; i < 3; ++i)
		{
			debug_lines_verts[line_vert_count].x = pos.x;
			debug_lines_verts[line_vert_count].y = pos.y;
			debug_lines_verts[line_vert_count].z = pos.z;
			debug_lines_verts[line_vert_count].w = 1.0f;

			debug_lines_verts[line_vert_count + 1].x = pos.x + mat.m[0 + i * 4] * size;
			debug_lines_verts[line_vert_count + 1].y = pos.y + mat.m[1 + i * 4] * size;
			debug_lines_verts[line_vert_count + 1].z = pos.z + mat.m[2 + i * 4] * size;
			debug_lines_verts[line_vert_count + 1].w = 1.0f;

			for (u32 j = 0; j < 2; ++j)
			{
				debug_lines_verts[line_vert_count + j].r = i == 0 ? 1.0f : 0.0f;
				debug_lines_verts[line_vert_count + j].g = i == 1 ? 1.0f : 0.0f;
				debug_lines_verts[line_vert_count + j].b = i == 2 ? 1.0f : 0.0f;
				debug_lines_verts[line_vert_count + j].a = 1.0f;
			}

			line_vert_count += 2;
		}
	}

	void add_point( vec3f point, f32 size, vec3f col )
	{
		vec3f units[6] =
		{
			vec3f( -1.0f, 0.0f, 0.0f ),
			vec3f( 1.0f, 0.0f, 0.0f ),

			vec3f( 0.0f, -1.0f, 0.0f ),
			vec3f( 0.0f, 1.0f, 0.0f ),

			vec3f( 0.0f, 0.0f, -1.0f ),
			vec3f( 0.0f, 0.0f, 1.0f ),
		};

		for (u32 i = 0; i < 6; ++i)
		{
			debug_lines_verts[line_vert_count].x = point.x;
			debug_lines_verts[line_vert_count].y = point.y;
			debug_lines_verts[line_vert_count].z = point.z;
			debug_lines_verts[line_vert_count].w = 1.0f;

			debug_lines_verts[line_vert_count + 1].x = point.x + units[i].x * size;
			debug_lines_verts[line_vert_count + 1].y = point.y + units[i].y * size;
			debug_lines_verts[line_vert_count + 1].z = point.z + units[i].z * size;
			debug_lines_verts[line_vert_count + 1].w = 1.0f;

			for (u32 j = 0; j < 2; ++j)
			{
				debug_lines_verts[line_vert_count + j].r = col.x;
				debug_lines_verts[line_vert_count + j].g = col.y;
				debug_lines_verts[line_vert_count + j].b = col.z;
				debug_lines_verts[line_vert_count + j].a = 1.0f;
			}

			line_vert_count += 2;
		}
	}

	void add_grid( vec3f centre, vec3f size, vec3f divisions )
	{
		vec3f start = centre - size * 0.5f;
		vec3f division_size = size / divisions;

		vec3f current = start;

		f32 grayness = 0.3f;

		for (u32 i = 0; i <= ( u32 ) divisions.x; ++i)
		{
			debug_lines_verts[line_vert_count + 0].x = current.x;
			debug_lines_verts[line_vert_count + 0].y = current.y;
			debug_lines_verts[line_vert_count + 0].z = current.z;
			debug_lines_verts[line_vert_count + 0].w = 1.0f;

			debug_lines_verts[line_vert_count + 1].x = current.x;
			debug_lines_verts[line_vert_count + 1].y = current.y;
			debug_lines_verts[line_vert_count + 1].z = current.z + size.z;
			debug_lines_verts[line_vert_count + 1].w = 1.0f;

			current.x += division_size.x;

			for (u32 j = 0; j < 2; ++j)
			{
				debug_lines_verts[line_vert_count + j].r = grayness;
				debug_lines_verts[line_vert_count + j].g = grayness;
				debug_lines_verts[line_vert_count + j].b = grayness;
				debug_lines_verts[line_vert_count + j].a = 1.0f;
			}

			line_vert_count += 2;
		}

		current = start;

		for (u32 i = 0; i <= ( u32 ) divisions.z; ++i)
		{
			debug_lines_verts[line_vert_count + 0].x = current.x;
			debug_lines_verts[line_vert_count + 0].y = current.y;
			debug_lines_verts[line_vert_count + 0].z = current.z;
			debug_lines_verts[line_vert_count + 0].w = 1.0f;

			debug_lines_verts[line_vert_count + 1].x = current.x + size.x;
			debug_lines_verts[line_vert_count + 1].y = current.y;
			debug_lines_verts[line_vert_count + 1].z = current.z;
			debug_lines_verts[line_vert_count + 1].w = 1.0f;

			current.z += division_size.z;

			for (u32 j = 0; j < 2; ++j)
			{
				debug_lines_verts[line_vert_count + j].r = grayness;
				debug_lines_verts[line_vert_count + j].g = grayness;
				debug_lines_verts[line_vert_count + j].b = grayness;
				debug_lines_verts[line_vert_count + j].a = 1.0f;
			}

			line_vert_count += 2;
		}
	}

	void add_debug_line2f( vec2f start, vec2f end, vec3f colour )
	{
		debug_lines_verts[line_vert_count].x = start.x;
		debug_lines_verts[line_vert_count].y = start.y;
		debug_lines_verts[line_vert_count].z = 0.0f;
		debug_lines_verts[line_vert_count].w = 1.0f;

		debug_lines_verts[line_vert_count + 1].x = end.x;
		debug_lines_verts[line_vert_count + 1].y = end.y;
		debug_lines_verts[line_vert_count + 1].z = 0.0f;
		debug_lines_verts[line_vert_count + 1].w = 1.0f;

		for (u32 i = 0; i < 2; ++i)
		{
			debug_lines_verts[line_vert_count + i].r = colour.x;
			debug_lines_verts[line_vert_count + i].g = colour.y;
			debug_lines_verts[line_vert_count + i].b = colour.z;
			debug_lines_verts[line_vert_count + i].a = 1.0f;
		}

		line_vert_count += 2;
	}

	void add_debug_point_2f( vec2f pos, vec3f colour )
	{
		const f32 size = 2.0f;

		u32 starting_line_vert_count = line_vert_count;

		debug_lines_verts[line_vert_count].x = pos.x - size;
		debug_lines_verts[line_vert_count].y = pos.y - size;
		debug_lines_verts[line_vert_count].z = 0.0f;
		debug_lines_verts[line_vert_count].w = 1.0f;
		line_vert_count++;

		debug_lines_verts[line_vert_count].x = pos.x - size;
		debug_lines_verts[line_vert_count].y = pos.y + size;
		debug_lines_verts[line_vert_count].z = 0.0f;
		debug_lines_verts[line_vert_count].w = 1.0f;
		line_vert_count++;

		debug_lines_verts[line_vert_count].x = pos.x - size;
		debug_lines_verts[line_vert_count].y = pos.y + size;
		debug_lines_verts[line_vert_count].z = 0.0f;
		debug_lines_verts[line_vert_count].w = 1.0f;
		line_vert_count++;

		debug_lines_verts[line_vert_count].x = pos.x + size;
		debug_lines_verts[line_vert_count].y = pos.y + size;
		debug_lines_verts[line_vert_count].z = 0.0f;
		debug_lines_verts[line_vert_count].w = 1.0f;
		line_vert_count++;

		debug_lines_verts[line_vert_count].x = pos.x + size;
		debug_lines_verts[line_vert_count].y = pos.y + size;
		debug_lines_verts[line_vert_count].z = 0.0f;
		debug_lines_verts[line_vert_count].w = 1.0f;
		line_vert_count++;

		debug_lines_verts[line_vert_count].x = pos.x + size;
		debug_lines_verts[line_vert_count].y = pos.y - size;
		debug_lines_verts[line_vert_count].z = 0.0f;
		debug_lines_verts[line_vert_count].w = 1.0f;
		line_vert_count++;

		debug_lines_verts[line_vert_count].x = pos.x + size;
		debug_lines_verts[line_vert_count].y = pos.y - size;
		debug_lines_verts[line_vert_count].z = 0.0f;
		debug_lines_verts[line_vert_count].w = 1.0f;
		line_vert_count++;

		debug_lines_verts[line_vert_count].x = pos.x - size;
		debug_lines_verts[line_vert_count].y = pos.y - size;
		debug_lines_verts[line_vert_count].z = 0.0f;
		debug_lines_verts[line_vert_count].w = 1.0f;
		line_vert_count++;

		for (u32 i = 0; i < 8; ++i)
		{
			debug_lines_verts[starting_line_vert_count + i].r = colour.x;
			debug_lines_verts[starting_line_vert_count + i].g = colour.y;
			debug_lines_verts[starting_line_vert_count + i].b = colour.z;
			debug_lines_verts[starting_line_vert_count + i].a = 1.0f;
		}
	}

	void add_debug_quad( vec2f pos, vec2f size, vec3f colour )
	{
		//calc pos in NDC space
		vec2f ndc_pos = pos;
		ndc_pos.x /= pen_window.width;
		ndc_pos.y /= pen_window.height;
		ndc_pos = (ndc_pos * vec2f(2.0f,2.0f)) - vec2f(1.0f,1.0f);

		//calc size in NDC space
		vec2f ndc_size = size;
		ndc_size.x /= pen_window.width;
		ndc_size.y /= pen_window.height;
		ndc_size = (ndc_size);

		vec2f corners[ 4 ] = 
		{
			ndc_pos + ndc_size * vec2f(-1.0f,-1.0f),
			ndc_pos + ndc_size * vec2f(-1.0f, 1.0f),
			ndc_pos + ndc_size * vec2f( 1.0f, 1.0f),
			ndc_pos + ndc_size * vec2f( 1.0f,-1.0f)
		};

		//tri 1
		debug_font_verts[ font_vert_count ].x = corners[ 0 ].x;
		debug_font_verts[ font_vert_count ].y = corners[ 0 ].y;
		font_vert_count++;

		debug_font_verts[ font_vert_count ].x = corners[ 1 ].x;
		debug_font_verts[ font_vert_count ].y = corners[ 1 ].y;
		font_vert_count++;

		debug_font_verts[ font_vert_count ].x = corners[ 2 ].x;
		debug_font_verts[ font_vert_count ].y = corners[ 2 ].y;
		font_vert_count++;

		//tri 2
		debug_font_verts[ font_vert_count ].x = corners[ 2 ].x;
		debug_font_verts[ font_vert_count ].y = corners[ 2 ].y;
		font_vert_count++;

		debug_font_verts[ font_vert_count ].x = corners[ 3 ].x;
		debug_font_verts[ font_vert_count ].y = corners[ 3 ].y;
		font_vert_count++;

		debug_font_verts[ font_vert_count ].x = corners[ 0 ].x;
		debug_font_verts[ font_vert_count ].y = corners[ 0 ].y;
		font_vert_count++;
	}

}
