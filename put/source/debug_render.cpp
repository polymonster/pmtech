#include "debug_render.h"
#include "pmfx.h"
#include "camera.h"
#include "input.h"
#include "memory.h"
#include "pen_string.h"
#include "stb_easy_font.h"
#include "pen.h"

extern pen::window_creation_params pen_window;

using namespace put;
using namespace pmfx;

namespace put
{
	namespace dbg
	{
#define NUM_VERTEX_BUFFERS 2

		u32 debug_lines_backbuffer_index = 0;
		u32 debug_font_backbuffer_index = 0;

		struct vertex_debug_lines
		{
			float x, y, z, w;
			float r, g, b, a;

		};

		u32 vb_lines;
		u32 line_vert_count = 0;

        vertex_debug_lines*  debug_lines_buffers[NUM_VERTEX_BUFFERS] = { 0 };
		vertex_debug_lines *debug_lines_verts = NULL;

		shader_program* debug_lines_program;

		struct vertex_debug_font
		{
			float x, y;
			float r, g, b, a;
		};

		u32 vb_font;
		u32 font_vert_count = 0;
        
        u32 lines_buffer_size_in_verts = 0;
        u32 font_buffer_size_in_verts = 0;

		vertex_debug_font*  debug_font_buffers[NUM_VERTEX_BUFFERS] = { 0 };
		vertex_debug_font *debug_font_verts = NULL;

        pmfx_handle debug_lines_shader;
        pmfx_handle debug_font_shader;

		void create_shaders()
		{
            debug_lines_shader = pmfx::load("debug_lines");
            debug_font_shader = pmfx::load("debug_font");
		}
        
        void release_lines_buffer()
        {
            pen::renderer_release_buffer(vb_lines);
            
            for( s32 i = 0; i < NUM_VERTEX_BUFFERS; ++i )
                delete[] debug_lines_buffers[i];
        }
        
        void alloc_lines_buffer(u32 num_verts)
        {
            if( num_verts < lines_buffer_size_in_verts )
                return;
            
            vertex_debug_lines*  prev_buffers[NUM_VERTEX_BUFFERS];
            for( s32 i = 0; i < NUM_VERTEX_BUFFERS; ++i )
                prev_buffers[i] = debug_lines_buffers[i];
            
            u32 prev_vb = vb_lines;
            u32 prev_size = sizeof(vertex_debug_lines) * lines_buffer_size_in_verts;
            
            lines_buffer_size_in_verts = num_verts + 2048;
            
            //debug lines buffer
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(vertex_debug_lines) * lines_buffer_size_in_verts;
            bcp.data = NULL;
            
            vb_lines = pen::renderer_create_buffer(bcp);
            
            for( s32 i = 0; i < NUM_VERTEX_BUFFERS; ++i )
                debug_lines_buffers[i] = new vertex_debug_lines[lines_buffer_size_in_verts];
            
            if( prev_buffers[0] )
            {
                for( s32 i = 0; i < NUM_VERTEX_BUFFERS; ++i )
                    pen::memory_cpy(debug_lines_buffers[i], debug_lines_verts, prev_size);
                
                for( s32 i = 0; i < NUM_VERTEX_BUFFERS; ++i )
                    delete[] prev_buffers[i];
                
                pen::renderer_release_buffer(prev_vb);
            }
            
            debug_lines_verts = &debug_lines_buffers[debug_lines_backbuffer_index][0];
        }
        
        void release_font_buffer()
        {
            pen::renderer_release_buffer(vb_font);
            
            for( s32 i = 0; i < NUM_VERTEX_BUFFERS; ++i )
                delete[] debug_font_buffers[i];
        }
        
        void alloc_font_buffer(u32 num_verts)
        {
            if( num_verts < font_buffer_size_in_verts )
                return;
            
            if(debug_font_buffers[0])
                release_font_buffer();
            
            vertex_debug_font*  prev_buffers[NUM_VERTEX_BUFFERS];
            for( s32 i = 0; i < NUM_VERTEX_BUFFERS; ++i )
                prev_buffers[i] = debug_font_buffers[i];
            
            u32 prev_vb = vb_lines;
            u32 prev_size = sizeof(vertex_debug_font) * font_buffer_size_in_verts;
            
            font_buffer_size_in_verts = num_verts + 2048;
            
            //debug lines buffer
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(vertex_debug_font) * font_buffer_size_in_verts;
            bcp.data = NULL;
            
            vb_font = pen::renderer_create_buffer(bcp);
            
            for( s32 i = 0; i < NUM_VERTEX_BUFFERS; ++i )
                debug_font_buffers[i] = new vertex_debug_font[font_buffer_size_in_verts];
            
            if( prev_buffers[0] )
            {
                for( s32 i = 0; i < NUM_VERTEX_BUFFERS; ++i )
                    pen::memory_cpy(debug_font_buffers[i], debug_font_verts, prev_size);
                
                for( s32 i = 0; i < NUM_VERTEX_BUFFERS; ++i )
                    delete[] prev_buffers[i];
                
                pen::renderer_release_buffer(prev_vb);
            }
            
            debug_font_verts = &debug_font_buffers[debug_font_backbuffer_index][0];
        }

