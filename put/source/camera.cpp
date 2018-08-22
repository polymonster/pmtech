#include "camera.h"
#include "debug_render.h"
#include "input.h"
#include "maths/maths.h"
#include "renderer.h"

using namespace pen;

namespace put
{
    void camera_create_perspective(camera* p_camera, f32 fov_degrees, f32 aspect_ratio, f32 near_plane, f32 far_plane)
    {
        vec2f near_size;
        vec2f far_size;

        near_size.y = 2.0f * tan(maths::deg_to_rad(fov_degrees) / 2.0f) * near_plane;
        near_size.x = near_size.y * aspect_ratio;

        far_size.y = 2.0f * tan(maths::deg_to_rad(fov_degrees) / 2.0f) * far_plane;
        far_size.x = far_size.y * aspect_ratio;

        p_camera->fov        = fov_degrees;
        p_camera->aspect     = aspect_ratio;
        p_camera->near_plane = near_plane;
        p_camera->far_plane  = far_plane;

        p_camera->proj = mat::create_perspective_projection(-near_size.x * 0.5f, near_size.x * 0.5f, -near_size.y * 0.5f,
                                                            near_size.y * 0.5f, near_plane, far_plane);

        p_camera->flags |= CF_INVALIDATED;
    }

    void camera_create_orthographic(camera* p_camera, f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar)
    {
        p_camera->proj = mat::create_orthographic_projection(left, right, bottom, top, znear, zfar);

        p_camera->flags |= CF_INVALIDATED;
        p_camera->flags |= CF_ORTHO;
    }

    void camera_update_fly(camera* p_camera, bool has_focus, bool invert_y)
    {
        mouse_state ms = input_get_mouse_state();

        // mouse drag
        static vec2f prev_mpos     = vec2f((f32)ms.x, (f32)ms.y);
        vec2f        current_mouse = vec2f((f32)ms.x, (f32)ms.y);
        vec2f        mouse_drag    = current_mouse - prev_mpos;
        prev_mpos                  = current_mouse;

        f32        mwheel      = (f32)ms.wheel;
        static f32 prev_mwheel = mwheel;
        prev_mwheel            = mwheel;

        f32 cursor_speed = 0.1f;
        f32 speed        = 1.0f;

        if (has_focus)
        {
            if (pen::input_key(PK_SHIFT))
            {
                speed        = 0.01f;
                cursor_speed = 0.1f;
            }

            if (pen::input_key(PK_UP))
                p_camera->rot.x -= cursor_speed;
            if (pen::input_key(PK_DOWN))
                p_camera->rot.x += cursor_speed;
            if (pen::input_key(PK_LEFT))
                p_camera->rot.y -= cursor_speed;
            if (pen::input_key(PK_RIGHT))
                p_camera->rot.y += cursor_speed;

            if (ms.buttons[PEN_MOUSE_R])
            {
                // rotation
                vec2f swapxy = vec2f(mouse_drag.y, mouse_drag.x);
                p_camera->rot += swapxy * 0.0075f;
            }

            if (pen::input_key(PK_W))
            {
                p_camera->pos -= p_camera->view.get_fwd() * speed;
            }

            if (pen::input_key(PK_A))
            {
                p_camera->pos -= p_camera->view.get_right() * speed;
            }

            if (pen::input_key(PK_S))
            {
                p_camera->pos += p_camera->view.get_fwd() * speed;
            }

            if (pen::input_key(PK_D))
            {
                p_camera->pos += p_camera->view.get_right() * speed;
            }
        }

        mat4 rx = mat::create_x_rotation(p_camera->rot.x);
        mat4 ry = mat::create_y_rotation(p_camera->rot.y);
        mat4 t  = mat::create_translation(p_camera->pos * -1.0f);

        mat4 view_rotation = rx * ry;
        p_camera->view     = view_rotation * t;

        p_camera->flags |= CF_INVALIDATED;
    }

    void camera_update_frustum(camera* p_camera)
    {
        static vec2f ndc_coords[] = {
            vec2f(0.0f, 1.0f),
            vec2f(1.0f, 1.0f),
            vec2f(0.0f, 0.0f),
            vec2f(1.0f, 0.0f),
        };

        vec2i vpi = vec2i(1, 1);

        mat4 view_proj = p_camera->proj * p_camera->view;

        for (s32 i = 0; i < 4; ++i)
        {
            p_camera->camera_frustum.corners[0][i] = maths::unproject_sc(vec3f(ndc_coords[i], -1.0f), view_proj, vpi);
            p_camera->camera_frustum.corners[1][i] = maths::unproject_sc(vec3f(ndc_coords[i], 1.0f), view_proj, vpi);
        }

        const frustum& f = p_camera->camera_frustum;

        vec3f plane_vectors[] = {
            f.corners[0][0], f.corners[1][0], f.corners[0][2], // left
            f.corners[0][0], f.corners[0][1], f.corners[1][0], // top

            f.corners[0][1], f.corners[0][3], f.corners[1][1], // right
            f.corners[0][2], f.corners[1][2], f.corners[0][3], // bottom

            f.corners[0][0], f.corners[0][2], f.corners[0][1], // near
            f.corners[1][0], f.corners[1][1], f.corners[1][2]  // far
        };

        for (s32 i = 0; i < 6; ++i)
        {
            s32   offset = i * 3;
            vec3f v1     = normalised(plane_vectors[offset + 1] - plane_vectors[offset + 0]);
            vec3f v2     = normalised(plane_vectors[offset + 2] - plane_vectors[offset + 0]);

            p_camera->camera_frustum.n[i] = cross(v1, v2);
            p_camera->camera_frustum.p[i] = plane_vectors[offset];
        }
    }

