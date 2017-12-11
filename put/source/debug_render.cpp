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
        enum VERTEX_BUFFER_TYPES
        {
            VB_LINES = 0,
            VB_TRIS = 1,
            VB_NUM = 2
        };

		struct vertex_debug_3d
		{
			float x, y, z, w;
			float r, g, b, a;

		};
        
        struct vertex_debug_2d
        {
            float x, y;
            float r, g, b, a;
        };
        
        shader_program*      debug_3d_program;

        u32 vb_3d[VB_NUM];
		u32 line_vert_3d_count = 0;
        u32 tri_vert_3d_count = 0;

        vertex_debug_3d*     debug_3d_buffers[VB_NUM] = { 0 };
		vertex_debug_3d*     debug_3d_verts = debug_3d_buffers[VB_LINES];
        vertex_debug_3d*     debug_3d_tris = debug_3d_buffers[VB_TRIS];

		u32 vb_2d[VB_NUM];
		u32 tri_vert_2d_count = 0;
        u32 line_vert_2d_count = 0;
        
        u32 buffer_2d_size_in_verts[VB_NUM] = { 0 };
        u32 buffer_3d_size_in_verts[VB_NUM] = { 0 };

		vertex_debug_2d*  debug_2d_buffers[VB_NUM] = { 0 };
        vertex_debug_2d*  debug_2d_verts = debug_2d_buffers[VB_LINES];
        vertex_debug_2d*  debug_2d_tris = debug_2d_buffers[VB_TRIS];

        pmfx_handle       debug_shader;

		void create_shaders()
		{
            debug_shader = pmfx::load("debug");
		}
        
        void release_3d_buffers( )
        {
            for( s32 i = 0; i < VB_NUM; ++i )
            {
                pen::renderer_release_buffer(vb_3d[i]);
                delete[] debug_3d_buffers[i];
            }
        }
        
        void alloc_3d_buffer(u32 num_verts, u32 buffer_index)
        {
            if( num_verts < buffer_3d_size_in_verts[buffer_index] )
                return;
            
            vertex_debug_3d*  prev_buffer = debug_3d_buffers[buffer_index];

            u32 prev_vb = vb_3d[buffer_index];
            u32 prev_size = sizeof(vertex_debug_3d) * buffer_3d_size_in_verts[buffer_index];
            
            buffer_3d_size_in_verts[buffer_index] = num_verts + 2048;
            
            //debug lines buffer
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(vertex_debug_3d) * buffer_3d_size_in_verts[buffer_index];
            bcp.data = NULL;
            
            vb_3d[buffer_index] = pen::renderer_create_buffer(bcp);
            
            debug_3d_buffers[buffer_index] = new vertex_debug_3d[buffer_3d_size_in_verts[buffer_index]];
            
            if( prev_buffer )
            {
                pen::memory_cpy(debug_3d_buffers[buffer_index], debug_3d_verts, prev_size);
                delete prev_buffer;
                
                pen::renderer_release_buffer(prev_vb);
            }
            
            debug_3d_verts = &debug_3d_buffers[VB_LINES][0];
            debug_3d_tris = &debug_3d_buffers[VB_TRIS][0];
        }
        
        void release_2d_buffers()
        {
            for( s32 i = 0; i < VB_NUM; ++i )
            {
                pen::renderer_release_buffer(vb_2d[i]);
                delete[] debug_2d_buffers[i];
            }
        }
        
        void alloc_2d_buffer(u32 num_verts, u32 buffer_index)
        {
            if( num_verts < buffer_2d_size_in_verts[buffer_index] )
                return;
            
            vertex_debug_2d*  prev_buffer = debug_2d_buffers[buffer_index];
            
            u32 prev_vb = vb_2d[buffer_index];
            u32 prev_size = sizeof(vertex_debug_2d) * buffer_2d_size_in_verts[buffer_index];
            
            buffer_2d_size_in_verts[buffer_index] = num_verts + 2048;
            
            //debug lines buffer
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size = sizeof(vertex_debug_2d) * buffer_2d_size_in_verts[buffer_index];
            bcp.data = NULL;
            
            vb_2d[buffer_index] = pen::renderer_create_buffer(bcp);
            
            debug_2d_buffers[buffer_index] = new vertex_debug_2d[buffer_2d_size_in_verts[buffer_index]];
            
            if( prev_buffer )
            {
                pen::memory_cpy(debug_2d_buffers[buffer_index], debug_2d_verts, prev_size);
                
                delete prev_buffer;
                
                pen::renderer_release_buffer(prev_vb);
            }

            debug_2d_verts = &debug_2d_buffers[VB_LINES][0];
            debug_2d_tris = &debug_2d_buffers[VB_TRIS][0];
        }

		void create_buffers()
		{
            for( s32 i = 0; i < VB_NUM; ++i)
            {
                alloc_3d_buffer(10, i);
                alloc_2d_buffer(10, i);
            }
		}

		void init()
		{
			create_buffers();
			create_shaders();
		}

		void shutdown()
		{
            release_2d_buffers();
            release_3d_buffers();
		}

		void render_3d( u32 cb_3d_view )
		{
            pen::renderer_update_buffer(vb_3d[VB_TRIS], &debug_3d_tris[0], sizeof(vertex_debug_3d) * tri_vert_3d_count);
			pen::renderer_update_buffer(vb_3d[VB_LINES], &debug_3d_verts[0], sizeof(vertex_debug_3d) * line_vert_3d_count);
            
            pmfx::set_technique(debug_shader, 1);
            pen::renderer_set_constant_buffer(cb_3d_view, 0, PEN_SHADER_TYPE_VS);
            
            pen::renderer_set_vertex_buffer(vb_3d[VB_TRIS], 0, sizeof(vertex_debug_3d), 0);
			pen::renderer_draw(tri_vert_3d_count, 0, PEN_PT_TRIANGLELIST);
            
            pen::renderer_set_vertex_buffer(vb_3d[VB_LINES], 0, sizeof(vertex_debug_3d), 0);
            pen::renderer_draw(line_vert_3d_count, 0, PEN_PT_LINELIST);

			//reset 
            tri_vert_3d_count = 0;
            line_vert_3d_count = 0;
		}

		void render_2d( u32 cb_2d_view )
		{
			pen::renderer_update_buffer(vb_2d[VB_TRIS], &debug_2d_tris[0], sizeof(vertex_debug_2d) * tri_vert_2d_count);
            pen::renderer_update_buffer(vb_2d[VB_LINES], &debug_2d_verts[0], sizeof(vertex_debug_2d) * line_vert_2d_count);
            
            pmfx::set_technique(debug_shader, 0);
            pen::renderer_set_constant_buffer(cb_2d_view, 1, PEN_SHADER_TYPE_VS);
            
            pen::renderer_set_vertex_buffer(vb_2d[VB_TRIS], 0, sizeof(vertex_debug_2d), 0);
			pen::renderer_draw(tri_vert_2d_count, 0, PEN_PT_TRIANGLELIST);
            
            pen::renderer_set_vertex_buffer(vb_2d[VB_LINES], 0, sizeof(vertex_debug_2d), 0);
            pen::renderer_draw(line_vert_2d_count, 0, PEN_PT_LINELIST);

			//reset
			tri_vert_2d_count = 0;
            line_vert_2d_count = 0;
		}

		void add_line(const vec3f& start, const vec3f& end, const vec4f& col)
		{
            alloc_3d_buffer(line_vert_3d_count + 2, VB_LINES);
            
			debug_3d_verts[line_vert_3d_count].x = start.x;
			debug_3d_verts[line_vert_3d_count].y = start.y;
			debug_3d_verts[line_vert_3d_count].z = start.z;
			debug_3d_verts[line_vert_3d_count].w = 1.0f;

			debug_3d_verts[line_vert_3d_count + 1].x = end.x;
			debug_3d_verts[line_vert_3d_count + 1].y = end.y;
			debug_3d_verts[line_vert_3d_count + 1].z = end.z;
			debug_3d_verts[line_vert_3d_count + 1].w = 1.0f;

			for (u32 j = 0; j < 2; ++j)
			{
				debug_3d_verts[line_vert_3d_count + j].r = col.x;
				debug_3d_verts[line_vert_3d_count + j].g = col.y;
				debug_3d_verts[line_vert_3d_count + j].b = col.z;
				debug_3d_verts[line_vert_3d_count + j].a = col.w;
			}

			line_vert_3d_count += 2;
		}
        
        void add_aabb(const vec3f &min, const vec3f& max, const vec4f& col )
        {
            alloc_3d_buffer(line_vert_3d_count + 24, VB_LINES);
            
            //sides
            //
            debug_3d_verts[line_vert_3d_count + 0].x = min.x;
            debug_3d_verts[line_vert_3d_count + 0].y = min.y;
            debug_3d_verts[line_vert_3d_count + 0].z = min.z;
            
            debug_3d_verts[line_vert_3d_count + 1].x = min.x;
            debug_3d_verts[line_vert_3d_count + 1].y = max.y;
            debug_3d_verts[line_vert_3d_count + 1].z = min.z;
            
            //
            debug_3d_verts[line_vert_3d_count + 2].x = max.x;
            debug_3d_verts[line_vert_3d_count + 2].y = min.y;
            debug_3d_verts[line_vert_3d_count + 2].z = min.z;
            
            debug_3d_verts[line_vert_3d_count + 3].x = max.x;
            debug_3d_verts[line_vert_3d_count + 3].y = max.y;
            debug_3d_verts[line_vert_3d_count + 3].z = min.z;
            
            //
            debug_3d_verts[line_vert_3d_count + 4].x = max.x;
            debug_3d_verts[line_vert_3d_count + 4].y = min.y;
            debug_3d_verts[line_vert_3d_count + 4].z = max.z;
            
            debug_3d_verts[line_vert_3d_count + 5].x = max.x;
            debug_3d_verts[line_vert_3d_count + 5].y = max.y;
            debug_3d_verts[line_vert_3d_count + 5].z = max.z;
            
            //
            debug_3d_verts[line_vert_3d_count + 6].x = min.x;
            debug_3d_verts[line_vert_3d_count + 6].y = min.y;
            debug_3d_verts[line_vert_3d_count + 6].z = max.z;
            
            debug_3d_verts[line_vert_3d_count + 7].x = min.x;
            debug_3d_verts[line_vert_3d_count + 7].y = max.y;
            debug_3d_verts[line_vert_3d_count + 7].z = max.z;
            
            //top and bottom
            s32 cur_offset = line_vert_3d_count + 8;
            f32 y[2] = { min.y, max.y };
            for( s32 i = 0; i < 2; ++i)
            {
                //
                debug_3d_verts[cur_offset].x = min.x;
                debug_3d_verts[cur_offset].y = y[i];
                debug_3d_verts[cur_offset].z = min.z;
                cur_offset++;
                
                debug_3d_verts[cur_offset].x = max.x;
                debug_3d_verts[cur_offset].y = y[i];
                debug_3d_verts[cur_offset].z = min.z;
                cur_offset++;
                
                //
                debug_3d_verts[cur_offset].x = min.x;
                debug_3d_verts[cur_offset].y = y[i];
                debug_3d_verts[cur_offset].z = min.z;
                cur_offset++;
                
                debug_3d_verts[cur_offset].x = min.x;
                debug_3d_verts[cur_offset].y = y[i];
                debug_3d_verts[cur_offset].z = max.z;
                cur_offset++;
                
                //
                debug_3d_verts[cur_offset].x = max.x;
                debug_3d_verts[cur_offset].y = y[i];
                debug_3d_verts[cur_offset].z = max.z;
                cur_offset++;
                
                debug_3d_verts[cur_offset].x = min.x;
                debug_3d_verts[cur_offset].y = y[i];
                debug_3d_verts[cur_offset].z = max.z;
                cur_offset++;
                
                //
                debug_3d_verts[cur_offset].x = max.x;
                debug_3d_verts[cur_offset].y = y[i];
                debug_3d_verts[cur_offset].z = max.z;
                cur_offset++;
                
                debug_3d_verts[cur_offset].x = max.x;
                debug_3d_verts[cur_offset].y = y[i];
                debug_3d_verts[cur_offset].z = min.z;
                cur_offset++;
            }
            
            //fill in defaults
            u32 num_verts = cur_offset - line_vert_3d_count;
            for( s32 i = 0; i < num_verts; ++i )
            {
                debug_3d_verts[line_vert_3d_count + i].w = 1.0;
                
                debug_3d_verts[line_vert_3d_count + i].r = col.x;
                debug_3d_verts[line_vert_3d_count + i].g = col.y;
                debug_3d_verts[line_vert_3d_count + i].b = col.z;
                debug_3d_verts[line_vert_3d_count + i].a = col.w;
            }
            
            line_vert_3d_count += num_verts;
        }

		void add_line_transform(const vec3f &start, const vec3f &end, const mat4 *matrix, const vec4f &col )
		{
			f32 w = 1.0f;
			vec3f transformed_s = matrix->transform_vector(start, &w);

			w = 1.0f;
			vec3f transformed_e = matrix->transform_vector(end, &w);

			dbg::add_line(transformed_s, transformed_e, col);
		}
        
        void add_axis_transform_widget(const mat4& mat, const f32 size, u32 selected_axis, u32 type, const mat4& view, const mat4& proj, const vec2i& vp )
        {
            add_coord_space( mat, size, selected_axis );
            
            vec3f p = mat.get_translation();
            vec3f up = mat.get_up();
            vec3f right = mat.get_right();
            vec3f fwd = mat.get_fwd();
            
            vec3f axis[4] =
            {
                vec3f::zero(),
                mat.get_right(),
                mat.get_up(),
                mat.get_fwd()
            };
            
            vec4f colours[4] =
            {
                vec4f::one(),
                vec4f::red(),
                vec4f::green(),
                vec4f::blue()
            };
            
            vec3f pp[4];
            
            for( s32 i = 0; i < 4; ++i )
            {
                pp[i] = put::maths::project( p + axis[i] * size, view, proj, vp );
            }
            
            for( s32 i = 0; i < 3; ++i )
            {
                vec2f p2 = vec2f( pp[i+1].x, pp[i+1].y );
                vec2f base = vec2f( pp[0].x, pp[0].y );
                
                vec2f v1 = put::maths::normalise( p2 - base );
                
                vec4f col = colours[i+1] * 0.5f;
                
                if( selected_axis & (1<<i) )
                    col = vec4f::one();
                
                if( type == 2 )
                {
                    //translate
                    vec2f perp = put::maths::perp(v1, 0);
                    
                    static const f32 s = 5.0f;
                    
                    add_tri_2f( p2 - perp * s, p2 + v1 * s, p2 + perp * s, col );
                }
                else if( type == 4 )
                {
                    //scale
                    add_quad_2f(p2, vec2f(2.5f, 2.5f), col );
                }
            }
        }

		void add_coord_space(const mat4& mat, const f32 size, u32 selected )
		{
            alloc_3d_buffer(line_vert_3d_count + 6, VB_LINES);
            
			vec3f pos = mat.get_translation();

			for (u32 i = 0; i < 3; ++i)
			{
				debug_3d_verts[line_vert_3d_count].x = pos.x;
				debug_3d_verts[line_vert_3d_count].y = pos.y;
				debug_3d_verts[line_vert_3d_count].z = pos.z;
				debug_3d_verts[line_vert_3d_count].w = 1.0f;

				debug_3d_verts[line_vert_3d_count + 1].x = pos.x + mat.m[0 + i * 4] * size;
				debug_3d_verts[line_vert_3d_count + 1].y = pos.y + mat.m[1 + i * 4] * size;
				debug_3d_verts[line_vert_3d_count + 1].z = pos.z + mat.m[2 + i * 4] * size;
				debug_3d_verts[line_vert_3d_count + 1].w = 1.0f;

				for (u32 j = 0; j < 2; ++j)
				{
                    debug_3d_verts[line_vert_3d_count + j].r = i == 0 || (1<<i) & selected ? 1.0f : 0.0f;
					debug_3d_verts[line_vert_3d_count + j].g = i == 1 || (1<<i) & selected ? 1.0f : 0.0f;
					debug_3d_verts[line_vert_3d_count + j].b = i == 2 || (1<<i) & selected ? 1.0f : 0.0f;
					debug_3d_verts[line_vert_3d_count + j].a = 1.0f;
				}

				line_vert_3d_count += 2;
			}
		}

		void add_point(const vec3f& point, f32 size, vec3f col)
		{
            alloc_3d_buffer(line_vert_3d_count + 12, VB_LINES);
            
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
				debug_3d_verts[line_vert_3d_count].x = point.x;
				debug_3d_verts[line_vert_3d_count].y = point.y;
				debug_3d_verts[line_vert_3d_count].z = point.z;
				debug_3d_verts[line_vert_3d_count].w = 1.0f;

				debug_3d_verts[line_vert_3d_count + 1].x = point.x + units[i].x * size;
				debug_3d_verts[line_vert_3d_count + 1].y = point.y + units[i].y * size;
				debug_3d_verts[line_vert_3d_count + 1].z = point.z + units[i].z * size;
				debug_3d_verts[line_vert_3d_count + 1].w = 1.0f;

				for (u32 j = 0; j < 2; ++j)
				{
					debug_3d_verts[line_vert_3d_count + j].r = col.x;
					debug_3d_verts[line_vert_3d_count + j].g = col.y;
					debug_3d_verts[line_vert_3d_count + j].b = col.z;
					debug_3d_verts[line_vert_3d_count + j].a = 1.0f;
				}

				line_vert_3d_count += 2;
			}
		}

		void add_grid(const vec3f& centre, const vec3f& size, const vec3f& divisions)
		{
            alloc_3d_buffer(line_vert_3d_count + divisions.x*2 + divisions.z*2, VB_LINES);
            
			vec3f start = centre - size * 0.5f;
			vec3f division_size = size / divisions;

			start.y = centre.y;

			vec3f current = start;

			f32 grayness = 0.3f;

			for (u32 i = 0; i <= (u32)divisions.x; ++i)
			{
				debug_3d_verts[line_vert_3d_count + 0].x = current.x;
				debug_3d_verts[line_vert_3d_count + 0].y = current.y;
				debug_3d_verts[line_vert_3d_count + 0].z = current.z;
				debug_3d_verts[line_vert_3d_count + 0].w = 1.0f;

				debug_3d_verts[line_vert_3d_count + 1].x = current.x;
				debug_3d_verts[line_vert_3d_count + 1].y = current.y;
				debug_3d_verts[line_vert_3d_count + 1].z = current.z + size.z;
				debug_3d_verts[line_vert_3d_count + 1].w = 1.0f;

				current.x += division_size.x;

				for (u32 j = 0; j < 2; ++j)
				{
					debug_3d_verts[line_vert_3d_count + j].r = grayness;
					debug_3d_verts[line_vert_3d_count + j].g = grayness;
					debug_3d_verts[line_vert_3d_count + j].b = grayness;
					debug_3d_verts[line_vert_3d_count + j].a = 1.0f;
				}

				line_vert_3d_count += 2;
			}

			current = start;

			for (u32 i = 0; i <= (u32)divisions.z; ++i)
			{
				debug_3d_verts[line_vert_3d_count + 0].x = current.x;
				debug_3d_verts[line_vert_3d_count + 0].y = current.y;
				debug_3d_verts[line_vert_3d_count + 0].z = current.z;
				debug_3d_verts[line_vert_3d_count + 0].w = 1.0f;

				debug_3d_verts[line_vert_3d_count + 1].x = current.x + size.x;
				debug_3d_verts[line_vert_3d_count + 1].y = current.y;
				debug_3d_verts[line_vert_3d_count + 1].z = current.z;
				debug_3d_verts[line_vert_3d_count + 1].w = 1.0f;

				current.z += division_size.z;

				for (u32 j = 0; j < 2; ++j)
				{
					debug_3d_verts[line_vert_3d_count + j].r = grayness;
					debug_3d_verts[line_vert_3d_count + j].g = grayness;
					debug_3d_verts[line_vert_3d_count + j].b = grayness;
					debug_3d_verts[line_vert_3d_count + j].a = 1.0f;
				}

				line_vert_3d_count += 2;
			}
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
            
            u32 start_vertex = tri_vert_2d_count;
            
            alloc_2d_buffer(tri_vert_2d_count + num_quads*6, VB_TRIS);
            
            for (u32 i = 0; i < num_quads; ++i)
            {
                f32 x[4];
                f32 y[4];
                
                for (u32 v = 0; v < 4; ++v)
                {
                    vec2f ndc_pos = vec2f(vb[0] + vp.x, vb[1] + vp.y);
                    
                    x[v] = ndc_pos.x;
                    y[v] = vp.height - ndc_pos.y;;
                    
                    vb += 4;
                }
                
                //t1
                debug_2d_tris[tri_vert_2d_count].x = x[0];
                debug_2d_tris[tri_vert_2d_count].y = y[0];
                tri_vert_2d_count++;
                
                debug_2d_tris[tri_vert_2d_count].x = x[1];
                debug_2d_tris[tri_vert_2d_count].y = y[1];
                tri_vert_2d_count++;
                
                debug_2d_tris[tri_vert_2d_count].x = x[2];
                debug_2d_tris[tri_vert_2d_count].y = y[2];
                tri_vert_2d_count++;
                
                //2
                debug_2d_tris[tri_vert_2d_count].x = x[2];
                debug_2d_tris[tri_vert_2d_count].y = y[2];
                tri_vert_2d_count++;
                
                debug_2d_tris[tri_vert_2d_count].x = x[3];
                debug_2d_tris[tri_vert_2d_count].y = y[3];
                tri_vert_2d_count++;
                
                debug_2d_tris[tri_vert_2d_count].x = x[0];
                debug_2d_tris[tri_vert_2d_count].y = y[0];
                tri_vert_2d_count++;
            }
            
            for (u32 i = start_vertex; i < tri_vert_2d_count; ++i)
            {
                debug_2d_tris[i].r = colour.x;
                debug_2d_tris[i].g = colour.y;
                debug_2d_tris[i].b = colour.z;
                debug_2d_tris[i].a = colour.w;
            }
        }

		void add_line_2f(const vec2f& start, const vec2f& end, const vec4f&  colour)
		{
            alloc_2d_buffer(line_vert_2d_count + 2, VB_LINES);
            
			debug_2d_verts[line_vert_2d_count].x = start.x;
			debug_2d_verts[line_vert_2d_count].y = start.y;

			debug_2d_verts[line_vert_2d_count + 1].x = end.x;
			debug_2d_verts[line_vert_2d_count + 1].y = end.y;

			for (u32 i = 0; i < 2; ++i)
			{
				debug_2d_verts[line_vert_2d_count + i].r = colour.x;
				debug_2d_verts[line_vert_2d_count + i].g = colour.y;
				debug_2d_verts[line_vert_2d_count + i].b = colour.z;
                debug_2d_verts[line_vert_2d_count + i].a = colour.w;
			}

			line_vert_2d_count += 2;
		}

		void add_point_2f(const vec2f& pos, const vec4f& colour)
		{
            add_quad_2f( pos, vec2f( 2.0f, 2.0f), colour );
		}
        
        void add_tri_2f(const vec2f& p1, const vec2f& p2, const vec2f& p3, const vec4f& colour)
        {
            alloc_2d_buffer(tri_vert_2d_count + 6, VB_TRIS);
            
            //tri 1
            s32 start_index = tri_vert_2d_count;
            debug_2d_tris[tri_vert_2d_count].x = p1.x;
            debug_2d_tris[tri_vert_2d_count].y = p1.y;
            tri_vert_2d_count++;
            
            debug_2d_tris[tri_vert_2d_count].x = p2.x;
            debug_2d_tris[tri_vert_2d_count].y = p2.y;
            tri_vert_2d_count++;
            
            debug_2d_tris[tri_vert_2d_count].x = p3.x;
            debug_2d_tris[tri_vert_2d_count].y = p3.y;
            tri_vert_2d_count++;
            
            for( s32 i = start_index; i < start_index + 3; ++i )
            {
                debug_2d_tris[i].r = colour.x;
                debug_2d_tris[i].g = colour.y;
                debug_2d_tris[i].b = colour.z;
                debug_2d_tris[i].a = colour.w;
            }
        }

		void add_quad_2f(const vec2f& pos, const vec2f& size, const vec4f& colour)
		{
            alloc_2d_buffer(tri_vert_2d_count + 6, VB_TRIS);
            
            vec2f corners[4] =
            {
                pos + size * vec2f(-1.0f,-1.0f),
                pos + size * vec2f(-1.0f, 1.0f),
                pos + size * vec2f(1.0f, 1.0f),
                pos + size * vec2f(1.0f,-1.0f)
            };

			//tri 1
            s32 start_index = tri_vert_2d_count;
			debug_2d_tris[tri_vert_2d_count].x = corners[0].x;
			debug_2d_tris[tri_vert_2d_count].y = corners[0].y;
			tri_vert_2d_count++;

			debug_2d_tris[tri_vert_2d_count].x = corners[1].x;
			debug_2d_tris[tri_vert_2d_count].y = corners[1].y;
			tri_vert_2d_count++;

			debug_2d_tris[tri_vert_2d_count].x = corners[2].x;
			debug_2d_tris[tri_vert_2d_count].y = corners[2].y;
			tri_vert_2d_count++;

			//tri 2
			debug_2d_tris[tri_vert_2d_count].x = corners[2].x;
			debug_2d_tris[tri_vert_2d_count].y = corners[2].y;
			tri_vert_2d_count++;

			debug_2d_tris[tri_vert_2d_count].x = corners[3].x;
			debug_2d_tris[tri_vert_2d_count].y = corners[3].y;
			tri_vert_2d_count++;

			debug_2d_tris[tri_vert_2d_count].x = corners[0].x;
			debug_2d_tris[tri_vert_2d_count].y = corners[0].y;
			tri_vert_2d_count++;
            
            for( s32 i = start_index; i < start_index + 6; ++i )
            {
                debug_2d_tris[i].r = colour.x;
                debug_2d_tris[i].g = colour.y;
                debug_2d_tris[i].b = colour.z;
                debug_2d_tris[i].a = colour.w;
            }
		}
	}
}
