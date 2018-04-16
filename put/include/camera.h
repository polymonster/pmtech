#ifndef _camera_h
#define _camera_h

#include "pen.h"
#include "renderer.h"
#include "maths/vec.h"
#include "maths/mat.h"

namespace put
{
	enum camera_flags : u32
	{
		CF_INVALIDATED = 1 << 1,
		CF_VP_CORRECTED = 1 << 2,
		CF_ORTHO = 1 << 3,
	};

	struct camera_cbuffer
	{
		mat4 view_projection;
		mat4 view_matrix;
		mat4 view_matrix_inverse;
        vec4f view_position;
        vec4f view_direction;
	};

	struct frustum
	{
		vec3f n[6];
		vec3f p[6];

		vec3f corners[2][4];
	};

	struct camera
	{
		vec3f pos = vec3f::zero();
		vec2f rot = vec2f( -0.5f, 0.5f );
        
		f32 fov = 0.0f;
        f32 aspect;
        f32 near_plane;
        f32 far_plane;

		vec3f focus = vec3f::zero();
		f32	  zoom = 60.0f;

		mat4  view;
		mat4  proj;

		u32 cbuffer = (u32)-1;
		u8 flags = 0;

		frustum camera_frustum;
	};

	void camera_create_perspective( camera* p_camera, f32 fov_degrees, f32 aspect_ratio, f32 near_plane, f32 far_plane );
	void camera_create_orthographic( camera* p_camera, f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar );

	void camera_update_projection_matrix(camera* p_camera);

	void camera_update_modelling( camera* p_camera, bool has_focus = true, bool invert_y = false );
	void camera_update_fly( camera* p_camera, bool has_focus = true, bool invert_y = false );

	void camera_update_shader_constants( camera* p_camera, bool viewport_correction = false );
}

#endif

