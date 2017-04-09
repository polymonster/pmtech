#ifndef debug_render_h__
#define debug_render_h__

#include "definitions.h"
#include "polyspoon_math.h"
#include "renderer.h"

namespace dbg
{
	void initialise( );

	//3d
	void add_line( vec3f start, vec3f end, vec3f col );
	void add_coord_space( mat4 mat, f32 size );
	void add_point( vec3f point, f32 size, vec3f col = vec3f( 0.0f, 0.0f, 1.0f ) );
	void add_grid( vec3f centre, vec3f size, vec3f divisions );
	void add_line_transform( vec3f &start, vec3f &end, const vec3f &col, mat4 *matrix );

	//2d
	void add_line2f( vec2f start, vec2f end, vec3f colour );
	void add_point_2f( vec2f pos, vec3f colour );
	void add_quad_2f( vec2f pos, vec2f size, vec3f colour );

	//text
    void print_text(f32 x, f32 y, const pen::viewport& vp, vec4f colour, const c8* format, ... );

    void render_2d( );
	void render_3d( u32 cb_3dview );
	void render_text();
}

#endif // render_h__
