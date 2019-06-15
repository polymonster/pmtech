// ecs_primitives.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "debug_render.h"
#include "dev_ui.h"
#include "file_system.h"
#include "hash.h"
#include "pen_string.h"
#include "str_utilities.h"

#include "ecs/ecs_resources.h"
#include "ecs/ecs_utilities.h"

namespace put
{
    namespace ecs
    {
        void create_cpu_buffers(geometry_resource* p_geometry, vertex_model* v, u32 num_verts, u16* indices, u32 num_indices)
        {
            // Create position and index buffer of primitives

            p_geometry->cpu_position_buffer = pen::memory_alloc(sizeof(vec4f) * num_verts);
            p_geometry->cpu_index_buffer = pen::memory_alloc(sizeof(u16) * num_indices);
            p_geometry->cpu_vertex_buffer = pen::memory_alloc(sizeof(vertex_model) * num_verts);

            memcpy(p_geometry->cpu_vertex_buffer, v, sizeof(vertex_model) * num_verts);

            vec4f* cpu_positions = (vec4f*)p_geometry->cpu_position_buffer;
            u16*   cpu_indices = (u16*)p_geometry->cpu_index_buffer;

            for (u32 i = 0; i < num_verts; ++i)
                cpu_positions[i] = v[i].pos;

            for (u32 i = 0; i < num_indices; ++i)
                cpu_indices[i] = indices[i];
        }

        void create_quad()
        {
            static const u32 num_verts = 4;
            static const u32 num_indices = 6;

            geometry_resource* p_geometry = new geometry_resource;

            vertex_model v0;
            v0.pos = vec4f(-1.0f, 0.0f, -1.0f, 1.0f);
            v0.normal = vec4f(0.0f, 1.0f, 0.0f, 1.0f);
            v0.uv12 = vec4f(0.0f, 1.0f, 0.0f, 0.0f);
            v0.tangent = vec4f(1.0f, 0.0f, 0.0f, 0.0f);
            v0.bitangent = vec4f(0.0f, 0.0f, 1.0f, 0.0f);

            vertex_model v1;
            v1.pos = vec4f(-1.0f, 0.0f, 1.0f, 1.0f);
            v1.normal = vec4f(0.0f, 1.0f, 0.0f, 1.0f);
            v1.uv12 = vec4f(0.0f, 0.0f, 0.0f, 0.0f);
            v1.tangent = vec4f(1.0f, 0.0f, 0.0f, 0.0f);
            v1.bitangent = vec4f(0.0f, 0.0f, 1.0f, 0.0f);

            vertex_model v2;
            v2.pos = vec4f(1.0f, 0.0f, 1.0f, 1.0f);
            v2.normal = vec4f(0.0f, 1.0f, 0.0f, 1.0f);
            v2.uv12 = vec4f(1.0f, 0.0f, 0.0f, 0.0f);
            v2.tangent = vec4f(1.0f, 0.0f, 0.0f, 0.0f);
            v2.bitangent = vec4f(0.0f, 0.0f, 1.0f, 0.0f);

            vertex_model v3;
            v3.pos = vec4f(1.0f, 0.0f, -1.0f, 1.0f);
            v3.normal = vec4f(0.0f, 1.0f, 0.0f, 1.0f);
            v3.uv12 = vec4f(1.0f, 1.0f, 0.0f, 0.0f);
            v3.tangent = vec4f(1.0f, 0.0f, 0.0f, 0.0f);
            v3.bitangent = vec4f(0.0f, 0.0f, 1.0f, 0.0f);

            vertex_model v[num_verts] = {v0, v1, v2, v3};

            u16 indices[num_indices] = {0, 2, 1, 2, 0, 3};

            // VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_model) * num_verts;
            bcp.data = (void*)v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            p_geometry->index_buffer = pen::renderer_create_buffer(bcp);

            // info
            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof(vertex_model);
            p_geometry->index_type = PEN_FORMAT_R16_UINT;
            p_geometry->min_extents = -vec3f(1.0f, 0.00001f, 1.0f);
            p_geometry->max_extents = vec3f(1.0f, 0.00001f, 1.0f);
            p_geometry->geometry_name = "quad";
            p_geometry->hash = PEN_HASH("quad");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            add_geometry_resource(p_geometry);
        }

