#ifndef _camera_h
#define _camera_h

#include "definitions.h"
#include "renderer.h"
#include "vector.h"
#include "matrix.h"

namespace put
{
	enum camera_flags : u32
	{
		CF_INVALIDATED = 1<<1,
        CF_VP_CORRECTED = 1<<2,
	};

	struct camera_cbuffer
	{
		mat4 view_projection;
		mat4 view_matrix;
	};

	struct camera
	{
		vec3f pos = vec3f::zero();
		vec2f rot = vec2f( -0.5f, 0.5f );
        
        f32 fov;
        f32 aspect;
        f32 near;
        f32 far;

		vec3f focus = vec3f::zero();
		f32	  zoom = 60.0f;

		mat4  view;
		mat4  proj;

		u32 cbuffer = (u32)-1;
		u8 flags = 0;
	};

	void camera_create_perspective( camera* p_camera, f32 fov_degrees, f32 aspect_ratio, f32 near_plane, f32 far_plane );
	void camera_create_orthographic( camera* p_camera, f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar );

	void camera_update_modelling( camera* p_camera );
	void camera_update_fly( camera* p_camera );

	void camera_update_shader_constants( camera* p_camera, bool viewport_correction = false );
}

#endif

