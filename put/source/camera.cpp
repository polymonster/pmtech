#include "camera.h"
#include "put_math.h"
#include "input.h"
#include "renderer.h"
#include "debug_render.h"

using namespace pen;

namespace put
{
	void camera_create_perspective( camera* p_camera, f32 fov_degrees, f32 aspect_ratio, f32 near_plane, f32 far_plane )
	{
		vec2f near_size;
		vec2f far_size;

		near_size.y = 2.0f * tan( put::maths::deg_to_rad( fov_degrees ) / 2.0f ) * near_plane;
		near_size.x = near_size.y * aspect_ratio;

		far_size.y = 2.0f * tan( put::maths::deg_to_rad( fov_degrees ) / 2.0f ) * far_plane;
		far_size.x = far_size.y * aspect_ratio;
        
        p_camera->fov = fov_degrees;
        p_camera->aspect = aspect_ratio;
        p_camera->near_plane = near_plane;
        p_camera->far_plane = far_plane;

        p_camera->proj = mat4::create_perspective_projection
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

	void camera_create_orthographic(camera* p_camera, f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar)
	{
        p_camera->proj = mat4::create_orthographic_projection
		(
			left,
			right,
			bottom,
			top,
			znear,
			zfar
		);

		p_camera->flags |= CF_INVALIDATED;
	}

	void camera_update_fly(camera* p_camera)
	{
		mouse_state ms = input_get_mouse_state();

		//mouse drag
		static vec2f prev_mpos = vec2f((f32)ms.x, (f32)ms.y);
		vec2f current_mouse = vec2f((f32)ms.x, (f32)ms.y);
		vec2f mouse_drag = current_mouse - prev_mpos;
		prev_mpos = current_mouse;

		f32 mwheel = (f32)ms.wheel;
		static f32 prev_mwheel = mwheel;
		prev_mwheel = mwheel;
        
        
        f32 cursor_speed = 0.1f;
        if( INPUT_PKEY( PENK_UP ) )
            p_camera->rot.x -= cursor_speed;
        if( INPUT_PKEY( PENK_DOWN ) )
            p_camera->rot.x += cursor_speed;
        if( INPUT_PKEY( PENK_LEFT ) )
            p_camera->rot.y -= cursor_speed;
        if( INPUT_PKEY( PENK_RIGHT ) )
            p_camera->rot.y += cursor_speed;
        
		if (ms.buttons[PEN_MOUSE_L])
		{
			//rotation
			vec2f swapxy = vec2f(mouse_drag.y, mouse_drag.x);
			p_camera->rot += swapxy * ((2.0f * PI) / 360.0f);
		}

        mat4 rx = mat4::create_x_rotation(p_camera->rot.x);
		mat4 ry = mat4::create_y_rotation(p_camera->rot.y);
		mat4 t = mat4::create_translation(p_camera->pos * -1.0f);

		mat4 view_rotation = rx * ry;

		f32 speed = 0.01f;

		if (INPUT_PKEY(PENK_W))
		{
			p_camera->pos -= p_camera->view.get_fwd() * speed;
		}

		if (INPUT_PKEY(PENK_A))
		{
			p_camera->pos -= p_camera->view.get_right() * speed;
		}

		if (INPUT_PKEY(PENK_S))
		{
			p_camera->pos += p_camera->view.get_fwd() * speed;
		}

		if (INPUT_PKEY(PENK_D))
		{
			p_camera->pos += p_camera->view.get_right() * speed;
		}

		t.create_translation(p_camera->pos * -1.0f);
		p_camera->view = view_rotation * t;

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

		//rotate
		if (ms.buttons[PEN_MOUSE_L] && INPUT_PKEY(PENK_MENU))
		{
			//rotation
			vec2f swapxy = vec2f(-mouse_drag.y, mouse_drag.x);
			p_camera->rot += swapxy * ((2.0f * PI) / 360.0f);
		}
		else if (ms.buttons[PEN_MOUSE_M] && INPUT_PKEY(PENK_MENU) || (ms.buttons[PEN_MOUSE_L] && INPUT_PKEY(PENK_COMMAND)) )
		{
			//pan
            vec3f up = p_camera->view.get_up();
            vec3f right = p_camera->view.get_right();
            
            p_camera->focus += up * mouse_drag.y * 0.5f;
            p_camera->focus += right * mouse_drag.x * 0.5f;  
		}
		else if (ms.buttons[PEN_MOUSE_R] && INPUT_PKEY(PENK_MENU))
		{
			//zoom
			vec3f up = p_camera->view.get_up();
			vec3f right = p_camera->view.get_right();

			p_camera->zoom += -mouse_drag.y + mouse_drag.x;
		}

		//zoom
		f32 mwheel = (f32)ms.wheel;
		static f32 prev_mwheel = mwheel;
		f32 zoom = mwheel - prev_mwheel;
		prev_mwheel = mwheel;
		p_camera->zoom += zoom;
    
        p_camera->zoom = fmax(p_camera->zoom, 1.0f);
        
        mat4 rx = mat4::create_x_rotation(p_camera->rot.x);
        mat4 ry = mat4::create_y_rotation(-p_camera->rot.y);
        mat4 t = mat4::create_translation( vec3f(0.0f, 0.0f, p_camera->zoom) );
        mat4 t2 = mat4::create_translation(p_camera->focus);

		p_camera->view = t2 * (ry * rx) * t;

		p_camera->view = p_camera->view.inverse3x4();

		p_camera->flags |= CF_INVALIDATED;
	}

	void camera_update_shader_constants( camera* p_camera, bool viewport_correction )
	{
        f32 cur_aspect = (f32)pen_window.width / (f32)pen_window.height;
        if( cur_aspect != p_camera->aspect )
            camera_create_perspective(p_camera, p_camera->fov, cur_aspect, p_camera->near_plane, p_camera->far_plane );
        
        if( viewport_correction && !(p_camera->flags & CF_VP_CORRECTED) )
        {
            p_camera->flags |= CF_VP_CORRECTED;
            p_camera->flags |= CF_INVALIDATED;
        }
        
        if( !viewport_correction && (p_camera->flags & CF_VP_CORRECTED) )
        {
            p_camera->flags &= ~CF_VP_CORRECTED;
            p_camera->flags |= CF_INVALIDATED;
        }
        
		if (p_camera->cbuffer == PEN_INVALID_HANDLE)
		{
			pen::buffer_creation_params bcp;
			bcp.usage_flags = PEN_USAGE_DYNAMIC;
			bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
			bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
			bcp.buffer_size = sizeof(camera_cbuffer);
			bcp.data = nullptr;

			p_camera->cbuffer = pen::renderer_create_buffer(bcp);
		}

		if ( p_camera->flags & CF_INVALIDATED )
		{
			camera_cbuffer wvp;
            
            static mat4 scale = mat4::create_scale( vec3f( 1.0f, -1.0f, 1.0f ) );
            
            if( viewport_correction && pen::renderer_viewport_vup() )
                wvp.view_projection = scale * (p_camera->proj * p_camera->view);
            else
                wvp.view_projection = p_camera->proj * p_camera->view;
            
            mat4 inv_view = p_camera->view.inverse3x4();
            
			wvp.view_matrix = p_camera->view;
            wvp.view_position = vec4f( inv_view.get_translation(), 0.0 );
            wvp.view_direction = vec4f( inv_view.get_fwd(), 0.0 );
            
            //wvp.view_position = vec4f( 1.0, 0.0, 1.0, 0.0 );
           // wvp.view_direction = vec4f( 1.0, 1.0, 0.0, 0.0 );

			pen::renderer_update_buffer(p_camera->cbuffer, &wvp, sizeof(camera_cbuffer));

			p_camera->flags &= ~CF_INVALIDATED;
		}
	}
}