        void create_fulscreen_quad()
        {
            static const u32 num_verts = 4;
            static const u32 num_indices = 6;

            geometry_resource* p_geometry = new geometry_resource;

            vertex_2d v[num_verts] = {vec4f(-1.0f, -1.0f, 0.0f, 1.0f), vec4f(0.0f, 1.0f, 0.0f, 0.0f),
                                      vec4f(-1.0f, 1.0f, 0.0f, 1.0f),  vec4f(0.0f, 0.0f, 0.0f, 0.0f),
                                      vec4f(1.0f, 1.0f, 0.0f, 1.0f),   vec4f(1.0f, 0.0f, 0.0f, 0.0f),
                                      vec4f(1.0f, -1.0f, 0.0f, 1.0f),  vec4f(1.0f, 1.0f, 0.0f, 0.0f)};

            u16 indices[num_indices] = {0, 1, 2, 2, 3, 0};

            // VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_2d) * num_verts;
            bcp.data = (void*)v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            p_geometry->index_buffer = pen::renderer_create_buffer(bcp);

            // info
            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof(vertex_2d);
            p_geometry->index_type = PEN_FORMAT_R16_UINT;
            p_geometry->min_extents = -vec3f(1.0f, 1.0f, 0.0f);
            p_geometry->max_extents = vec3f(1.0f, 1.0f, 0.0f);
            p_geometry->geometry_name = "full_screen_quad";
            p_geometry->hash = PEN_HASH("full_screen_quad");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            add_geometry_resource(p_geometry);
        }

        void create_cone_primitive(Str name, float top, float bottom)
        {
            static const s32 segments = 16;

            static const u32   num_verts = (segments + 1) + (segments * 2);
            vertex_model       v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;

            vec3f axis = vec3f::unit_y();
            vec3f right = vec3f::unit_x();

            vec3f up = cross(axis, right);
            right = cross(axis, up);

            vec3f points[segments];
            vec3f tangents[segments];

            f32 angle = 0.0;
            f32 angle_step = M_TWO_PI / segments;
            for (s32 i = 0; i < segments; ++i)
            {
                f32 x = cos(angle);
                f32 y = -sin(angle);

                vec3f v1 = right * x + up * y;

                angle += angle_step;

                x = cos(angle);
                y = -sin(angle);

                vec3f v2 = right * x + up * y;

                points[i] = v1;

                tangents[i] = v2 - v1;
            }

            vec3f bottom_points[segments];
            for (s32 i = 0; i < segments; ++i)
                bottom_points[i] = points[i] + vec3f(0.0f, bottom, 0.0f);

            vec3f top_point = vec3f(0.0, top, 0.0f);

            // bottom face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 vi = i;
                v[vi].pos = vec4f(bottom_points[i], 1.0f);
                v[vi].normal = vec4f(0.0f, -1.0f, 0.0f, 1.0f);
                v[vi].tangent = vec4f(1.0f, 0.0f, 0.0f, 1.0f);
                v[vi].bitangent = vec4f(0.0f, 0.0f, 1.0f, 1.0f);
            }

            // bottom middle
            s32 bm = segments;
            v[bm].pos = vec4f(0.0f, bottom, 0.0f, 1.0f);
            v[bm].normal = vec4f(0.0f, -1.0f, 0.0f, 1.0f);
            v[bm].tangent = vec4f(1.0f, 0.0f, 0.0f, 1.0f);
            v[bm].bitangent = vec4f(0.0f, 0.0f, 1.0f, 1.0f);

