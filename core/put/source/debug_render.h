// debug_render.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

// Minimalist c-style api for rendering debug primitives.
// Adding primitives will push verts into a buffer which will grow to accomodate space as required.
// 2D vertices and 3D vertices are stored in different buffers.
// Calling render_2d or render_3d will flush the buffers and reset them ready for reuse.

#include "maths/maths.h"
#include "pen.h"
#include "renderer.h"

namespace put
{
    namespace dbg
    {
        // Init and shutdown will allocate and free buffer space for primitives
        void init();
        void shutdown();

        // 3d line primitives
        void add_line(const vec3f& start, const vec3f& end, const vec4f& col = vec4f::white());
        void add_coord_space(const mat4& mat, const f32 size, u32 selected = 0);
        void add_point(const vec3f& point, f32 size, const vec4f& col = vec4f::white());
        void add_grid(const vec3f& centre, const vec3f& size, const vec3f& divisions);
        void add_aabb(const vec3f& min, const vec3f& max, const vec4f& col = vec4f(1.0f, 1.0f, 1.0f, 1.0f));
        void add_circle(const vec3f& axis, const vec3f& centre, f32 radius, const vec4f& col = vec4f::white());
        void add_circle_segment(const vec3f& axis, const vec3f& centre, f32 radius, f32 min = 0.0, f32 max = M_TWO_PI,
                                const vec4f& col = vec4f::white());
        void add_frustum(const vec3f* near_corners, const vec3f* far_corners, const vec4f& col = vec4f::white());
        void add_triangle(const vec3f& v1, const vec3f& v2, const vec3f& v3, const vec4f& col = vec4f::white());
        void add_triangle_with_normal(const vec3f& v1, const vec3f& v2, const vec3f& v3, const vec4f& col = vec4f::white());
        void add_plane(const vec3f& point, const vec3f& normal, f32 size = 50.0f, vec4f colour = vec4f::white());
        void add_obb(const mat4& matrix, vec4f colour = vec4f::white());

        // 2d
        void add_line_2f(const vec2f& start, const vec2f& end, const vec4f& colour = vec4f::white());
        void add_point_2f(const vec2f& pos, const vec4f& colour = vec4f::white());
        void add_quad_2f(const vec2f& pos, const vec2f& size, const vec4f& colour = vec4f::white());
        void add_tri_2f(const vec2f& p1, const vec2f& p2, const vec2f& p3, const vec4f& colour = vec4f::white());
        void add_text_2f(const f32 x, const f32 y, const pen::viewport& vp, const vec4f& colour, const c8* format, ...);

        // combinations
        void add_axis_transform_widget(const mat4& mat, const f32 size, u32 selected_axis, u32 type, const mat4& view,
                                       const mat4& proj, const vec2i& vp);

        // cb_xd_view is a constant buffer with view projection matrix
        void render_2d(u32 cb_2d_view);
        void render_3d(u32 cb_3d_view);
    } // namespace dbg
} // namespace put
