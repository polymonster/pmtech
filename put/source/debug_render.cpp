#include "debug_render.h"
#include "camera.h"
#include "hash.h"
#include "input.h"
#include "memory.h"
#include "pen.h"
#include "pen_string.h"
#include "pmfx.h"
#include "stb_easy_font.h"

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
            VB_TRIS  = 1,
            VB_NUM   = 2
        };

        struct vertex_debug_3d
        {
            vec4f pos;
            vec4f col;

            vertex_debug_3d(){};
        };

        struct vertex_debug_2d
        {
            vec2f pos;
            vec4f col;

            vertex_debug_2d(){};
        };

        shader_program* debug_3d_program;

        u32 vb_3d[VB_NUM];
        u32 line_vert_3d_count = 0;
        u32 tri_vert_3d_count  = 0;

        vertex_debug_3d* debug_3d_buffers[VB_NUM] = {0};
        vertex_debug_3d* debug_3d_verts           = debug_3d_buffers[VB_LINES];
        vertex_debug_3d* debug_3d_tris            = debug_3d_buffers[VB_TRIS];

        u32 vb_2d[VB_NUM];
        u32 tri_vert_2d_count  = 0;
        u32 line_vert_2d_count = 0;

        u32 buffer_2d_size_in_verts[VB_NUM] = {0};
        u32 buffer_3d_size_in_verts[VB_NUM] = {0};

        vertex_debug_2d* debug_2d_buffers[VB_NUM] = {0};
        vertex_debug_2d* debug_2d_verts           = debug_2d_buffers[VB_LINES];
        vertex_debug_2d* debug_2d_tris            = debug_2d_buffers[VB_TRIS];

        shader_handle debug_shader;

        void create_shaders()
        {
            debug_shader = pmfx::load_shader("debug");
        }

        void release_3d_buffers()
        {
            for (s32 i = 0; i < VB_NUM; ++i)
            {
                pen::renderer_release_buffer(vb_3d[i]);
                delete[] debug_3d_buffers[i];
            }
        }

        void alloc_3d_buffer(u32 num_verts, u32 buffer_index)
        {
            if (num_verts < buffer_3d_size_in_verts[buffer_index])
                return;

            vertex_debug_3d* prev_buffer = debug_3d_buffers[buffer_index];

            u32 prev_vb   = vb_3d[buffer_index];
            u32 prev_size = sizeof(vertex_debug_3d) * buffer_3d_size_in_verts[buffer_index];

            buffer_3d_size_in_verts[buffer_index] = num_verts + 2048;

            // debug lines buffer
            pen::buffer_creation_params bcp;
            bcp.usage_flags      = PEN_USAGE_DYNAMIC;
            bcp.bind_flags       = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size      = sizeof(vertex_debug_3d) * buffer_3d_size_in_verts[buffer_index];
            bcp.data             = NULL;

            vb_3d[buffer_index] = pen::renderer_create_buffer(bcp);

            debug_3d_buffers[buffer_index] = new vertex_debug_3d[buffer_3d_size_in_verts[buffer_index]];

            if (prev_buffer)
            {
                memcpy(debug_3d_buffers[buffer_index], debug_3d_verts, prev_size);
                delete prev_buffer;

                pen::renderer_release_buffer(prev_vb);
            }

            debug_3d_verts = &debug_3d_buffers[VB_LINES][0];
            debug_3d_tris  = &debug_3d_buffers[VB_TRIS][0];
        }

        void release_2d_buffers()
        {
            for (s32 i = 0; i < VB_NUM; ++i)
            {
                pen::renderer_release_buffer(vb_2d[i]);
                delete[] debug_2d_buffers[i];
            }
        }

        void alloc_2d_buffer(u32 num_verts, u32 buffer_index)
        {
            if (num_verts < buffer_2d_size_in_verts[buffer_index])
                return;

            vertex_debug_2d* prev_buffer = debug_2d_buffers[buffer_index];

            u32 prev_vb   = vb_2d[buffer_index];
            u32 prev_size = sizeof(vertex_debug_2d) * buffer_2d_size_in_verts[buffer_index];

            buffer_2d_size_in_verts[buffer_index] = num_verts + 2048;

            // debug lines buffer
            pen::buffer_creation_params bcp;
            bcp.usage_flags      = PEN_USAGE_DYNAMIC;
            bcp.bind_flags       = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size      = sizeof(vertex_debug_2d) * buffer_2d_size_in_verts[buffer_index];
            bcp.data             = NULL;

            vb_2d[buffer_index] = pen::renderer_create_buffer(bcp);

            debug_2d_buffers[buffer_index] = new vertex_debug_2d[buffer_2d_size_in_verts[buffer_index]];

            if (prev_buffer)
            {
                memcpy(debug_2d_buffers[buffer_index], debug_2d_verts, prev_size);

                delete prev_buffer;

                pen::renderer_release_buffer(prev_vb);
            }

            debug_2d_verts = &debug_2d_buffers[VB_LINES][0];
            debug_2d_tris  = &debug_2d_buffers[VB_TRIS][0];
        }

        void create_buffers()
        {
            for (s32 i = 0; i < VB_NUM; ++i)
            {
                alloc_3d_buffer(2048, i);
                alloc_2d_buffer(2048, i);
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

        void render_3d(u32 cb_3d_view)
        {
            pen::renderer_update_buffer(vb_3d[VB_TRIS], &debug_3d_tris[0], sizeof(vertex_debug_3d) * tri_vert_3d_count);
            pen::renderer_update_buffer(vb_3d[VB_LINES], &debug_3d_verts[0], sizeof(vertex_debug_3d) * line_vert_3d_count);

            static hash_id ID_DEBUG_3D = PEN_HASH("debug_3d");

            pmfx::set_technique(debug_shader, ID_DEBUG_3D, 0);
            pen::renderer_set_constant_buffer(cb_3d_view, 1, PEN_SHADER_TYPE_VS); // gles on ios will crash if not set
            pen::renderer_set_constant_buffer(cb_3d_view, 0, PEN_SHADER_TYPE_VS);

            pen::renderer_set_vertex_buffer(vb_3d[VB_TRIS], 0, sizeof(vertex_debug_3d), 0);
            pen::renderer_draw(tri_vert_3d_count, 0, PEN_PT_TRIANGLELIST);

            pen::renderer_set_vertex_buffer(vb_3d[VB_LINES], 0, sizeof(vertex_debug_3d), 0);
            pen::renderer_draw(line_vert_3d_count, 0, PEN_PT_LINELIST);

            // reset
            tri_vert_3d_count  = 0;
            line_vert_3d_count = 0;
        }

        void render_2d(u32 cb_2d_view)
        {
            pen::renderer_update_buffer(vb_2d[VB_TRIS], &debug_2d_tris[0], sizeof(vertex_debug_2d) * tri_vert_2d_count);
            pen::renderer_update_buffer(vb_2d[VB_LINES], &debug_2d_verts[0], sizeof(vertex_debug_2d) * line_vert_2d_count);

            static hash_id ID_DEBUG_2D = PEN_HASH("debug_2d");

            pmfx::set_technique(debug_shader, ID_DEBUG_2D, 0);
            pen::renderer_set_constant_buffer(cb_2d_view, 1, PEN_SHADER_TYPE_VS);
            pen::renderer_set_constant_buffer(cb_2d_view, 0, PEN_SHADER_TYPE_VS); // gles on ios will crash if not set

            pen::renderer_set_vertex_buffer(vb_2d[VB_TRIS], 0, sizeof(vertex_debug_2d), 0);
            pen::renderer_draw(tri_vert_2d_count, 0, PEN_PT_TRIANGLELIST);

            pen::renderer_set_vertex_buffer(vb_2d[VB_LINES], 0, sizeof(vertex_debug_2d), 0);
            pen::renderer_draw(line_vert_2d_count, 0, PEN_PT_LINELIST);

            // reset
            tri_vert_2d_count  = 0;
            line_vert_2d_count = 0;
        }

        void add_line(const vec3f& start, const vec3f& end, const vec4f& col)
        {
            alloc_3d_buffer(line_vert_3d_count + 2, VB_LINES);

            debug_3d_verts[line_vert_3d_count].pos     = vec4f(start, 1.0f);
            debug_3d_verts[line_vert_3d_count + 1].pos = vec4f(end, 1.0f);

            for (u32 j = 0; j < 2; ++j)
                debug_3d_verts[line_vert_3d_count + j].col = col;

            line_vert_3d_count += 2;
        }

        void add_circle(const vec3f& axis, const vec3f& centre, f32 radius, const vec4f& col)
        {
            add_circle_segment(axis, centre, radius, 0.0f, M_TWO_PI, col);
        }

        void add_circle_segment(const vec3f& axis, const vec3f& centre, f32 radius, f32 min, f32 max, const vec4f& col)
        {
            alloc_3d_buffer(line_vert_3d_count + 24, VB_LINES);

            vec3f right = cross(axis, vec3f::unit_y());
            if (mag(right) < 0.1)
                right = cross(axis, vec3f::unit_z());
            if (mag(right) < 0.1)
                right = cross(axis, vec3f::unit_x());

            vec3f up = cross(axis, right);
            right    = cross(axis, up);

            static const s32 segments   = 16;
            f32              angle      = 0.0;
            f32              angle_step = M_TWO_PI / segments;
            for (s32 i = 0; i < segments; ++i)
            {
                f32 clamped_angle = std::max<f32>(angle, min);
                clamped_angle     = std::min<f32>(angle, max);

                f32 x = cos(clamped_angle);
                f32 y = -sin(clamped_angle);

                vec3f v1 = normalised(vec3f(x, y, 0.0));

                v1 = right * x + up * y;

                angle += angle_step;

                clamped_angle = std::max<f32>(angle, min);
                clamped_angle = std::min<f32>(angle, max);

                x        = cos(clamped_angle);
                y        = -sin(clamped_angle);
                vec3f v2 = normalised(vec3f(x, y, 0.0));

                v2 = right * x + up * y;

                vec3f p1 = centre + v1 * radius;
                vec3f p2 = centre + v2 * radius;

                add_line(p1, p2, col);
            }
        }

        void add_frustum(const vec3f* near_corners, const vec3f* far_corners, const vec4f& col)
        {
            for (s32 i = 0; i < 4; ++i)
            {
                put::dbg::add_line(near_corners[i], far_corners[i], col);

                if (i == 0)
                {
                    put::dbg::add_line(near_corners[i], near_corners[i + 1], col);
                    put::dbg::add_line(near_corners[i], near_corners[i + 2], col);
                    put::dbg::add_line(far_corners[i], far_corners[i + 1], col);
                    put::dbg::add_line(far_corners[i], far_corners[i + 2], col);
                }
                else if (i == 3)
                {
                    put::dbg::add_line(near_corners[i], near_corners[i - 1], col);
                    put::dbg::add_line(near_corners[i], near_corners[i - 2], col);
                    put::dbg::add_line(far_corners[i], far_corners[i - 1], col);
                    put::dbg::add_line(far_corners[i], far_corners[i - 2], col);
                }
            }
        }

        void add_triangle(const vec3f& v1, const vec3f& v2, const vec3f& v3, const vec4f& col)
        {
            put::dbg::add_line(v1, v2, col);
            put::dbg::add_line(v2, v3, col);
            put::dbg::add_line(v3, v1, col);
        }

        void add_aabb(const vec3f& min, const vec3f& max, const vec4f& col)
        {
            alloc_3d_buffer(line_vert_3d_count + 24, VB_LINES);

            // sides
            //
            debug_3d_verts[line_vert_3d_count + 0].pos.x = min.x;
            debug_3d_verts[line_vert_3d_count + 0].pos.y = min.y;
            debug_3d_verts[line_vert_3d_count + 0].pos.z = min.z;

            debug_3d_verts[line_vert_3d_count + 1].pos.x = min.x;
            debug_3d_verts[line_vert_3d_count + 1].pos.y = max.y;
            debug_3d_verts[line_vert_3d_count + 1].pos.z = min.z;

            //
            debug_3d_verts[line_vert_3d_count + 2].pos.x = max.x;
            debug_3d_verts[line_vert_3d_count + 2].pos.y = min.y;
            debug_3d_verts[line_vert_3d_count + 2].pos.z = min.z;

            debug_3d_verts[line_vert_3d_count + 3].pos.x = max.x;
            debug_3d_verts[line_vert_3d_count + 3].pos.y = max.y;
            debug_3d_verts[line_vert_3d_count + 3].pos.z = min.z;

            //
            debug_3d_verts[line_vert_3d_count + 4].pos.x = max.x;
            debug_3d_verts[line_vert_3d_count + 4].pos.y = min.y;
            debug_3d_verts[line_vert_3d_count + 4].pos.z = max.z;

            debug_3d_verts[line_vert_3d_count + 5].pos.x = max.x;
            debug_3d_verts[line_vert_3d_count + 5].pos.y = max.y;
            debug_3d_verts[line_vert_3d_count + 5].pos.z = max.z;

            //
            debug_3d_verts[line_vert_3d_count + 6].pos.x = min.x;
            debug_3d_verts[line_vert_3d_count + 6].pos.y = min.y;
            debug_3d_verts[line_vert_3d_count + 6].pos.z = max.z;

            debug_3d_verts[line_vert_3d_count + 7].pos.x = min.x;
            debug_3d_verts[line_vert_3d_count + 7].pos.y = max.y;
            debug_3d_verts[line_vert_3d_count + 7].pos.z = max.z;

            // top and bottom
            s32 cur_offset = line_vert_3d_count + 8;
            f32 y[2]       = {min.y, max.y};
            for (s32 i = 0; i < 2; ++i)
            {
                //
                debug_3d_verts[cur_offset].pos.x = min.x;
                debug_3d_verts[cur_offset].pos.y = y[i];
                debug_3d_verts[cur_offset].pos.z = min.z;
                cur_offset++;

                debug_3d_verts[cur_offset].pos.x = max.x;
                debug_3d_verts[cur_offset].pos.y = y[i];
                debug_3d_verts[cur_offset].pos.z = min.z;
                cur_offset++;

                //
                debug_3d_verts[cur_offset].pos.x = min.x;
                debug_3d_verts[cur_offset].pos.y = y[i];
                debug_3d_verts[cur_offset].pos.z = min.z;
                cur_offset++;

                debug_3d_verts[cur_offset].pos.x = min.x;
                debug_3d_verts[cur_offset].pos.y = y[i];
                debug_3d_verts[cur_offset].pos.z = max.z;
                cur_offset++;

                //
                debug_3d_verts[cur_offset].pos.x = max.x;
                debug_3d_verts[cur_offset].pos.y = y[i];
                debug_3d_verts[cur_offset].pos.z = max.z;
                cur_offset++;

                debug_3d_verts[cur_offset].pos.x = min.x;
                debug_3d_verts[cur_offset].pos.y = y[i];
                debug_3d_verts[cur_offset].pos.z = max.z;
                cur_offset++;

                //
                debug_3d_verts[cur_offset].pos.x = max.x;
                debug_3d_verts[cur_offset].pos.y = y[i];
                debug_3d_verts[cur_offset].pos.z = max.z;
                cur_offset++;

                debug_3d_verts[cur_offset].pos.x = max.x;
                debug_3d_verts[cur_offset].pos.y = y[i];
                debug_3d_verts[cur_offset].pos.z = min.z;
                cur_offset++;
            }

            // fill in defaults
            u32 num_verts = cur_offset - line_vert_3d_count;
            for (s32 i = 0; i < num_verts; ++i)
            {
                debug_3d_verts[line_vert_3d_count + i].pos.w = 1.0;
                debug_3d_verts[line_vert_3d_count + i].col   = col;
            }

            line_vert_3d_count += num_verts;
        }

        void add_obb(const mat4& matrix, vec4f col)
        {
            u32 start_index = line_vert_3d_count;
            add_aabb(vec3f::one(), -vec3f::one(), col);
            u32 end_index = line_vert_3d_count;

            for (u32 i = start_index; i < end_index; i++)
            {
                debug_3d_verts[i].pos = matrix.transform_vector(debug_3d_verts[i].pos);
            }
        }

        void add_line_transform(const vec3f& start, const vec3f& end, const mat4* matrix, const vec4f& col)
        {
            f32   w             = 1.0f;
            vec3f transformed_s = matrix->transform_vector(start, w);

            w                   = 1.0f;
            vec3f transformed_e = matrix->transform_vector(end, w);

            dbg::add_line(transformed_s, transformed_e, col);
        }

        void add_axis_transform_widget(const mat4& mat, const f32 size, u32 selected_axis, u32 type, const mat4& view,
                                       const mat4& proj, const vec2i& vp)
        {
            add_coord_space(mat, size, selected_axis);

            vec3f p = mat.get_translation();

            static vec3f axis[4] = {vec3f::zero(), mat.get_right(), mat.get_up(), mat.get_fwd()};

            static vec4f colours[4] = {vec4f::one(), vec4f::red(), vec4f::green(), vec4f::blue()};

            vec3f pp[4];

            for (s32 i = 0; i < 4; ++i)
            {
                mat4 view_proj = proj * view;
                pp[i]          = maths::project_to_sc(p + axis[i] * size, view_proj, vp);
            }

            for (s32 i = 0; i < 3; ++i)
            {
                vec2f p2   = vec2f(pp[i + 1].x, pp[i + 1].y);
                vec2f base = vec2f(pp[0].x, pp[0].y);

                vec2f v1 = normalised(p2 - base);

                vec4f col = colours[i + 1];

                if (selected_axis & (1 << i))
                    col = vec4f::one();

                if (type == 2)
                {
                    // translate
                    vec2f pp = perp(v1);

                    static const f32 s = 5.0f;

                    add_tri_2f(p2 - pp * s, p2 + v1 * s, p2 + pp * s, col);
                }
                else if (type == 4)
                {
                    // scale
                    add_quad_2f(p2, vec2f(2.5f, 2.5f), col);
                }
            }
        }

        void add_coord_space(const mat4& mat, const f32 size, u32 selected)
        {
            alloc_3d_buffer(line_vert_3d_count + 6, VB_LINES);

            vec3f pos = mat.get_translation();

            for (u32 i = 0; i < 3; ++i)
            {
                debug_3d_verts[line_vert_3d_count].pos = vec4f(pos, 1.0f);

                debug_3d_verts[line_vert_3d_count + 1].pos.x = pos.x + mat.m[0 + i * 4] * size;
                debug_3d_verts[line_vert_3d_count + 1].pos.y = pos.y + mat.m[1 + i * 4] * size;
                debug_3d_verts[line_vert_3d_count + 1].pos.z = pos.z + mat.m[2 + i * 4] * size;
                debug_3d_verts[line_vert_3d_count + 1].pos.w = 1.0f;

                for (u32 j = 0; j < 2; ++j)
                {
                    debug_3d_verts[line_vert_3d_count + j].col.r = i == 0 || (1 << i) & selected ? 1.0f : 0.0f;
                    debug_3d_verts[line_vert_3d_count + j].col.g = i == 1 || (1 << i) & selected ? 1.0f : 0.0f;
                    debug_3d_verts[line_vert_3d_count + j].col.b = i == 2 || (1 << i) & selected ? 1.0f : 0.0f;
                    debug_3d_verts[line_vert_3d_count + j].col.a = 1.0f;
                }

                line_vert_3d_count += 2;
            }
        }

        void add_point(const vec3f& point, f32 size, const vec4f& col)
        {
            alloc_3d_buffer(line_vert_3d_count + 12, VB_LINES);

            vec3f units[6] = {
                vec3f(-1.0f, 0.0f, 0.0f), vec3f(1.0f, 0.0f, 0.0f),

                vec3f(0.0f, -1.0f, 0.0f), vec3f(0.0f, 1.0f, 0.0f),

                vec3f(0.0f, 0.0f, -1.0f), vec3f(0.0f, 0.0f, 1.0f),
            };

            for (u32 i = 0; i < 6; ++i)
            {
                debug_3d_verts[line_vert_3d_count].pos     = vec4f(point, 1.0f);
                debug_3d_verts[line_vert_3d_count + 1].pos = vec4f(point + units[i] * size, 1.0f);

                for (u32 j = 0; j < 2; ++j)
                {
                    debug_3d_verts[line_vert_3d_count + j].col = col;
                }

                line_vert_3d_count += 2;
            }
        }

        void add_grid(const vec3f& centre, const vec3f& size, const vec3f& divisions)
        {
            alloc_3d_buffer(line_vert_3d_count + divisions.x * 2 + divisions.z * 2, VB_LINES);

            vec3f start         = centre - size * 0.5f;
            vec3f division_size = size / divisions;

            start.y = centre.y;

            vec3f current = start;

            f32 grayness = 0.3f;

            for (u32 i = 0; i <= (u32)divisions.x; ++i)
            {
                grayness = 0.3f;
                if (divisions.x >= 10.0f)
                {
                    if (i % ((u32)divisions.x / 10) == 0)
                        grayness = 0.2f;
                    if (i % ((u32)divisions.x / 5) == 0)
                        grayness = 0.1f;
                }

                debug_3d_verts[line_vert_3d_count + 0].pos = vec4f(current, 1.0f);

                debug_3d_verts[line_vert_3d_count + 1].pos.x = current.x;
                debug_3d_verts[line_vert_3d_count + 1].pos.y = current.y;
                debug_3d_verts[line_vert_3d_count + 1].pos.z = current.z + size.z;
                debug_3d_verts[line_vert_3d_count + 1].pos.w = 1.0f;

                current.x += division_size.x;

                for (u32 j = 0; j < 2; ++j)
                {
                    debug_3d_verts[line_vert_3d_count + j].col = vec4f(grayness, grayness, grayness, 1.0f);
                }

                line_vert_3d_count += 2;
            }

            current = start;

            for (u32 i = 0; i <= (u32)divisions.z; ++i)
            {
                grayness = 0.3f;
                if (divisions.z >= 10.0f)
                {
                    if (i % ((u32)divisions.z / 10) == 0)
                        grayness = 0.2f;
                    if (i % ((u32)divisions.z / 5) == 0)
                        grayness = 0.1f;
                }

                debug_3d_verts[line_vert_3d_count + 0].pos = vec4f(current, 1.0f);

                debug_3d_verts[line_vert_3d_count + 1].pos.x = current.x + size.x;
                debug_3d_verts[line_vert_3d_count + 1].pos.y = current.y;
                debug_3d_verts[line_vert_3d_count + 1].pos.z = current.z;
                debug_3d_verts[line_vert_3d_count + 1].pos.w = 1.0f;

                current.z += division_size.z;

                for (u32 j = 0; j < 2; ++j)
                {
                    debug_3d_verts[line_vert_3d_count + j].col = vec4f(grayness, grayness, grayness, 1.0f);
                }

                line_vert_3d_count += 2;
            }
        }

        void add_plane(const vec3f& point, const vec3f& normal, f32 size, vec4f colour)
        {
            vec3f v = vec3f::unit_y();

            if (dot(normal, v) < 0.1f)
                v = vec3f::unit_z();

            vec3f b = cross(normal, v);
            vec3f t = cross(b, normal);

            vec3f horiz = point - b * size;
            vec3f vert  = point - t * size;
            for (int i = 0; i < size * 2; ++i)
            {
                dbg::add_line(horiz - t * size, horiz + t * size, vec4f::white());
                dbg::add_line(vert - b * size, vert + b * size, vec4f::white());

                horiz += b;
                vert += t;
            }

            dbg::add_line(point, point + normal, colour);
        }

        void add_text_2f(const f32 x, const f32 y, const pen::viewport& vp, const vec4f& colour, const c8* format, ...)
        {
            va_list va;
            va_start(va, format);

            c8 expanded_buffer[512];

            pen::string_format_va(expanded_buffer, 512, format, va);

            va_end(va);

            static c8 buffer[99999]; // ~500 chars
            u32       num_quads;
            num_quads = stb_easy_font_print(x, y, expanded_buffer, nullptr, buffer, sizeof(buffer));

            f32* vb = (f32*)&buffer[0];

            u32 start_vertex = tri_vert_2d_count;

            alloc_2d_buffer(tri_vert_2d_count + num_quads * 6, VB_TRIS);

            for (u32 i = 0; i < num_quads; ++i)
            {
                f32 x[4];
                f32 y[4];

                for (u32 v = 0; v < 4; ++v)
                {
                    vec2f ndc_pos = vec2f(vb[0] + vp.x, vb[1] + vp.y);

                    x[v] = ndc_pos.x;
                    y[v] = vp.height - ndc_pos.y;

                    vb += 4;
                }

                // t1
                debug_2d_tris[tri_vert_2d_count].pos.x = x[0];
                debug_2d_tris[tri_vert_2d_count].pos.y = y[0];
                tri_vert_2d_count++;

                debug_2d_tris[tri_vert_2d_count].pos.x = x[1];
                debug_2d_tris[tri_vert_2d_count].pos.y = y[1];
                tri_vert_2d_count++;

                debug_2d_tris[tri_vert_2d_count].pos.x = x[2];
                debug_2d_tris[tri_vert_2d_count].pos.y = y[2];
                tri_vert_2d_count++;

                // 2
                debug_2d_tris[tri_vert_2d_count].pos.x = x[2];
                debug_2d_tris[tri_vert_2d_count].pos.y = y[2];
                tri_vert_2d_count++;

                debug_2d_tris[tri_vert_2d_count].pos.x = x[3];
                debug_2d_tris[tri_vert_2d_count].pos.y = y[3];
                tri_vert_2d_count++;

                debug_2d_tris[tri_vert_2d_count].pos.x = x[0];
                debug_2d_tris[tri_vert_2d_count].pos.y = y[0];
                tri_vert_2d_count++;
            }

            for (u32 i = start_vertex; i < tri_vert_2d_count; ++i)
            {
                debug_2d_tris[i].col = colour;
            }
        }

        void add_line_2f(const vec2f& start, const vec2f& end, const vec4f& colour)
        {
            alloc_2d_buffer(line_vert_2d_count + 2, VB_LINES);

            debug_2d_verts[line_vert_2d_count].pos     = start;
            debug_2d_verts[line_vert_2d_count + 1].pos = end;

            for (u32 i = 0; i < 2; ++i)
            {
                debug_2d_verts[line_vert_2d_count + i].col = colour;
            }

            line_vert_2d_count += 2;
        }

        void add_point_2f(const vec2f& pos, const vec4f& colour)
        {
            add_quad_2f(pos, vec2f(2.0f, 2.0f), colour);
        }

        void add_tri_2f(const vec2f& p1, const vec2f& p2, const vec2f& p3, const vec4f& colour)
        {
            alloc_2d_buffer(tri_vert_2d_count + 6, VB_TRIS);

            // tri 1
            s32 start_index                      = tri_vert_2d_count;
            debug_2d_tris[tri_vert_2d_count].pos = p1;
            tri_vert_2d_count++;

            debug_2d_tris[tri_vert_2d_count].pos = p2;
            tri_vert_2d_count++;

            debug_2d_tris[tri_vert_2d_count].pos = p3;
            tri_vert_2d_count++;

            for (s32 i = start_index; i < start_index + 3; ++i)
                debug_2d_tris[i].col = colour;
        }

        void add_quad_2f(const vec2f& pos, const vec2f& size, const vec4f& colour)
        {
            alloc_2d_buffer(tri_vert_2d_count + 6, VB_TRIS);

            vec2f corners[4] = {pos + size * vec2f(-1.0f, -1.0f), pos + size * vec2f(-1.0f, 1.0f),
                                pos + size * vec2f(1.0f, 1.0f), pos + size * vec2f(1.0f, -1.0f)};

            // tri 1
            s32 start_index                      = tri_vert_2d_count;
            debug_2d_tris[tri_vert_2d_count].pos = corners[0];
            tri_vert_2d_count++;

            debug_2d_tris[tri_vert_2d_count].pos = corners[1];
            tri_vert_2d_count++;

            debug_2d_tris[tri_vert_2d_count].pos = corners[2];
            tri_vert_2d_count++;

            // tri 2
            debug_2d_tris[tri_vert_2d_count].pos = corners[2];
            tri_vert_2d_count++;

            debug_2d_tris[tri_vert_2d_count].pos = corners[3];
            tri_vert_2d_count++;

            debug_2d_tris[tri_vert_2d_count].pos = corners[0];
            tri_vert_2d_count++;

            for (s32 i = start_index; i < start_index + 6; ++i)
                debug_2d_tris[i].col = colour;
        }
    } // namespace dbg
} // namespace put