            // sides
            for (s32 i = 0; i < segments; ++i)
            {
                s32 start_offset = segments + 1;
                s32 vi = start_offset + (i * 2);

                s32 next = (i + 1) % segments;

                vec3f p1 = bottom_points[i];
                vec3f p2 = top_point;
                vec3f p3 = bottom_points[next];

                vec3f t = p3 - p1;
                vec3f b = p2 - p1;
                vec3f n = cross(t, b);

                v[vi].pos = vec4f(bottom_points[i], 1.0f);
                v[vi + 1].pos = vec4f(top_point, 1.0f);

                for (s32 x = 0; x < 2; ++x)
                {
                    v[vi].normal = vec4f(n, 1.0f);
                    v[vi].tangent = vec4f(t, 1.0f);
                    v[vi].bitangent = vec4f(b, 1.0f);
                }
            }

            const u32 num_indices = (segments * 3) + (segments * 3);
            u16       indices[num_indices] = {0};

            // bottom face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 face_current = i;
                s32 face_next = (i + 1) % segments;

                s32 index_offset = i * 3;

                indices[index_offset + 0] = bm;
                indices[index_offset + 1] = face_current;
                indices[index_offset + 2] = face_next;
            }

            // sides
            for (s32 i = 0; i < segments; ++i)
            {
                s32 sides_offset = segments + 1;

                s32 face_current = sides_offset + (i * 2);
                s32 index_offset = segments * 3 + (i * 3);

                indices[index_offset + 0] = face_current;
                indices[index_offset + 1] = face_current + 1;

                s32 face_next = sides_offset + ((i + 1) % segments) * 2;
                indices[index_offset + 2] = face_next;
            }

            // VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_model) * num_verts;
            bcp.data = (void*)v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            p_geometry->index_buffer = pen::renderer_create_buffer(bcp);

