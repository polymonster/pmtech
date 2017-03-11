#ifndef _camera_h
#define _camera_h

#include "definitions.h"
#include "renderer.h"
#include "vector.h"
#include "matrix.h"

namespace put
{
	typedef struct camera
	{
		vec3f pos;
		vec2f rot;

		vec3f focus;
		f32	  zoom;

		mat4  view;
		mat4  proj;

	} camera;

	void camera_create_projection( camera* p_camera, f32 fov_degrees, f32 aspect_ratio, f32 near_plane, f32 far_plane );

	void camera_update_modelling( camera* p_camera, vec2f mouse_drag, f32 zoom, u8 middle_mouse, u8 toggle );

	void camera_bake_matices( camera* p_camera );
}

#endif

