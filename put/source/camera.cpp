#include "camera.h"
#include "polyspoon_math.h"

namespace put
{
	void camera_create_projection( camera* p_camera, f32 fov_degrees, f32 aspect_ratio, f32 near_plane, f32 far_plane )
	{
		//calculate the width and height of the near and far planes
		vec2f near_size;
		vec2f far_size;

		near_size.y = 2.0f * tan( psmath::deg_to_rad( fov_degrees ) / 2.0f ) * near_plane;
		near_size.x = near_size.y * aspect_ratio;

		far_size.y = 2.0f * tan( psmath::deg_to_rad( fov_degrees ) / 2.0f ) * far_plane;
		far_size.x = far_size.y * aspect_ratio;

		p_camera->proj.create_perspective_projection
		(
			-near_size.x * 0.5f,
			 near_size.x * 0.5f,
			-near_size.y * 0.5f,
			 near_size.y * 0.5f,
			 near_plane,
			 far_plane
		);
	}

	void camera_update_modelling( camera* p_camera, vec2f mouse_drag, f32 zoom, u8 middle_mouse, u8 toggle )
	{
		if( middle_mouse )
		{
			if (toggle)
			{
				//rotation
				vec2f swapxy = vec2f( mouse_drag.y, mouse_drag.x );
				p_camera->rot += swapxy * ((2 * PI) / 360.0f);
			}
			else
			{
				//pan
				p_camera->focus.x += f32( (cos( p_camera->rot.y ) * mouse_drag.x) + (-(sin( p_camera->rot.x ) * sin( p_camera->rot.y )) * mouse_drag.y) );
				p_camera->focus.y -= f32( cos( p_camera->rot.x ) * mouse_drag.y );
				p_camera->focus.z += f32( (sin( p_camera->rot.y ) * mouse_drag.x) + ((sin( p_camera->rot.x ) * cos( p_camera->rot.y )) * mouse_drag.y) );
			}
		}

		p_camera->zoom += zoom;

		//vector to store the calculated camera pos
		vec3f new_position = p_camera->focus;

		//calculate the unit sphere position
		vec3f unit_vector;
		unit_vector.x += f32( -cos( p_camera->rot.x ) * sin( p_camera->rot.y ) );
		unit_vector.y += f32( sin( p_camera->rot.x ) );
		unit_vector.z += f32( cos( p_camera->rot.x ) * cos( p_camera->rot.y ) );

		//scale the unit vector by zoom
		new_position += (unit_vector * f32( p_camera->zoom ));

		p_camera->pos = new_position;
	}

	void camera_bake_matices( camera* p_camera )
	{
		//f32 xrotrad = psmath::deg_to_rad( p_camera->rot.x );
		//f32 yrotrad = psmath::deg_to_rad( p_camera->rot.y );

		mat4 rx, ry, t;
		rx.create_cardinal_rotation( X_AXIS, p_camera->rot.x );
		ry.create_cardinal_rotation( Y_AXIS, p_camera->rot.y );
		t.create_translation( p_camera->pos * -1.0f );

		p_camera->view = (rx * ry) * t;
	}
}
