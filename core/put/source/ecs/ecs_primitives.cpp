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
        void create_position_only_buffers(geometry_resource* p_geometry, vertex_model* v, u32 num_verts, u16* indices,
                                          u32 num_indices)
        {
            // position only from full vertex
            pmm_renderable& rp = p_geometry->renderable[e_pmm_renderable::position_only];

            // assign from full vertex
            rp = p_geometry->renderable[e_pmm_renderable::full_vertex_buffer];

            // create pos only buffer
            rp.cpu_vertex_buffer = pen::memory_alloc(sizeof(vec4f) * num_verts);
            vec4f* cpu_pos = (vec4f*)rp.cpu_vertex_buffer;
            for (u32 i = 0; i < num_verts; ++i)
                cpu_pos[i] = v[i].pos;

            // create the gpu copy
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vec4f) * num_verts;
            bcp.data = rp.cpu_vertex_buffer;

            rp.vertex_buffer = pen::renderer_create_buffer(bcp);

            // change vertex size
            rp.vertex_size = sizeof(vec4f);
        }

        void create_cpu_buffers(geometry_resource* p_geometry, vertex_model* v, u32 num_verts, u16* indices, u32 num_indices)
        {
            // Create position and index buffer of primitives
            pmm_renderable& r = p_geometry->renderable[e_pmm_renderable::full_vertex_buffer];

            r.cpu_index_buffer = pen::memory_alloc(sizeof(u16) * num_indices);
            r.cpu_vertex_buffer = pen::memory_alloc(sizeof(vertex_model) * num_verts);

            memcpy(r.cpu_vertex_buffer, v, sizeof(vertex_model) * num_verts);
            memcpy(r.cpu_index_buffer, indices, sizeof(u16) * num_indices);
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

            pmm_renderable& r = p_geometry->renderable[e_pmm_renderable::full_vertex_buffer];
            r.vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            r.index_buffer = pen::renderer_create_buffer(bcp);

            r.num_indices = num_indices;
            r.num_vertices = num_verts;
            r.vertex_size = sizeof(vertex_model);
            r.index_type = PEN_FORMAT_R16_UINT;

            // info
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

            pmm_renderable& r = p_geometry->renderable[e_pmm_renderable::full_vertex_buffer];
            r.vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            r.index_buffer = pen::renderer_create_buffer(bcp);

            r.num_indices = num_indices;
            r.num_vertices = num_verts;
            r.vertex_size = sizeof(vertex_2d);
            r.index_type = PEN_FORMAT_R16_UINT;

            // info
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
            f32 angle_step = (f32)M_TWO_PI / segments;
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
                    v[vi + x].normal = vec4f(n, 1.0f);
                    v[vi + x].tangent = vec4f(t, 1.0f);
                    v[vi + x].bitangent = vec4f(b, 1.0f);
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

            pmm_renderable& r = p_geometry->renderable[e_pmm_renderable::full_vertex_buffer];
            r.vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            r.index_buffer = pen::renderer_create_buffer(bcp);

            r.num_indices = num_indices;
            r.num_vertices = num_verts;
            r.vertex_size = sizeof(vertex_model);
            r.index_type = PEN_FORMAT_R16_UINT;

            // info
            p_geometry->min_extents = vec3f(-1.0f, bottom, -1.0f);
            p_geometry->max_extents = vec3f(1.0f, top, 1.0f);
            p_geometry->geometry_name = name;
            p_geometry->hash = PEN_HASH(name);
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, v, num_verts, indices, num_indices);
            create_position_only_buffers(p_geometry, v, num_verts, indices, num_indices);
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

                    // cylindrical normals for the centre cause artifacts
                    /*
                    if (fabs(r - (segments / 2.0f)) < 2.0f)
                        n = normalised(xz);
                    */

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

            pmm_renderable& r = p_geometry->renderable[e_pmm_renderable::full_vertex_buffer];
            r.vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            r.index_buffer = pen::renderer_create_buffer(bcp);

            r.num_indices = num_indices;
            r.num_vertices = num_verts;
            r.vertex_size = sizeof(vertex_model);
            r.index_type = PEN_FORMAT_R16_UINT;

            p_geometry->min_extents = vec3f(-1.0f, -1.5f, -1.0f);
            p_geometry->max_extents = vec3f(1.0f, 1.5f, 1.0f);

            // hash / ids
            p_geometry->geometry_name = "capsule";
            p_geometry->hash = PEN_HASH("capsule");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, v, num_verts, indices, num_indices);
            create_position_only_buffers(p_geometry, v, num_verts, indices, num_indices);
            add_geometry_resource(p_geometry);
        }

        void create_sphere_primitive()
        {
            static const s32 segments = 24;
            static const s32 rows = 24;

            static const u32   num_verts = segments * rows;
            vertex_model       v[num_verts];
            geometry_resource* p_geometry = new geometry_resource;

            f32 angle = 0.0;
            f32 angle_step = (f32)M_TWO_PI / segments;
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

            pmm_renderable& r = p_geometry->renderable[e_pmm_renderable::full_vertex_buffer];
            r.vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            r.index_buffer = pen::renderer_create_buffer(bcp);

            r.num_indices = num_indices;
            r.num_vertices = num_verts;
            r.vertex_size = sizeof(vertex_model);
            r.index_type = PEN_FORMAT_R16_UINT;

            p_geometry->min_extents = -vec3f::one();
            p_geometry->max_extents = vec3f::one();

            // hash / ids
            p_geometry->geometry_name = "sphere";
            p_geometry->hash = PEN_HASH("sphere");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, v, num_verts, indices, num_indices);
            create_position_only_buffers(p_geometry, v, num_verts, indices, num_indices);
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

            pmm_renderable& r = p_geometry->renderable[e_pmm_renderable::full_vertex_buffer];
            r.vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            r.index_buffer = pen::renderer_create_buffer(bcp);

            r.num_indices = 36;
            r.num_vertices = num_verts;
            r.vertex_size = sizeof(vertex_model);
            r.index_type = PEN_FORMAT_R16_UINT;

            p_geometry->min_extents = -vec3f::one();
            p_geometry->max_extents = vec3f::one();

            // hash / ids
            p_geometry->geometry_name = "cube";
            p_geometry->hash = PEN_HASH("cube");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, v, num_verts, indices, num_indices);
            create_position_only_buffers(p_geometry, v, num_verts, indices, num_indices);
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
            f32 angle_step = (f32)M_TWO_PI / segments;
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
                v[vi].tangent = vec4f(1.0f, 0.0f, 0.0f, 1.0f);
                v[vi].bitangent = vec4f(0.0f, 0.0f, 1.0f, 1.0f);
            }

            // top face
            for (s32 i = 0; i < segments; ++i)
            {
                s32 vi = (segments * 3) + i;

                v[vi].pos = vec4f(top_points[i], 1.0f);
                v[vi].normal = vec4f(0.0f, 1.0f, 0.0f, 1.0f);
                v[vi].tangent = vec4f(1.0f, 0.0f, 0.0f, 1.0f);
                v[vi].bitangent = vec4f(0.0f, 0.0f, 1.0f, 1.0f);
            }

            // centre points
            v[64].pos = vec4f(0.0f, -1.0f, 0.0f, 1.0f);
            v[64].normal = vec4f(0.0f, -1.0f, 0.0f, 1.0f);
            v[64].tangent = vec4f(1.0f, 0.0f, 0.0f, 1.0f);
            v[64].bitangent = vec4f(0.0f, 0.0f, 1.0f, 1.0f);

            v[65].pos = vec4f(0.0f, 1.0f, 0.0f, 1.0f);
            v[65].normal = vec4f(0.0f, 1.0f, 0.0f, 1.0f);
            v[65].tangent = vec4f(1.0f, 0.0f, 0.0f, 1.0f);
            v[65].bitangent = vec4f(0.0f, 0.0f, 1.0f, 1.0f);

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

            pmm_renderable& r = p_geometry->renderable[e_pmm_renderable::full_vertex_buffer];
            r.vertex_buffer = pen::renderer_create_buffer(bcp);

            // IB
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * num_indices;
            bcp.data = (void*)indices;

            r.index_buffer = pen::renderer_create_buffer(bcp);

            // info
            r.num_indices = num_indices;
            r.num_vertices = num_verts;
            r.vertex_size = sizeof(vertex_model);
            r.index_type = PEN_FORMAT_R16_UINT;

            p_geometry->min_extents = -vec3f::one();
            p_geometry->max_extents = vec3f::one();
            p_geometry->geometry_name = "cylinder";
            p_geometry->hash = PEN_HASH("cylinder");
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, v, num_verts, indices, num_indices);
            create_position_only_buffers(p_geometry, v, num_verts, indices, num_indices);
            add_geometry_resource(p_geometry);
        }
        
        void create_primitive_resource(Str name, vertex_model* vertices, u16* indices, u32 nv, u32 ni)
        {
            // extents from verts
            vec3f min_extents = vec3f::flt_max();
            vec3f max_extents = -vec3f::flt_max();
            
            for(u32 i = 0; i < nv; i++)
            {
                min_extents = min_union(vertices[i].pos.xyz, min_extents);
                max_extents = max_union(vertices[i].pos.xyz, max_extents);
            }
            
            geometry_resource* p_geometry = new geometry_resource;
            
            // vb
            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = sizeof(vertex_model) * nv;
            bcp.data = (void*)&vertices[0];

            pmm_renderable& r = p_geometry->renderable[e_pmm_renderable::full_vertex_buffer];
            r.vertex_buffer = pen::renderer_create_buffer(bcp);

            // ib
            bcp.usage_flags = PEN_USAGE_DEFAULT;
            bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            bcp.cpu_access_flags = 0;
            bcp.buffer_size = 2 * ni;
            bcp.data = (void*)indices;

            r.index_buffer = pen::renderer_create_buffer(bcp);

            r.num_indices = ni;
            r.num_vertices = nv;
            r.vertex_size = sizeof(vertex_model);
            r.index_type = PEN_FORMAT_R16_UINT;

            // info
            p_geometry->min_extents = min_extents;
            p_geometry->max_extents = max_extents;
            
            p_geometry->geometry_name = name;
            p_geometry->hash = PEN_HASH(name.c_str());
            p_geometry->file_hash = PEN_HASH("primitive");
            p_geometry->filename = "primitive";
            p_geometry->p_skin = nullptr;

            create_cpu_buffers(p_geometry, vertices, nv, indices, ni);
            create_position_only_buffers(p_geometry, vertices, nv, indices, ni);
            add_geometry_resource(p_geometry);
        }
        
        void create_primitive_resource_faceted(Str name, vertex_model* vertices, u32 nv)
        {
            u16* indices = nullptr;
            
            for(u32 i = 0; i < nv; i++)
            {
                sb_push(indices, i);
            }
            
            create_primitive_resource(name, vertices, indices, nv, nv);
            
            sb_free(indices);
        }
        
        void basis_from_axis(const vec3d axis, vec3d& right, vec3d& up, vec3d& at)
        {
            right = cross(axis, vec3d::unit_y());
            
            if (mag(right) < 0.1)
                right = cross(axis, vec3d::unit_z());
            
            if (mag(right) < 0.1)
                right = cross(axis, vec3d::unit_x());
                
            normalise(right);
            up = normalised(cross(axis, right));
            right = normalised(cross(axis, up));
            at = cross(right, up);
        }
        
        void create_terahedron_primitive()
        {
            vec3d axis = vec3d::unit_y();
            vec3d pos = vec3d(0.0, -M_INV_PHI, 0.0);
            
            vertex_model* vertices = nullptr;
            
            vec3d right, up, at;
            basis_from_axis(axis, right, up, at);
                
            vec3d tip = pos - at * sqrt(2.0); // sqrt 2 is pythagoras constant
            
            vec3d base_p[3];
            
            f64 angle_step = (M_PI*2.0) / 3.0;
            f64 a = 0.0f;
            for(u32 i = 0; i < 4; ++i)
            {
                f64 x = sin(a);
                f64 y = cos(a);
                            
                vec3d p = pos + right * x + up * y;
                
                a += angle_step;
                f64 x2 = sin(a);
                f64 y2 = cos(a);
                
                vec3d np = pos + right * x2 + up * y2;
                vec3d tp = tip;
                
                // bottom face
                if(i == 3)
                {
                    p = base_p[0];
                    np = base_p[2];
                    tp = base_p[1];
                }
                else
                {
                    base_p[i] = p;
                }
                
                vec3f n = maths::get_normal((vec3f)p, (vec3f)np, (vec3f)tp);
                vec3f b = (vec3f)normalised(p - np);
                vec3f t = cross(n, b);
                
                vertex_model v[3];
                
                v[0].pos.xyz = (vec3f)np;
                v[1].pos.xyz = (vec3f)p;
                v[2].pos.xyz = (vec3f)tp;

                for(u32 j = 0; j < 3; ++j)
                {
                    v[j].pos.w = 1.0;
                    v[j].normal = vec4f(n, 1.0f);
                    v[j].bitangent = vec4f(b, 1.0f);
                    v[j].tangent = vec4f(t, 1.0f);
                    
                    sb_push(vertices, v[j]);
                }
            }
            
            create_primitive_resource_faceted("tetrahedron", vertices, sb_count(vertices));
            
            sb_free(vertices);
        }
        
        void create_octahedron_primitive()
        {
            vertex_model* vertices = nullptr;
            
            vec3f corner[] = {
                vec3f(-1.0, 0.0, -1.0),
                vec3f(-1.0, 0.0, 1.0),
                vec3f(1.0, 0.0, 1.0),
                vec3f(1.0, 0.0, -1.0)
            };
            
            f32 pc = sqrt(2.0);
            vec3f tip = vec3f(0.0f, pc, 0.0f);
            vec3f dip = vec3f(0.0f, -pc, 0.0f);
            
            for(u32 i = 0; i < 4; ++i)
            {
                u32 n = (i + 1) % 4;
                
                vec3f y[] = {
                    tip,
                    dip
                };
                
                // 2 tris per edg
                for(u32 j = 0; j < 2; ++j)
                {
                    vertex_model v[3];
                    v[0].pos.xyz = corner[i];
                    v[1].pos.xyz = corner[n];
                    v[2].pos.xyz = y[j];
                    
                    if(j == 0)
                    {
                        std::swap(v[0], v[2]);
                    }
                    
                    vec3f n = maths::get_normal(v[0].pos.xyz, v[2].pos.xyz, v[1].pos.xyz);
                    vec3f b = normalised(v[0].pos.xyz - v[1].pos.xyz);
                    vec3f t = cross(n, b);
                    
                    for(u32 k = 0; k < 3; ++k)
                    {
                        v[k].pos.w = 1.0f;
                        v[k].normal = vec4f(n, 1.0f);
                        v[k].tangent = vec4f(t, 1.0f);
                        v[k].bitangent = vec4f(b, 1.0f);
                        
                        sb_push(vertices, v[k]);
                    }
                }
            }
            
            create_primitive_resource_faceted("octahedron", vertices, sb_count(vertices));
            
            sb_free(vertices);
        }
        
        void dodecahedron_face_in_axis(const vec3d axis, const vec3d pos, f64 start_angle, bool recurse, vertex_model*& verts)
        {
            vec3d right, up, at;
            basis_from_axis(axis, right, up, at);
            
            f64 half_gr = 1.61803398875l/2.0;
                
            f64 internal_angle = 0.309017 * 1.5;
            f64 angle_step = M_PI / 2.5;
            f64 a = start_angle;
            for(u32 i = 0; i < 5; ++i)
            {
                f64 x = sin(a) * M_INV_PHI;
                f64 y = cos(a) * M_INV_PHI;
                
                vec3d p = pos + right * x + up * y;
                
                a += angle_step;
                f64 x2 = sin(a) * M_INV_PHI;
                f64 y2 = cos(a) * M_INV_PHI;
                
                vec3d np = pos + right * x2 + up * y2;
                
                // tri per edge
                vertex_model v[3];
                v[0].pos.xyz = p;
                v[1].pos.xyz = np;
                v[2].pos.xyz = pos;
                
                vec3f n = maths::get_normal(v[0].pos.xyz, v[2].pos.xyz, v[1].pos.xyz);
                vec3f b = normalised(v[0].pos.xyz - v[1].pos.xyz);
                vec3f t = cross(n, b);
                
                for(u32 j = 0; j < 3; ++j)
                {
                    v[j].pos.w = 1.0f;
                    v[j].normal = vec4f(n, 1.0f);
                    v[j].tangent = vec4f(t, 1.0f);
                    v[j].bitangent = vec4f(b, 1.0f);
                    
                    sb_push(verts, v[j]);
                }
                            
                vec3d ev = normalised(np - p);
                vec3d cp = normalised(cross(ev, axis));

                vec3d mid = p + (np - p) * 0.5;
                
                f64 rx = sin((M_PI*2.0)+internal_angle) * M_INV_PHI;
                f64 ry = cos((M_PI*2.0)+internal_angle) * M_INV_PHI;
                vec3d xp = mid + cp * rx + axis * ry;
                
                vec3d xv = normalised(xp - mid);

                if(recurse)
                {
                    vec3d next_axis = normalised(cross(xv, ev));
                    dodecahedron_face_in_axis(next_axis, mid + xv * half_gr * M_INV_PHI, M_PI + start_angle, false, verts);
                }
            }
        }
        
        void create_dodecahedron_primitive()
        {
            vec3f axis = vec3f::unit_y();
            
            vertex_model* verts = nullptr;
            
            f32 h = M_PI * 0.83333333333f * 0.5f * M_INV_PHI;
            dodecahedron_face_in_axis((vec3d)axis, vec3d(0.0, -h, 0.0), 0.0f, true, verts);
            dodecahedron_face_in_axis((vec3d)-axis, vec3d(0.0, h, 0.0), M_PI, true, verts);
            
            create_primitive_resource_faceted("dodecahedron", verts, sb_count(verts));
        }
        
        void hemi_icosohedron(const vec3d axis, const vec3d pos, f64 start_angle, vertex_model*& verts)
        {
            vec3d right, up, at;
            basis_from_axis(axis, right, up, at);
            
            vec3d tip = pos - at * M_INV_PHI;
            vec3d dip = pos + at * 0.5 * 2.0;
            
            f64 angle_step = M_PI / 2.5;
            f64 a = start_angle;
            for(u32 i = 0; i < 5; ++i)
            {
                f64 x = sin(a);
                f64 y = cos(a);
                
                vec3d p = pos + right * x + up * y;
                
                a += angle_step;
                f64 x2 = sin(a);
                f64 y2 = cos(a);
                
                vec3d np = pos + right * x2 + up * y2;
                
                // 2 triangles
                vertex_model v[3];
                v[0].pos.xyz = p;
                v[1].pos.xyz = tip;
                v[2].pos.xyz = np;
                
                vec3f n = maths::get_normal(v[0].pos.xyz, v[2].pos.xyz, v[1].pos.xyz);
                vec3f b = normalised(v[0].pos.xyz - v[1].pos.xyz);
                vec3f t = cross(n, b);
                
                for(u32 j = 0; j < 3; ++j)
                {
                    v[j].pos.w = 1.0f;
                    v[j].normal = vec4f(n, 1.0f);
                    v[j].tangent = vec4f(t, 1.0f);
                    v[j].bitangent = vec4f(b, 1.0f);
                    
                    sb_push(verts, v[j]);
                }
                
                vec3d side_dip = dip + cross(normalized(p-np), at);
                
                v[0].pos.xyz = p;
                v[1].pos.xyz = np;
                v[2].pos.xyz = side_dip;
                
                n = maths::get_normal(v[0].pos.xyz, v[2].pos.xyz, v[1].pos.xyz);
                b = normalised(v[0].pos.xyz - v[1].pos.xyz);
                t = cross(n, b);
                
                for(u32 j = 0; j < 3; ++j)
                {
                    v[j].pos.w = 1.0f;
                    v[j].normal = vec4f(n, 1.0f);
                    v[j].tangent = vec4f(t, 1.0f);
                    v[j].bitangent = vec4f(b, 1.0f);
                    
                    sb_push(verts, v[j]);
                }
            }
        }
        
        void create_icosahedron_primitive()
        {
            vec3f axis = vec3f::unit_y();
            
            vertex_model* verts = nullptr;
            
            hemi_icosohedron((vec3d)axis, (vec3d)(axis * 0.5f), 0.0, verts);
            hemi_icosohedron((vec3d)-axis, (vec3d)(-axis * 0.5f), M_PI, verts);
            
            create_primitive_resource_faceted("icosahedron", verts, sb_count(verts));
        }
        
        void create_torus_primitive(f32 radius)
        {
            vertex_model* verts = nullptr;
            
            static const f32 k_segments = 64.0f;
            f64 angle_step = (M_PI*2.0)/k_segments;
            f64 aa = 0.0f;
            for(u32 i = 0; i < k_segments; ++i)
            {
                f64 x = sin(aa);
                f64 y = cos(aa);
                
                aa += angle_step;
                f64 x2 = sin(aa);
                f64 y2 = cos(aa);
                
                f64 x3 = sin(aa + angle_step);
                f64 y3 = cos(aa + angle_step);
                
                vec3f p = vec3f(x, 0.0, y);
                vec3f np = vec3f(x2, 0.0, y2);
                vec3f nnp = vec3f(x3, 0.0, y3);
                
                vec3f at = normalized(np - p);
                vec3f up = vec3f::unit_y();
                vec3f right = cross(up, at);
                
                vec3f nat = normalized(nnp - np);
                vec3f nright = cross(up, nat);
                
                f64 ab = 0.0f;
                for(u32 j = 0; j < 64; ++j)
                {
                    f32 vx = sin(ab) * radius;
                    f32 vy = cos(ab) * radius;
                    
                    vec3f vv = p + vx * up + vy * right;
                    
                    ab += angle_step;
                    
                    f32 vx2 = sin(ab) * radius;
                    f32 vy2 = cos(ab) * radius;
                    
                    vec3f vv2 = p + vx2 * up + vy2 * right;
                    vec3f vv3 = np + vx * up + vy * nright;
                    vec3f vv4 = np + vx2 * up + vy2 * nright;
                    
                    // 2 triangles
                    vertex_model v[6];
                    v[0].pos.xyz = vv;
                    v[2].pos.xyz = vv2;
                    v[1].pos.xyz = vv3;
                    
                    v[3].pos.xyz = vv3;
                    v[4].pos.xyz = vv4;
                    v[5].pos.xyz = vv2;
                    
                    for(u32 k = 0; k < 6; ++k)
                    {
                        v[k].pos.w = 1.0;
                        v[k].normal.xyz = normalized(vx * up + vy * right);
                        v[k].tangent.xyz = right;
                        v[k].bitangent.xyz = up;
                        
                        sb_push(verts, v[k]);
                    }
                }
            }
            
            create_primitive_resource_faceted("torus", verts, sb_count(verts));
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
            
            // ext primitives
            create_terahedron_primitive();
            create_octahedron_primitive();
            create_dodecahedron_primitive();
            create_icosahedron_primitive();
            create_torus_primitive(0.5f);
        }
    } // namespace ecs
} // namespace put
