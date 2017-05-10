#include "camera.h"
#include "polyspoon_math.h"
#include "input.h"
#include "renderer.h"
#include "debug_render.h"

using namespace pen;

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

		p_camera->flags |= CF_INVALIDATED;
	}

	void camera_update_modelling( camera* p_camera )
	{
		mouse_state ms = input_get_mouse_state();

		//mouse drag
		static vec2f prev_mpos = vec2f((f32)ms.x, (f32)ms.y);
		vec2f current_mouse = vec2f((f32)ms.x, (f32)ms.y);
		vec2f mouse_drag = current_mouse - prev_mpos;
		prev_mpos = current_mouse;

		f32 mwheel = (f32)ms.wheel;
		static f32 prev_mwheel = mwheel;
		f32 zoom = mwheel - prev_mwheel;

		if (ms.buttons[PEN_MOUSE_L])
		{
			//rotation
			vec2f swapxy = vec2f(mouse_drag.y, mouse_drag.x);
			p_camera->rot += swapxy * ((2.0f * PI) / 360.0f);
		}
		else if (ms.buttons[PEN_MOUSE_R])
		{
			//pan
			p_camera->focus.x += f32((cos(p_camera->rot.y) * mouse_drag.x) + (-(sin(p_camera->rot.x) * sin(p_camera->rot.y)) * mouse_drag.y));
			p_camera->focus.y -= f32(cos(p_camera->rot.x) * mouse_drag.y);
			p_camera->focus.z += f32((sin(p_camera->rot.y) * mouse_drag.x) + ((sin(p_camera->rot.x) * cos(p_camera->rot.y)) * mouse_drag.y));
		}

		p_camera->zoom += zoom;

		//vector to store the calculated camera pos
		vec3f new_position = p_camera->focus;

		//calculate the unit sphere position
		vec3f unit_vector = vec3f::zero();
		unit_vector.x += f32( -cos( p_camera->rot.x ) * sin( p_camera->rot.y ) );
		unit_vector.y += f32( sin( p_camera->rot.x ) );
		unit_vector.z += f32( cos( p_camera->rot.x ) * cos( p_camera->rot.y ) );

		unit_vector = psmath::normalise(unit_vector);

		//scale the unit vector by zoom
		new_position += (unit_vector * f32(p_camera->zoom));

		//p_camera->pos = new_position;

		vec3f fwd = psmath::normalise(unit_vector * -1.0f);
		vec3f right = psmath::cross(fwd, vec3f::unit_y());
		vec3f up = psmath::cross(right, fwd);

		p_camera->view.set_vectors( right, up, fwd, new_position );

		f32 dir = 1.0f;

		if (INPUT_PKEY(PENK_SHIFT))
		{
			dir = -1.0f;
		}

		static vec3f fly_pos = vec3f(0.0f, 10.0f, 10.0f);

		mat4 rx, ry, t, t2;
		rx.create_cardinal_rotation(X_AXIS, p_camera->rot.x);
		ry.create_cardinal_rotation(Y_AXIS, p_camera->rot.y);

		mat4 fly;
		fly.set_vectors(vec3f(1.0, 0.0, 0.0), vec3f(0.0, 1.0, 0.0), vec3f(0.0, 0.0, 1.0), fly_pos);

		f32 speed = 0.01f;

		if (INPUT_PKEY(PENK_W))
		{
			fly_pos -= fly.get_fwd() * speed;
		}

		if (INPUT_PKEY(PENK_A))
		{
			fly_pos -= fly.get_right() * speed;
		}

		if (INPUT_PKEY(PENK_S))
		{
			fly_pos += fly.get_fwd() * speed;
		}

		if (INPUT_PKEY(PENK_D))
		{
			fly_pos += fly.get_right() * speed;
		}

		//fps
		t.create_translation(fly_pos * -1.0f);
		p_camera->view = rx * ry * t;

		//model?
		t2.create_translation(vec3f(1.0f, 0.0f, 1.0f));

		t.create_translation( vec3f(0.0f, 0.0f, zoom) );

		p_camera->view = (ry * rx) * t;

		p_camera->view = p_camera->view.inverse3x4();

		dbg::add_line(vec3f(0.0f, 0.0f, 0.0f), p_camera->view.get_fwd(), vec3f::magenta());
		dbg::add_line(vec3f(0.0f, 0.0f, 0.0f), p_camera->view.get_right(), vec3f::cyan());
		dbg::add_line(vec3f(0.0f, 0.0f, 0.0f), p_camera->view.get_up(), vec3f::yellow());

		//dbg::add_line(vec3f(0.0f, 0.0f, 0.0f), fly.get_fwd(), vec3f::unit_x());
		//dbg::add_line(vec3f(0.0f, 0.0f, 0.0f), fly.get_right(), vec3f::unit_y());
		//dbg::add_line(vec3f(0.0f, 0.0f, 0.0f), fly.get_up(), vec3f::unit_z());

		p_camera->flags |= CF_INVALIDATED;
	}

	void camera_update_shader_constants( camera* p_camera )
	{
		if (p_camera->cbuffer == PEN_INVALID_HANDLE)
		{
			pen::buffer_creation_params bcp;
			bcp.usage_flags = PEN_USAGE_DYNAMIC;
			bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
			bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
			bcp.buffer_size = sizeof(camera_cbuffer);
			bcp.data = nullptr;

			p_camera->cbuffer = pen::defer::renderer_create_buffer(bcp);
		}

		if (p_camera->flags &= CF_INVALIDATED)
		{
			//camera_bake_matices(p_camera);

			camera_cbuffer wvp;

			wvp.view_projection = p_camera->proj * p_camera->view;
			wvp.view_matrix = p_camera->view;

			pen::defer::renderer_update_buffer(p_camera->cbuffer, &wvp, sizeof(camera_cbuffer));

			p_camera->flags &= ~CF_INVALIDATED;
		}
	}

	void camera_bake_matices( camera* p_camera )
	{
		mat4 rx, ry, t;
		rx.create_cardinal_rotation( X_AXIS, p_camera->rot.x );
		ry.create_cardinal_rotation( Y_AXIS, p_camera->rot.y );
		t.create_translation( p_camera->pos * -1.0f );

		//p_camera->view = (rx * ry) * t;
	}
}