		void create_buffers()
		{
            alloc_lines_buffer(10);
            alloc_font_buffer(10);
		}

		void init()
		{
			create_buffers();
			create_shaders();
		}

		void shutdown()
		{
            release_font_buffer();
            release_lines_buffer();
		}

		void render_3d(u32 cb_3dview)
		{
			pen::renderer_update_buffer(vb_lines, &debug_lines_verts[0], sizeof(vertex_debug_lines) * line_vert_count);

            pmfx::set_technique(debug_lines_shader, 0);

			//bind vertex buffer
			pen::renderer_set_vertex_buffer(vb_lines, 0, sizeof(vertex_debug_lines), 0);

			//shader constants and textures
			pen::renderer_set_constant_buffer(cb_3dview, 0, PEN_SHADER_TYPE_VS);

			//draw
			pen::renderer_draw(line_vert_count, 0, PEN_PT_LINELIST);

			//reset 
			line_vert_count = 0;

			debug_lines_backbuffer_index = (debug_lines_backbuffer_index + 1) & NUM_VERTEX_BUFFERS;
			debug_lines_verts = &debug_lines_buffers[debug_lines_backbuffer_index][0];

		}

		void render_2d( )
		{
            //font
			pen::renderer_update_buffer(vb_font, &debug_font_verts[0], sizeof(vertex_debug_font) * font_vert_count);
            pmfx::set_technique(debug_font_shader, 0);
			pen::renderer_set_vertex_buffer(vb_font, 0, sizeof(vertex_debug_font), 0);
			pen::renderer_draw(font_vert_count, 0, PEN_PT_TRIANGLELIST);

			//reset
			font_vert_count = 0;

			debug_font_backbuffer_index = (debug_font_backbuffer_index + 1) & NUM_VERTEX_BUFFERS;
			debug_font_verts = &debug_font_buffers[debug_font_backbuffer_index][0];
		}

