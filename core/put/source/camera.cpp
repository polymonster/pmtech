// camera.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "camera.h"
#include "debug_render.h"
#include "dev_ui.h"

#include "input.h"
#include "os.h"
#include "renderer.h"

#include "maths/maths.h"

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

        p_camera->fov = fov_degrees;

        if (aspect_ratio == -1)
        {
            p_camera->flags |= e_camera_flags::window_aspect;
            p_camera->aspect = pen::window_get_aspect();
        }
        else
        {
            p_camera->aspect = aspect_ratio;
        }

        p_camera->near_plane = near_plane;
        p_camera->far_plane = far_plane;

        p_camera->proj = mat::create_perspective_projection(-near_size.x * 0.5f, near_size.x * 0.5f, -near_size.y * 0.5f,
                                                            near_size.y * 0.5f, near_plane, far_plane);

        p_camera->flags |= e_camera_flags::invalidated;
    }

    void camera_create_orthographic(camera* p_camera, f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar)
    {
        p_camera->proj = mat::create_orthographic_projection(left, right, bottom, top, znear, zfar);

        p_camera->flags |= e_camera_flags::invalidated;
        p_camera->flags |= e_camera_flags::orthographic;
    }

    void camera_update_fly(camera* p_camera, bool has_focus, camera_settings settings)
    {
        mouse_state ms = input_get_mouse_state();

        // mouse drag
        static vec2f prev_mpos = vec2f((f32)ms.x, (f32)ms.y);
        vec2f        current_mouse = vec2f((f32)ms.x, (f32)ms.y);
        vec2f        mouse_drag = current_mouse - prev_mpos;
        prev_mpos = current_mouse;

        f32        mwheel = (f32)ms.wheel;
        static f32 prev_mwheel = mwheel;
        prev_mwheel = mwheel;

        f32 cursor_speed = 0.1f;
        f32 speed = 1.0f;

        if (has_focus)
        {
            if (pen::input_key(PK_SHIFT))
            {
                speed = 0.01f;
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

            if (ms.buttons[PEN_MOUSE_L])
            {
                // rotation
                vec2f swapxy = vec2f(mouse_drag.y, mouse_drag.x);
                p_camera->rot += swapxy * 0.0075f;
            }

            if (pen::input_key(PK_W))
            {
                p_camera->pos -= p_camera->view.get_row(2).xyz * speed;
            }

            if (pen::input_key(PK_A))
            {
                p_camera->pos -= p_camera->view.get_row(0).xyz * speed;
            }

            if (pen::input_key(PK_S))
            {
                p_camera->pos += p_camera->view.get_row(2).xyz * speed;
            }

            if (pen::input_key(PK_D))
            {
                p_camera->pos += p_camera->view.get_row(0).xyz * speed;
            }
        }

        mat4 rx = mat::create_x_rotation(p_camera->rot.x);
        mat4 ry = mat::create_y_rotation(p_camera->rot.y);
        mat4 t = mat::create_translation(p_camera->pos * -1.0f);

        mat4 view_rotation = rx * ry;
        p_camera->view = view_rotation * t;

        p_camera->flags |= e_camera_flags::invalidated;
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
            p_camera->camera_frustum.corners[0][i] = maths::unproject_sc(vec3f(ndc_coords[i], 0.0f), view_proj, vpi);
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
            vec3f v1 = normalize(plane_vectors[offset + 1] - plane_vectors[offset + 0]);
            vec3f v2 = normalize(plane_vectors[offset + 2] - plane_vectors[offset + 0]);

            p_camera->camera_frustum.n[i] = cross(v1, v2);
            p_camera->camera_frustum.p[i] = plane_vectors[offset];
        }
    }

    void camera_update_modelling(camera* p_camera, bool has_focus, camera_settings settings)
    {
        mouse_state ms = input_get_mouse_state();

        // mouse drag
        static vec2f prev_mpos = vec2f((f32)ms.x, (f32)ms.y);
        vec2f        current_mouse = vec2f((f32)ms.x, (f32)ms.y);
        vec2f        mouse_drag = current_mouse - prev_mpos;
        prev_mpos = current_mouse;

        f32 mouse_y_inv = settings.invert_y ? -1.0f : 1.0f;
        f32 mouse_x_inv = settings.invert_x ? -1.0f : 1.0f;

        // zoom
        f32        mwheel = (f32)ms.wheel;
        static f32 prev_mwheel = mwheel;
        f32        zoom = (mwheel - prev_mwheel);
        prev_mwheel = mwheel;

        if (has_focus)
        {
            if (ms.buttons[PEN_MOUSE_L] && pen::input_key(PK_MENU))
            {
                // rotation
                vec2f swapxy = vec2f(mouse_drag.y * -mouse_y_inv, mouse_drag.x * mouse_x_inv);
                p_camera->rot += swapxy * ((2.0f * (f32)M_PI) / 360.0f);
            }
            else if ((ms.buttons[PEN_MOUSE_M] && pen::input_key(PK_MENU)) ||
                     ((ms.buttons[PEN_MOUSE_L] && pen::input_key(PK_COMMAND))))
            {
                // pan
                vec3f up = p_camera->view.get_row(1).xyz;
                vec3f right = p_camera->view.get_row(0).xyz;

                p_camera->focus += up * mouse_drag.y * mouse_y_inv * 0.5f;
                p_camera->focus += right * mouse_drag.x * mouse_x_inv * 0.5f;
            }
            else if (ms.buttons[PEN_MOUSE_R] && pen::input_key(PK_MENU))
            {
                // zoom
                p_camera->zoom += -mouse_drag.y + mouse_drag.x;
            }

            // zoom
            p_camera->zoom += zoom * settings.zoom_speed;

            p_camera->zoom = fmax(p_camera->zoom, 1.0f);
        }

        mat4 rx = mat::create_x_rotation(p_camera->rot.x);
        mat4 ry = mat::create_y_rotation(-p_camera->rot.y);
        mat4 t = mat::create_translation(vec3f(0.0f, 0.0f, p_camera->zoom));
        mat4 t2 = mat::create_translation(p_camera->focus);

        p_camera->view = t2 * (ry * rx) * t;

        p_camera->pos = p_camera->view.get_translation();

        p_camera->view = mat::inverse3x4(p_camera->view);

        p_camera->flags |= e_camera_flags::invalidated;
    }

    void camera_update_look_at(camera* p_camera)
    {
        mat4 rx = mat::create_x_rotation(p_camera->rot.x);
        mat4 ry = mat::create_y_rotation(-p_camera->rot.y);
        mat4 t = mat::create_translation(vec3f(0.0f, 0.0f, p_camera->zoom));
        mat4 t2 = mat::create_translation(p_camera->focus);

        p_camera->view = t2 * (ry * rx) * t;
        p_camera->pos = p_camera->view.get_translation();
        p_camera->view = mat::inverse3x4(p_camera->view);

        p_camera->flags |= e_camera_flags::invalidated;
    }

    void camera_update_look_at(camera* p_camera, vec3f pos, vec3f look_at)
    {
        vec3f at = look_at - pos;
        vec3f up = cross(at, vec3f(0.0f, 1.0f, 0.0f));
        vec3f right = cross(up, at);
        up = cross(at, right);
    }

    void camera_update_projection_matrix(camera* p_camera)
    {
        camera_create_perspective(p_camera, p_camera->fov, p_camera->aspect, p_camera->near_plane, p_camera->far_plane);
    }

    void camera_update_shader_constants(camera* p_camera)
    {
        // create cbuffer if needed
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

        // auto detect window aspect
        if (p_camera->flags & e_camera_flags::window_aspect)
        {
            f32 cur_aspect = pen::window_get_aspect();
            if (cur_aspect != p_camera->aspect)
            {
                p_camera->aspect = cur_aspect;
                camera_update_projection_matrix(p_camera);
            }
        }

        camera_update_frustum(p_camera);

        camera_cbuffer wvp;
        wvp.view_projection = p_camera->proj * p_camera->view;
        p_camera->view_projection = wvp.view_projection;

        // apply jitter to proj matrix
        if (p_camera->flags & e_camera_flags::apply_jitter)
        {
            mat4 jitter = mat::create_translation(vec3f(p_camera->jitter, 0.0f));
            wvp.view_projection = (jitter * p_camera->proj) * p_camera->view;
            p_camera->flags &= ~e_camera_flags::apply_jitter;
        }

        mat4 inv_view = mat::inverse3x4(p_camera->view);
        wvp.view_matrix = p_camera->view;
        wvp.view_position = vec4f(inv_view.get_translation(), p_camera->near_plane);
        wvp.view_direction = vec4f(inv_view.get_row(2).xyz, p_camera->far_plane);
        wvp.view_matrix_inverse = inv_view;
        wvp.view_projection_inverse = mat::inverse4x4(wvp.view_projection);

        pen::renderer_update_buffer(p_camera->cbuffer, &wvp, sizeof(camera_cbuffer));

        p_camera->flags &= ~e_camera_flags::invalidated;
    }

    void camera_create_cubemap(camera* p_camera, f32 near_plane, f32 far_plane)
    {
        camera_create_perspective(p_camera, 90, 1, near_plane, far_plane);
    }

    void camera_set_cubemap_face(camera* p_camera, u32 face)
    {
        // clang-format off
        static const vec3f at[] = {
            vec3f(-1.0, 0.0, 0.0), //+x
            vec3f(1.0, 0.0, 0.0),  //-x
            vec3f(0.0, -1.0, 0.0), //+y
            vec3f(0.0, 1.0, 0.0),  //-y
            vec3f(0.0, 0.0, 1.0),  //+z
            vec3f(0.0, 0.0, -1.0)  //-z
        };

        static const vec3f right[] = {
            vec3f(0.0, 0.0, 1.0),
            vec3f(0.0, 0.0, -1.0),
            vec3f(1.0, 0.0, 0.0),
            vec3f(1.0, 0.0, 0.0),
            vec3f(1.0, 0.0, 0.0),
            vec3f(-1.0, 0.0, -0.0)
        };

        static const vec3f up[] = {
            vec3f(0.0, 1.0, 0.0),
            vec3f(0.0, 1.0, 0.0),
            vec3f(0.0, 0.0, 1.0),
            vec3f(0.0, 0.0, -1.0),
            vec3f(0.0, 1.0, 0.0),
            vec3f(0.0, 1.0, 0.0)
        };
        // clang-format on

        p_camera->view.set_row(0, vec4f(right[face], 0.0f));
        p_camera->view.set_row(1, vec4f(up[face], 0.0f));
        p_camera->view.set_row(2, vec4f(at[face], 0.0f));
        p_camera->view.set_row(3, vec4f(0.0f, 0.0f, 0.0f, 1.0f));

        mat4 translate = mat::create_translation(-p_camera->pos);

        p_camera->view = p_camera->view * translate;
    }

    void get_aabb_corners(vec3f* corners, vec3f min, vec3f max)
    {
        // clang-format off
        static const vec3f offsets[8] = {
            vec3f::zero(),
            vec3f::one(),
            vec3f::unit_x(),
            vec3f::unit_y(),
            vec3f::unit_z(),
            vec3f(1.0f, 0.0f, 1.0f),
            vec3f(1.0f, 1.0f, 0.0f),
            vec3f(0.0f, 1.0f, 1.0f)
        };
        // clang-format on

        vec3f size = max - min;
        for (s32 i = 0; i < 8; ++i)
        {
            corners[i] = min + offsets[i] * size;
        }
    }

    void camera_update_shadow_frustum(put::camera* p_camera, vec3f light_dir, vec3f min, vec3f max)
    {
        // create view matrix
        vec3f right = cross(light_dir, vec3f::unit_y());
        vec3f up = cross(right, light_dir);

        mat4 shadow_view;
        shadow_view.set_vectors(right, up, -light_dir, vec3f::zero());

        // get corners
        vec3f corners[8];
        get_aabb_corners(&corners[0], min, max);

        // calculate extents in shadow space
        vec3f cmin = vec3f::flt_max();
        vec3f cmax = -vec3f::flt_max();
        for (s32 i = 0; i < 8; ++i)
        {
            vec3f p = shadow_view.transform_vector(corners[i]);
            p.z *= -1.0f;

            cmin = min_union(cmin, p);
            cmax = max_union(cmax, p);
        }

        // create ortho mat and set view matrix
        p_camera->view = shadow_view;
        p_camera->proj = mat::create_orthographic_projection(cmin.x, cmax.x, cmin.y, cmax.y, cmin.z, cmax.z);
        p_camera->flags |= e_camera_flags::invalidated | e_camera_flags::orthographic;

        camera_update_frustum(p_camera);

        return;

        // debug rendering.. to move into ecs
        for (u32 i = 0; i < 8; ++i)
            dbg::add_point(corners[i], 5.0f, vec4f::green());

        dbg::add_aabb(min, max, vec4f::white());
        dbg::add_frustum(p_camera->camera_frustum.corners[0], p_camera->camera_frustum.corners[1], vec4f::white());
    }
} // namespace put