            // info
            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof(vertex_model);
            p_geometry->index_type = PEN_FORMAT_R16_UINT;
            p_geometry->min_extents = vec3f(-1.0f, bottom, -1.0f);
            p_geometry->max_extents = vec3f(1.0f, top, 1.0f);
            p_geometry->geometry_name = name;
            p_geometry->hash = PEN_HASH(name);
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, v, num_verts, indices, num_indices);
            add_geometry_resource(p_geometry);
        }

        void create_capsule_primitive()
        {
            static const s32 segments = 16;
            static const s32 rows = 16;

            static const u32   num_verts = segments * rows;
            vertex_model       v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;

            f32 angle = 0.0;
            f32 angle_step = M_TWO_PI / segments;
            f32 height_step = 2.0f / (rows - 1);

            s32 v_index = 0;

            f32 y = -1.0f;
            for (s32 r = 0; r < rows; ++r)
            {
                for (s32 i = 0; i < segments; ++i)
                {
                    f32 x = cos(angle);
                    f32 z = -sin(angle);

                    angle += angle_step;

                    f32 radius = 1.0f - fabs(y);

                    vec3f xz = vec3f(x, 0.0f, z) * radius;
                    vec3f p = vec3f(xz.x, y, xz.z);

                    // tangent
                    x = cos(angle);
                    z = -sin(angle);

                    xz = vec3f(x, 0.0f, z) * radius;

                    vec3f p_next = vec3f(xz.x, y + height_step, xz.z);

                    p = normalised(p);
                    p_next = normalised(p_next);

                    vec3f n = p;

                    if (r < segments / 2.0f)
                    {
                        p.y -= 0.5f;
                        p_next -= 0.5f;
                    }
                    else if (r > segments / 2.0f)
                    {
                        p.y += 0.5f;
                        p_next += 0.5f;
                    }

                    if (fabs(r - (segments / 2.0f)) < 2.0f)
                        n = normalised(xz);

                    vec3f t = normalised(p_next - p);
                    vec3f bt = cross(p, t);

                    v[v_index].pos = vec4f(p, 1.0f);
                    v[v_index].normal = vec4f(n, 1.0f);
                    v[v_index].tangent = vec4f(t, 1.0f);
                    v[v_index].bitangent = vec4f(bt, 1.0f);

                    v_index++;
                }

                y += height_step;
            }

            static const u32 num_indices = 6 * (rows - 1) * segments;
            u16              indices[num_indices] = {0};
            s32              index_offset = 0;

            for (s32 r = 0; r < rows - 1; ++r)
            {
                for (s32 i = 0; i < segments; ++i)
                {
                    s32 i_next = ((i + 1) % segments);

                    s32 v_index = r * segments;
                    s32 v_next_index = (r + 1) * segments + i;
                    s32 v_next_next_index = (r + 1) * segments + i_next;

                    indices[index_offset + 0] = v_index + i;
                    indices[index_offset + 1] = v_next_index;
                    indices[index_offset + 2] = v_index + i_next;

                    indices[index_offset + 3] = v_next_index;
                    indices[index_offset + 4] = v_next_next_index;
                    indices[index_offset + 5] = v_index + i_next;

                    index_offset += 6;
                }
            }

            // VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_model) * num_verts;
            bcp.data = (void*)v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            p_geometry->index_buffer = pen::renderer_create_buffer(bcp);

            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof(vertex_model);
            p_geometry->index_type = PEN_FORMAT_R16_UINT;

            p_geometry->min_extents = vec3f(-1.0f, -1.5f, -1.0f);
            p_geometry->max_extents = vec3f(1.0f, 1.5f, 1.0f);

            // hash / ids
            p_geometry->geometry_name = "capsule";
            p_geometry->hash = PEN_HASH("capsule");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, v, num_verts, indices, num_indices);
            add_geometry_resource(p_geometry);
        }

        void create_sphere_primitive()
        {
            static const s32 segments = 16;
            static const s32 rows = 16;

            static const u32   num_verts = segments * rows;
            vertex_model       v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;

            f32 angle = 0.0;
            f32 angle_step = M_TWO_PI / segments;
            f32 height_step = 2.0f / (segments - 1);

            s32 v_index = 0;

            f32 y = -1.0f;
            for (s32 r = 0; r < rows; ++r)
            {
                for (s32 i = 0; i < segments; ++i)
                {
                    f32 x = cos(angle);
                    f32 z = -sin(angle);

                    angle += angle_step;

                    f32 radius = 1.0f - fabs(y);

                    vec3f xz = vec3f(x, 0.0f, z) * radius;
                    vec3f p = vec3f(xz.x, y, xz.z);

                    // tangent
                    x = cos(angle);
                    z = -sin(angle);

                    xz = vec3f(x, 0.0f, z) * radius;

                    vec3f p_next = vec3f(xz.x, y, xz.z);
                    p_next = normalised(p_next);

                    p = normalised(p);
                    vec3f t = p_next - p;
                    vec3f bt = cross(p, t);

                    v[v_index].pos = vec4f(p, 1.0f);
                    v[v_index].normal = vec4f(p, 1.0f);
                    v[v_index].tangent = vec4f(t, 1.0f);
                    v[v_index].bitangent = vec4f(bt, 1.0f);

                    v_index++;
                }

                y += height_step;
            }

            static const u32 num_indices = 6 * (rows - 1) * segments;
            u16              indices[num_indices] = {0};
            s32              index_offset = 0;

            for (s32 r = 0; r < rows - 1; ++r)
            {
                for (s32 i = 0; i < segments; ++i)
                {
                    s32 i_next = ((i + 1) % segments);

                    s32 v_index = r * segments;
                    s32 v_next_index = (r + 1) * segments + i;
                    s32 v_next_next_index = (r + 1) * segments + i_next;

                    indices[index_offset + 0] = v_index + i;
                    indices[index_offset + 1] = v_next_index;
                    indices[index_offset + 2] = v_index + i_next;

                    indices[index_offset + 3] = v_next_index;
                    indices[index_offset + 4] = v_next_next_index;
                    indices[index_offset + 5] = v_index + i_next;

                    index_offset += 6;
                }
            }

            // VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_model) * num_verts;
            bcp.data = (void*)v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            p_geometry->index_buffer = pen::renderer_create_buffer(bcp);

            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof(vertex_model);
            p_geometry->index_type = PEN_FORMAT_R16_UINT;

            p_geometry->min_extents = -vec3f::one();
            p_geometry->max_extents = vec3f::one();

            // hash / ids
            p_geometry->geometry_name = "sphere";
            p_geometry->hash = PEN_HASH("sphere");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, v, num_verts, indices, num_indices);
            add_geometry_resource(p_geometry);
        }

        void create_cube_primitive()
        {
            static const u32   num_verts = 24;
            vertex_model       v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;

            // 3 ------ 2
            //|        |
            //|        |
            // 0 ------ 1

            // 7 ------ 6
            //|        |
            //|        |
            // 4 ------ 5

            // clang-format off
            vec3f corners[] = {
                vec3f(-1.0f, -1.0f, -1.0f),
                vec3f(1.0f, -1.0f, -1.0f),
                vec3f(1.0f, -1.0f, 1.0f),
                vec3f(-1.0f, -1.0f, 1.0f),
                vec3f(-1.0f, 1.0f, -1.0f),
                vec3f(1.0f, 1.0f, -1.0f),
                vec3f(1.0f, 1.0f, 1.0f),
                vec3f(-1.0f, 1.0f, 1.0f)
            };

            vec3f face_normals[] = {
                vec3f(0.0f, -1.0f, 0.0f),
                vec3f(0.0f, 0.0f, -1.0f),
                vec3f(0.0f, 0.0f, 1.0f),
                vec3f(0.0f, 1.0f, 0.0f),
                vec3f(-1.0f, 0.0f, 0.0f),
                vec3f(1.0f, 0.0f, 0.0f)
            };

            vec3f face_tangents[] = {
                vec3f(-1.0f, 0.0f, 0.0f),
                vec3f(-1.0f, 0.0f, -1.0f),
                vec3f(1.0f, 0.0f, 0.0f),
                vec3f(1.0f, 0.0f, 0.0f),
                vec3f(0.0f, 0.0f, -1.0f),
                vec3f(0.0f, 0.0f, 1.0f)
            };
            
            vec2f corner_uv[] = {
                vec2f(0.0f, 0.0f),
                vec2f(1.0f, 0.0f),
                vec2f(1.0f, 1.0f),
                vec2f(0.0f, 1.0f),
                vec2f(0.0f, 1.0f),
                vec2f(1.0f, 1.0f),
                vec2f(1.0f, 0.0f),
                vec2f(0.0f, 0.0f),
            };
            
            vec2f corner_uv_x[] = {
                vec2f(0.0f, 0.0f), //
                vec2f(1.0f, 0.0f),
                vec2f(0.0f, 0.0f),
                vec2f(1.0f, 0.0f), //
                vec2f(0.0f, 1.0f), //
                vec2f(1.0f, 1.0f),
                vec2f(0.0f, 1.0f),
                vec2f(1.0f, 1.0f) //
            };

            s32 c[] = {
                0, 3, 2, 1, 0, 1, 5, 4, 3, 7, 6, 2,
                4, 5, 6, 7, 3, 0, 4, 7, 1, 2, 6, 5
            };

            // clang-format on

            const u32 num_indices = 36;
            u16       indices[num_indices];

            for (s32 i = 0; i < 6; ++i)
            {
                s32 offset = i * 4;
                s32 index_offset = i * 6;

                vec3f bt = cross(face_normals[i], face_tangents[i]);

                for (s32 j = 0; j < 4; ++j)
                {
                    s32 cc = c[offset + j];

                    vec2f uv = corner_uv[cc];
                    if (i >= 4)
                        uv = corner_uv_x[cc];

                    v[offset + j].pos = vec4f(corners[cc], 1.0f);
                    v[offset + j].normal = vec4f(face_normals[i], 1.0f);
                    v[offset + j].tangent = vec4f(face_tangents[i], 1.0f);
                    v[offset + j].bitangent = vec4f(bt, 1.0f);
                    v[offset + j].uv12 = vec4f(uv.x, uv.y, 0.0f, 0.0f);
                }

                indices[index_offset + 0] = offset + 0;
                indices[index_offset + 1] = offset + 1;
                indices[index_offset + 2] = offset + 2;

                indices[index_offset + 3] = offset + 2;
                indices[index_offset + 4] = offset + 3;
                indices[index_offset + 5] = offset + 0;
            }

            // VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_model) * num_verts;
            bcp.data = (void*)v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            p_geometry->index_buffer = pen::renderer_create_buffer(bcp);

            p_geometry->num_indices = 36;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof(vertex_model);
            p_geometry->index_type = PEN_FORMAT_R16_UINT;

            p_geometry->min_extents = -vec3f::one();
            p_geometry->max_extents = vec3f::one();

            // hash / ids
            p_geometry->geometry_name = "cube";
            p_geometry->hash = PEN_HASH("cube");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, v, num_verts, indices, num_indices);
            add_geometry_resource(p_geometry);
        }

        void create_cylinder_primitive()
        {
            static const u32   num_verts = 66;
            vertex_model       v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;

            vec3f axis = vec3f::unit_y();
            vec3f right = vec3f::unit_x();

            vec3f up = cross(axis, right);
            right = cross(axis, up);

            static const s32 segments = 16;

            vec3f points[segments];
            vec3f tangents[segments];

            f32 angle = 0.0;
            f32 angle_step = M_TWO_PI / segments;
            for (s32 i = 0; i < segments; ++i)
            {
                f32 x = cos(angle);
                f32 y = -sin(angle);

                vec3f v1 = right * x + up * y;

                angle += angle_step;

                x = cos(angle);
                y = -sin(angle);

                vec3f v2 = right * x + up * y;

                points[i] = v1;

                tangents[i] = v2 - v1;
            }

            vec3f bottom_points[segments];
            for (s32 i = 0; i < segments; ++i)
                bottom_points[i] = points[i] - vec3f(0.0f, 1.0f, 0.0f);

            vec3f top_points[segments];
            for (s32 i = 0; i < segments; ++i)
                top_points[i] = points[i] + vec3f(0.0f, 1.0f, 0.0f);

            // bottom ring
            for (s32 i = 0; i < segments; ++i)
            {
                vec3f bt = cross(tangents[i], points[i]);

                v[i].pos = vec4f(bottom_points[i], 1.0f);
                v[i].normal = vec4f(points[i], 1.0f);
                v[i].tangent = vec4f(tangents[i], 1.0f);
                v[i].bitangent = vec4f(bt, 1.0f);
            }

            // top ring
            for (s32 i = 0; i < segments; ++i)
            {
                s32   vi = i + segments;
                vec3f bt = cross(tangents[i], points[i]);

                v[vi].pos = vec4f(top_points[i], 1.0f);
                v[vi].normal = vec4f(points[i], 1.0f);
                v[vi].tangent = vec4f(tangents[i], 1.0f);
                v[vi].bitangent = vec4f(bt, 1.0f);
            }

            // bottom face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 vi = (segments * 2) + i;

                v[vi].pos = vec4f(bottom_points[i], 1.0f);
                v[vi].normal = vec4f(0.0f, -1.0f, 0.0f, 1.0f);
                v[vi].tangent = vec4f(1.0f, 0.0f, 1.0f, 1.0f);
                v[vi].bitangent = vec4f(0.0f, 0.0f, 1.0f, 1.0f);
            }

            // top face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 vi = (segments * 3) + i;

                v[vi].pos = vec4f(top_points[i], 1.0f);
                v[vi].normal = vec4f(0.0f, 1.0f, 0.0f, 1.0f);
                v[vi].tangent = vec4f(1.0f, 0.0f, 1.0f, 1.0f);
                v[vi].bitangent = vec4f(0.0f, 0.0f, 1.0f, 1.0f);
            }

            // centre points
            v[64].pos = vec4f(0.0f, -1.0f, 0.0f, 1.0f);
            v[64].normal = vec4f(0.0f, -1.0f, 0.0f, 1.0f);

            v[65].pos = vec4f(0.0f, 1.0f, 0.0f, 1.0f);
            v[65].normal = vec4f(0.0f, 1.0f, 1.0f, 1.0f);

            // sides
            const u32 num_indices = segments * 6 + segments * 3 * 2;
            u16       indices[num_indices] = {0};

            for (s32 i = 0; i < segments; ++i)
            {
                s32 bottom = i;
                s32 top = i + segments;
                s32 next = (i + 1) % segments;
                s32 top_next = ((i + 1) % segments) + segments;

                s32 index_offset = i * 6;

                indices[index_offset + 0] = bottom;
                indices[index_offset + 1] = top;
                indices[index_offset + 2] = next;

                indices[index_offset + 3] = top;
                indices[index_offset + 4] = top_next;
                indices[index_offset + 5] = next;
            }

            // bottom face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 face_offset = (segments * 2);

                s32 face_current = face_offset + i;
                s32 face_next = face_offset + (i + 1) % segments;

                s32 index_offset = i * 3 + (segments * 6);

                indices[index_offset + 0] = 64;
                indices[index_offset + 1] = face_current;
                indices[index_offset + 2] = face_next;
            }

            // top face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 face_offset = (segments * 3);

                s32 face_current = face_offset + i;
                s32 face_next = face_offset + (i + 1) % segments;

                s32 index_offset = i * 3 + (segments * 6) + (segments * 3);

                indices[index_offset + 0] = 65;
                indices[index_offset + 1] = face_next;
                indices[index_offset + 2] = face_current;
            }

            // VB
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_model) * num_verts;
            bcp.data = (void*)v;

            p_geometry->vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            p_geometry->index_buffer = pen::renderer_create_buffer(bcp);

            // info
            p_geometry->num_indices = num_indices;
            p_geometry->num_vertices = num_verts;
            p_geometry->vertex_size = sizeof(vertex_model);
            p_geometry->index_type = PEN_FORMAT_R16_UINT;
            p_geometry->min_extents = -vec3f::one();
            p_geometry->max_extents = vec3f::one();
            p_geometry->geometry_name = "cylinder";
            p_geometry->hash = PEN_HASH("cylinder");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, v, num_verts, indices, num_indices);
            add_geometry_resource(p_geometry);
        }

        void create_geometry_primitives()
        {
            // default material
            material_resource* mr = new material_resource;

            // albedo rgb roughness, spec rgb reflectiviy
            vec4f f = vec4f(0.5f, 0.5f, 0.5f, 0.5f);
            memcpy(&mr->data[0], &f, sizeof(vec4f));
            memcpy(&mr->data[4], &f, sizeof(vec4f));

            mr->material_name = "default_material";
            mr->hash = PEN_HASH("default_material");

            static const u32 default_maps[] = {put::load_texture("data/textures/defaults/albedo.dds"),
                                               put::load_texture("data/textures/defaults/normal.dds"),
                                               put::load_texture("data/textures/defaults/spec.dds"),
                                               put::load_texture("data/textures/defaults/black.dds")};

            for (s32 i = 0; i < 4; ++i)
                mr->texture_handles[i] = default_maps[i];

            add_material_resource(mr);

            // geom primitives
            create_cube_primitive();
            create_cylinder_primitive();
            create_sphere_primitive();
            create_capsule_primitive();
            create_cone_primitive("cone", 0.0f, -1.0f);
            create_cone_primitive("physics_cone", 0.5f, -0.5f);
            create_quad();
            create_fulscreen_quad();
        }
    } // namespace ecs
} // namespace put