		void add_text_2f(const f32 x, const f32 y, const pen::viewport& vp, const vec4f& colour, const c8* format, ...)
		{
			va_list va;
			va_start(va, format);

			c8 expanded_buffer[512];

			pen::string_format_va(expanded_buffer, 512, format, va);

			va_end(va);

			static c8 buffer[99999]; // ~500 chars
			u32 num_quads;
			num_quads = stb_easy_font_print(x, y, expanded_buffer, nullptr, buffer, sizeof(buffer));

			f32* vb = (f32*)&buffer[0];

			u32 start_vertex = font_vert_count;

			f32 recipricol_width = 1.0f / (vp.width - vp.x);
			f32 recipricol_height = 1.0f / (vp.height - vp.y);
            
            alloc_font_buffer(font_vert_count + num_quads*6);

			for (u32 i = 0; i < num_quads; ++i)
			{
				f32 x[4];
				f32 y[4];

				for (u32 v = 0; v < 4; ++v)
				{
					vec2f ndc_pos = vec2f(vb[0] + vp.x, vb[1] + vp.y);

					ndc_pos.x *= recipricol_width;
					ndc_pos.y *= recipricol_height;

					ndc_pos = (ndc_pos * vec2f(2.0f, 2.0f)) - vec2f(1.0f, 1.0f);

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

			for (u32 i = start_vertex; i < font_vert_count; ++i)
			{
				debug_font_verts[i].r = colour.x;
				debug_font_verts[i].g = colour.y;
				debug_font_verts[i].b = colour.z;
				debug_font_verts[i].a = colour.w;
			}
		}

		void add_line(const vec3f& start, const vec3f& end, const vec4f& col)
		{
            alloc_lines_buffer(line_vert_count + 2);
            
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
				debug_lines_verts[line_vert_count + j].a = col.w;
			}

			line_vert_count += 2;
		}
        
        void add_aabb(const vec3f &min, const vec3f& max, const vec4f& col )
        {
            alloc_lines_buffer(line_vert_count + 24);
            
            //sides
            //
            debug_lines_verts[line_vert_count + 0].x = min.x;
            debug_lines_verts[line_vert_count + 0].y = min.y;
            debug_lines_verts[line_vert_count + 0].z = min.z;
            
            debug_lines_verts[line_vert_count + 1].x = min.x;
            debug_lines_verts[line_vert_count + 1].y = max.y;
            debug_lines_verts[line_vert_count + 1].z = min.z;
            
            //
            debug_lines_verts[line_vert_count + 2].x = max.x;
            debug_lines_verts[line_vert_count + 2].y = min.y;
            debug_lines_verts[line_vert_count + 2].z = min.z;
            
            debug_lines_verts[line_vert_count + 3].x = max.x;
            debug_lines_verts[line_vert_count + 3].y = max.y;
            debug_lines_verts[line_vert_count + 3].z = min.z;
            
            //
            debug_lines_verts[line_vert_count + 4].x = max.x;
            debug_lines_verts[line_vert_count + 4].y = min.y;
            debug_lines_verts[line_vert_count + 4].z = max.z;
            
            debug_lines_verts[line_vert_count + 5].x = max.x;
            debug_lines_verts[line_vert_count + 5].y = max.y;
            debug_lines_verts[line_vert_count + 5].z = max.z;
            
            //
            debug_lines_verts[line_vert_count + 6].x = min.x;
            debug_lines_verts[line_vert_count + 6].y = min.y;
            debug_lines_verts[line_vert_count + 6].z = max.z;
            
            debug_lines_verts[line_vert_count + 7].x = min.x;
            debug_lines_verts[line_vert_count + 7].y = max.y;
            debug_lines_verts[line_vert_count + 7].z = max.z;
            
            //top and bottom
            s32 cur_offset = line_vert_count + 8;
            f32 y[2] = { min.y, max.y };
            for( s32 i = 0; i < 2; ++i)
            {
                //
                debug_lines_verts[cur_offset].x = min.x;
                debug_lines_verts[cur_offset].y = y[i];
                debug_lines_verts[cur_offset].z = min.z;
                cur_offset++;
                
                debug_lines_verts[cur_offset].x = max.x;
                debug_lines_verts[cur_offset].y = y[i];
                debug_lines_verts[cur_offset].z = min.z;
                cur_offset++;
                
                //
                debug_lines_verts[cur_offset].x = min.x;
                debug_lines_verts[cur_offset].y = y[i];
                debug_lines_verts[cur_offset].z = min.z;
                cur_offset++;
                
                debug_lines_verts[cur_offset].x = min.x;
                debug_lines_verts[cur_offset].y = y[i];
                debug_lines_verts[cur_offset].z = max.z;
                cur_offset++;
                
                //
                debug_lines_verts[cur_offset].x = max.x;
                debug_lines_verts[cur_offset].y = y[i];
                debug_lines_verts[cur_offset].z = max.z;
                cur_offset++;
                
                debug_lines_verts[cur_offset].x = min.x;
                debug_lines_verts[cur_offset].y = y[i];
                debug_lines_verts[cur_offset].z = max.z;
                cur_offset++;
                
                //
                debug_lines_verts[cur_offset].x = max.x;
                debug_lines_verts[cur_offset].y = y[i];
                debug_lines_verts[cur_offset].z = max.z;
                cur_offset++;
                
                debug_lines_verts[cur_offset].x = max.x;
                debug_lines_verts[cur_offset].y = y[i];
                debug_lines_verts[cur_offset].z = min.z;
                cur_offset++;
            }
            
            //fill in defaults
            u32 num_verts = cur_offset - line_vert_count;
            for( s32 i = 0; i < num_verts; ++i )
            {
                debug_lines_verts[line_vert_count + i].w = 1.0;
                
                debug_lines_verts[line_vert_count + i].r = col.x;
                debug_lines_verts[line_vert_count + i].g = col.y;
                debug_lines_verts[line_vert_count + i].b = col.z;
                debug_lines_verts[line_vert_count + i].a = col.w;
            }
            
            line_vert_count += num_verts;
        }

		void add_line_transform(const vec3f &start, const vec3f &end, const mat4 *matrix, const vec4f &col )
		{
			f32 w = 1.0f;
			vec3f transformed_s = matrix->transform_vector(start, &w);

			w = 1.0f;
			vec3f transformed_e = matrix->transform_vector(end, &w);

			dbg::add_line(transformed_s, transformed_e, col);
		}

		void add_coord_space(const mat4& mat, const f32 size, u32 selected )
		{
            alloc_lines_buffer(line_vert_count + 6);
            
			vec3f pos = mat.get_translation();

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
                    debug_lines_verts[line_vert_count + j].r = i == 0 || (1<<i) & selected ? 1.0f : 0.0f;
					debug_lines_verts[line_vert_count + j].g = i == 1 || (1<<i) & selected ? 1.0f : 0.0f;
					debug_lines_verts[line_vert_count + j].b = i == 2 || (1<<i) & selected ? 1.0f : 0.0f;
					debug_lines_verts[line_vert_count + j].a = 1.0f;
				}

				line_vert_count += 2;
			}
		}

