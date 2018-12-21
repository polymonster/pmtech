#ifndef _camera_h
#define _camera_h

#include "maths/mat.h"
#include "maths/vec.h"
#include "pen.h"
#include "renderer.h"
#include "str/Str.h"

namespace put
{
    static const f32 k_use_window_aspect = -1;

    enum camera_flags : u32
    {
        CF_INVALIDATED = 1 << 1,
        CF_VP_CORRECTED = 1 << 2,
        CF_ORTHO = 1 << 3,
        CF_WINDOW_ASPECT = 1 << 4
    };

    struct camera_cbuffer
    {
        mat4 view_projection;
        mat4 view_matrix;
        mat4 view_projection_inverse;
        mat4 view_matrix_inverse;
        vec4f view_position;
        vec4f view_direction;
        vec4f viewport_correction;
    };

    struct frustum
    {
        vec3f n[6];
        vec3f p[6];
        vec3f corners[2][4];
    };

    struct camera_settings
    {
        bool invert_x = false;
        bool invert_y = false;
        f32  zoom_speed = 1.0f;
    };

    struct camera
    {
        vec3f pos = vec3f::zero();
        vec3f focus = vec3f::zero();
        vec2f rot = vec2f(-0.5f, 0.5f);

        f32 fov = 0.0f;
        f32 aspect;
        f32 near_plane;
        f32 far_plane;
        f32 zoom = 60.0f;

        mat4 view;
        mat4 proj;

        u32 cbuffer = (u32)-1;
        u8  flags = 0;

        frustum camera_frustum;

        Str name;
    };

    

    void camera_create_perspective(camera* p_camera, f32 fov_degrees, f32 aspect_ratio, f32 near_plane, f32 far_plane);
    void camera_create_orthographic(camera* p_camera, f32 left, f32 right, f32 bottom, f32 top, f32 znear, f32 zfar);
    void camera_cteate_cubemap(camera* p_camera, f32 near_plane, f32 far_plane);
    void camera_set_cubemap_face(camera* p_camera, u32 face);
    void camera_update_look_at(camera* p_camera);
    void camera_update_projection_matrix(camera* p_camera);
    void camera_update_frustum(camera* p_camera);
    void camera_update_modelling(camera* p_camera, bool has_focus = true, camera_settings settings = {});
    void camera_update_fly(camera* p_camera, bool has_focus = true, camera_settings settings = {});
    void camera_update_shader_constants(camera* p_camera, bool viewport_correction = false);
    void camera_update_shadow_frustum(put::camera* p_camera, vec3f light_dir, vec3f min, vec3f max);
} // namespace put

#endif
