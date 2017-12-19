#ifndef debug_render_h__
#define debug_render_h__

#include "definitions.h"
#include "put_math.h"
#include "renderer.h"

namespace put
{
	namespace dbg
	{
		void init();
		void shutdown();

		//3d
		void add_line(const vec3f& start, const vec3f& end, const vec4f& col = vec4f::white());
		void add_coord_space( const mat4& mat, const f32 size, u32 selected = 0);
		void add_point( const vec3f& point, f32 size, const vec4f& col = vec4f::white());
		void add_grid( const vec3f& centre, const vec3f& size, const vec3f& divisions);
		void add_line_transform( const vec3f& start, vec3f& end, const mat4 *matrix, const vec4f& col = vec4f::white());
        void add_aabb(const vec3f &min, const vec3f& max, const vec4f& col = vec4f(1.0f, 1.0f, 1.0f, 1.0f) );
        void add_circle(const vec3f& axis, const vec3f& centre, f32 radius, const vec4f& col = vec4f::white());
		void add_frustum(const vec3f* near_corners, const vec3f* far_corners, const vec4f& col = vec4f::white() );

		//2d
		void add_line_2f(const vec2f& start, const vec2f& end, const vec4f& colour = vec4f::white());
		void add_point_2f(const vec2f& pos, const vec4f& colour = vec4f::white());
		void add_quad_2f(const vec2f& pos, const vec2f& size, const vec4f& colour = vec4f::white());
        void add_tri_2f(const vec2f& p1, const vec2f& p2, const vec2f& p3, const vec4f& colour = vec4f::white());
        
		//text
		void add_text_2f(const f32 x, const f32 y, const pen::viewport& vp, const vec4f& colour, const c8* format, ... );
        
        //combinations
        void add_axis_transform_widget(const mat4& mat, const f32 size, u32 selected_axis, u32 type, const mat4& view, const mat4& proj, const vec2i& vp );

		void render_2d(u32 cb_2d_view);
		void render_3d(u32 cb_3d_view);
	}
}

#endif // render_h__