		void add_point(const vec3f& point, f32 size, vec3f col)
		{
            alloc_lines_buffer(line_vert_count + 12);
            
			vec3f units[6] =
			{
				vec3f(-1.0f, 0.0f, 0.0f),
				vec3f(1.0f, 0.0f, 0.0f),

				vec3f(0.0f, -1.0f, 0.0f),
				vec3f(0.0f, 1.0f, 0.0f),

				vec3f(0.0f, 0.0f, -1.0f),
				vec3f(0.0f, 0.0f, 1.0f),
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

		void add_grid(const vec3f& centre, const vec3f& size, const vec3f& divisions)
		{
            alloc_lines_buffer(line_vert_count + divisions.x*2 + divisions.z*2);
            
			vec3f start = centre - size * 0.5f;
			vec3f division_size = size / divisions;

			start.y = centre.y;

			vec3f current = start;

			f32 grayness = 0.3f;

			for (u32 i = 0; i <= (u32)divisions.x; ++i)
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

			for (u32 i = 0; i <= (u32)divisions.z; ++i)
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

		void add_line_2f(const vec2f& start, const vec2f& end, const vec4f&  colour)
		{
            alloc_lines_buffer(line_vert_count + 2);
            
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

		void add_point_2f(const vec2f& pos, const vec4f& colour)
		{
            alloc_lines_buffer(line_vert_count + 8);
            
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

		void add_quad_2f(const vec2f& pos, const vec2f& size, const vec4f& colour)
		{
            alloc_lines_buffer(line_vert_count + 6);
            
			//calc pos in NDC space
			vec2f ndc_pos = pos;
			ndc_pos.x /= pen_window.width;
			ndc_pos.y /= pen_window.height;
			ndc_pos = (ndc_pos * vec2f(2.0f, 2.0f)) - vec2f(1.0f, 1.0f);

			//calc size in NDC space
			vec2f ndc_size = size;
			ndc_size.x /= pen_window.width;
			ndc_size.y /= pen_window.height;
			ndc_size = (ndc_size);

			vec2f corners[4] =
			{
				ndc_pos + ndc_size * vec2f(-1.0f,-1.0f),
				ndc_pos + ndc_size * vec2f(-1.0f, 1.0f),
				ndc_pos + ndc_size * vec2f(1.0f, 1.0f),
				ndc_pos + ndc_size * vec2f(1.0f,-1.0f)
			};

			//tri 1
            s32 start_index = font_vert_count;
			debug_font_verts[font_vert_count].x = corners[0].x;
			debug_font_verts[font_vert_count].y = corners[0].y;
			font_vert_count++;

			debug_font_verts[font_vert_count].x = corners[1].x;
			debug_font_verts[font_vert_count].y = corners[1].y;
			font_vert_count++;

			debug_font_verts[font_vert_count].x = corners[2].x;
			debug_font_verts[font_vert_count].y = corners[2].y;
			font_vert_count++;

			//tri 2
			debug_font_verts[font_vert_count].x = corners[2].x;
			debug_font_verts[font_vert_count].y = corners[2].y;
			font_vert_count++;

			debug_font_verts[font_vert_count].x = corners[3].x;
			debug_font_verts[font_vert_count].y = corners[3].y;
			font_vert_count++;

			debug_font_verts[font_vert_count].x = corners[0].x;
			debug_font_verts[font_vert_count].y = corners[0].y;
			font_vert_count++;
            
            for( s32 i = start_index; i < start_index + 6; ++i )
            {
                debug_font_verts[i].r = colour.x;
                debug_font_verts[i].g = colour.y;
                debug_font_verts[i].b = colour.z;
                debug_font_verts[i].a = colour.w;
            }
		}
	}
}