    void camera_update_modelling(camera* p_camera, bool has_focus, bool invert_y)
    {
        mouse_state ms = input_get_mouse_state();

        // mouse drag
        static vec2f prev_mpos     = vec2f((f32)ms.x, (f32)ms.y);
        vec2f        current_mouse = vec2f((f32)ms.x, (f32)ms.y);
        vec2f        mouse_drag    = current_mouse - prev_mpos;
        prev_mpos                  = current_mouse;

        f32 mouse_y_inv = invert_y ? -1.0f : 1.0f;

        // zoom
        f32        mwheel      = (f32)ms.wheel;
        static f32 prev_mwheel = mwheel;
        f32        zoom        = mwheel - prev_mwheel;
        prev_mwheel            = mwheel;

        if (has_focus)
        {
            if (ms.buttons[PEN_MOUSE_L] && pen::input_key(PK_MENU))
            {
                // rotation
                vec2f swapxy = vec2f(mouse_drag.y * -mouse_y_inv, mouse_drag.x);
                p_camera->rot += swapxy * ((2.0f * (f32)M_PI) / 360.0f);
            }
            else if ((ms.buttons[PEN_MOUSE_M] && pen::input_key(PK_MENU)) ||
                     ((ms.buttons[PEN_MOUSE_L] && pen::input_key(PK_COMMAND))))
            {
                // pan
                vec3f up    = p_camera->view.get_up();
                vec3f right = p_camera->view.get_right();

                p_camera->focus += up * mouse_drag.y * mouse_y_inv * 0.5f;
                p_camera->focus += right * mouse_drag.x * 0.5f;
            }
            else if (ms.buttons[PEN_MOUSE_R] && pen::input_key(PK_MENU))
            {
                // zoom
                p_camera->zoom += -mouse_drag.y + mouse_drag.x;
            }

            // zoom
            p_camera->zoom += zoom;

            p_camera->zoom = fmax(p_camera->zoom, 1.0f);
        }

        mat4 rx = mat::create_x_rotation(p_camera->rot.x);
        mat4 ry = mat::create_y_rotation(-p_camera->rot.y);
        mat4 t  = mat::create_translation(vec3f(0.0f, 0.0f, p_camera->zoom));
        mat4 t2 = mat::create_translation(p_camera->focus);

        p_camera->view = t2 * (ry * rx) * t;

        p_camera->view = mat::inverse3x4(p_camera->view);

        p_camera->flags |= CF_INVALIDATED;
    }

    void camera_update_projection_matrix(camera* p_camera)
    {
        camera_create_perspective(p_camera, p_camera->fov, p_camera->aspect, p_camera->near_plane, p_camera->far_plane);
    }

    void camera_update_shader_constants(camera* p_camera, bool viewport_correction)
    {
        if (!(p_camera->flags & CF_ORTHO))
        {
            f32 cur_aspect = (f32)pen_window.width / (f32)pen_window.height;
            if (cur_aspect != p_camera->aspect)
            {
                p_camera->aspect = cur_aspect;
                camera_update_projection_matrix(p_camera);
            }
        }

        camera_update_frustum(p_camera);

        if (viewport_correction && !(p_camera->flags & CF_VP_CORRECTED))
        {
            p_camera->flags |= CF_VP_CORRECTED;
            p_camera->flags |= CF_INVALIDATED;
        }

        if (!viewport_correction && (p_camera->flags & CF_VP_CORRECTED))
        {
            p_camera->flags &= ~CF_VP_CORRECTED;
            p_camera->flags |= CF_INVALIDATED;
        }

        if (p_camera->cbuffer == PEN_INVALID_HANDLE)
        {
            pen::buffer_creation_params bcp;
            bcp.usage_flags      = PEN_USAGE_DYNAMIC;
            bcp.bind_flags       = PEN_BIND_CONSTANT_BUFFER;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
            bcp.buffer_size      = sizeof(camera_cbuffer);
            bcp.data             = nullptr;

            p_camera->cbuffer = pen::renderer_create_buffer(bcp);
        }

        // if (p_camera->flags & CF_INVALIDATED)
        {
            camera_cbuffer wvp;

            static mat4 scale = mat::create_scale(vec3f(1.0f, -1.0f, 1.0f));

            if (viewport_correction && pen::renderer_viewport_vup())
            {
                wvp.view_projection     = scale * (p_camera->proj * p_camera->view);
                wvp.viewport_correction = vec4f(-1.0f, 1.0f, 0.0f, 0.0f);
            }
            else
            {
                wvp.view_projection     = p_camera->proj * p_camera->view;
                wvp.viewport_correction = vec4f(1.0f, 0.0f, 0.0f, 0.0f);
            }

            mat4 inv_view           = mat::inverse3x4(p_camera->view);
            wvp.view_matrix         = p_camera->view;
            wvp.view_position       = vec4f(inv_view.get_translation(), p_camera->near_plane);
            wvp.view_direction      = vec4f(inv_view.get_fwd(), p_camera->far_plane);
            wvp.view_matrix_inverse = inv_view;

            pen::renderer_update_buffer(p_camera->cbuffer, &wvp, sizeof(camera_cbuffer));

            p_camera->flags &= ~CF_INVALIDATED;
        }
    }
} // namespace put
