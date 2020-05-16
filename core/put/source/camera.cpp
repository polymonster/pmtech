// camera.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "camera.h"
#include "debug_render.h"
#include "dev_ui.h"

#include "input.h"
#include "os.h"
#include "renderer.h"
#include "renderer_shared.h"

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
            vec3f v1 = normalised(plane_vectors[offset + 1] - plane_vectors[offset + 0]);
            vec3f v2 = normalised(plane_vectors[offset + 2] - plane_vectors[offset + 0]);

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

    void camera_update_projection_matrix(camera* p_camera)
    {
        camera_create_perspective(p_camera, p_camera->fov, p_camera->aspect, p_camera->near_plane, p_camera->far_plane);
    }
    
    vec4f halton(u32 index)
    {
        static const vec4f HALTON[] = {
			vec4f(0.5000000000f, 0.3333333333f, 0.2000000000f, 0.1428571429f),
			vec4f(0.2500000000f, 0.6666666667f, 0.4000000000f, 0.2857142857f),
			vec4f(0.7500000000f, 0.1111111111f, 0.6000000000f, 0.4285714286f),
			vec4f(0.1250000000f, 0.4444444444f, 0.8000000000f, 0.5714285714f),
			vec4f(0.6250000000f, 0.7777777778f, 0.0400000000f, 0.7142857143f),
			vec4f(0.3750000000f, 0.2222222222f, 0.2400000000f, 0.8571428571f),
			vec4f(0.8750000000f, 0.5555555556f, 0.4400000000f, 0.0204081633f),
			vec4f(0.0625000000f, 0.8888888889f, 0.6400000000f, 0.1632653061f),
			vec4f(0.5625000000f, 0.0370370370f, 0.8400000000f, 0.3061224490f),
			vec4f(0.3125000000f, 0.3703703704f, 0.0800000000f, 0.4489795918f),
			vec4f(0.8125000000f, 0.7037037037f, 0.2800000000f, 0.5918367347f),
			vec4f(0.1875000000f, 0.1481481481f, 0.4800000000f, 0.7346938776f),
			vec4f(0.6875000000f, 0.4814814815f, 0.6800000000f, 0.8775510204f),
			vec4f(0.4375000000f, 0.8148148148f, 0.8800000000f, 0.0408163265f),
			vec4f(0.9375000000f, 0.2592592593f, 0.1200000000f, 0.1836734694f),
			vec4f(0.0312500000f, 0.5925925926f, 0.3200000000f, 0.3265306122f),
			vec4f(0.5312500000f, 0.9259259259f, 0.5200000000f, 0.4693877551f),
			vec4f(0.2812500000f, 0.0740740741f, 0.7200000000f, 0.6122448980f),
			vec4f(0.7812500000f, 0.4074074074f, 0.9200000000f, 0.7551020408f),
			vec4f(0.1562500000f, 0.7407407407f, 0.1600000000f, 0.8979591837f),
			vec4f(0.6562500000f, 0.1851851852f, 0.3600000000f, 0.0612244898f),
			vec4f(0.4062500000f, 0.5185185185f, 0.5600000000f, 0.2040816327f),
			vec4f(0.9062500000f, 0.8518518519f, 0.7600000000f, 0.3469387755f),
			vec4f(0.0937500000f, 0.2962962963f, 0.9600000000f, 0.4897959184f),
			vec4f(0.5937500000f, 0.6296296296f, 0.0080000000f, 0.6326530612f),
			vec4f(0.3437500000f, 0.9629629630f, 0.2080000000f, 0.7755102041f),
			vec4f(0.8437500000f, 0.0123456790f, 0.4080000000f, 0.9183673469f),
			vec4f(0.2187500000f, 0.3456790123f, 0.6080000000f, 0.0816326531f),
			vec4f(0.7187500000f, 0.6790123457f, 0.8080000000f, 0.2244897959f),
			vec4f(0.4687500000f, 0.1234567901f, 0.0480000000f, 0.3673469388f),
			vec4f(0.9687500000f, 0.4567901235f, 0.2480000000f, 0.5102040816f),
			vec4f(0.0156250000f, 0.7901234568f, 0.4480000000f, 0.6530612245f),
			vec4f(0.5156250000f, 0.2345679012f, 0.6480000000f, 0.7959183673f),
			vec4f(0.2656250000f, 0.5679012346f, 0.8480000000f, 0.9387755102f),
			vec4f(0.7656250000f, 0.9012345679f, 0.0880000000f, 0.1020408163f),
			vec4f(0.1406250000f, 0.0493827160f, 0.2880000000f, 0.2448979592f),
			vec4f(0.6406250000f, 0.3827160494f, 0.4880000000f, 0.3877551020f),
			vec4f(0.3906250000f, 0.7160493827f, 0.6880000000f, 0.5306122449f),
			vec4f(0.8906250000f, 0.1604938272f, 0.8880000000f, 0.6734693878f),
			vec4f(0.0781250000f, 0.4938271605f, 0.1280000000f, 0.8163265306f),
			vec4f(0.5781250000f, 0.8271604938f, 0.3280000000f, 0.9591836735f),
			vec4f(0.3281250000f, 0.2716049383f, 0.5280000000f, 0.1224489796f),
			vec4f(0.8281250000f, 0.6049382716f, 0.7280000000f, 0.2653061224f),
			vec4f(0.2031250000f, 0.9382716049f, 0.9280000000f, 0.4081632653f),
			vec4f(0.7031250000f, 0.0864197531f, 0.1680000000f, 0.5510204082f),
			vec4f(0.4531250000f, 0.4197530864f, 0.3680000000f, 0.6938775510f),
			vec4f(0.9531250000f, 0.7530864198f, 0.5680000000f, 0.8367346939f),
			vec4f(0.0468750000f, 0.1975308642f, 0.7680000000f, 0.9795918367f),
			vec4f(0.5468750000f, 0.5308641975f, 0.9680000000f, 0.0029154519f),
			vec4f(0.2968750000f, 0.8641975309f, 0.0160000000f, 0.1457725948f),
			vec4f(0.7968750000f, 0.3086419753f, 0.2160000000f, 0.2886297376f),
			vec4f(0.1718750000f, 0.6419753086f, 0.4160000000f, 0.4314868805f),
			vec4f(0.6718750000f, 0.9753086420f, 0.6160000000f, 0.5743440233f),
			vec4f(0.4218750000f, 0.0246913580f, 0.8160000000f, 0.7172011662f),
			vec4f(0.9218750000f, 0.3580246914f, 0.0560000000f, 0.8600583090f),
			vec4f(0.1093750000f, 0.6913580247f, 0.2560000000f, 0.0233236152f),
			vec4f(0.6093750000f, 0.1358024691f, 0.4560000000f, 0.1661807580f),
			vec4f(0.3593750000f, 0.4691358025f, 0.6560000000f, 0.3090379009f),
			vec4f(0.8593750000f, 0.8024691358f, 0.8560000000f, 0.4518950437f),
			vec4f(0.2343750000f, 0.2469135802f, 0.0960000000f, 0.5947521866f),
			vec4f(0.7343750000f, 0.5802469136f, 0.2960000000f, 0.7376093294f),
			vec4f(0.4843750000f, 0.9135802469f, 0.4960000000f, 0.8804664723f),
			vec4f(0.9843750000f, 0.0617283951f, 0.6960000000f, 0.0437317784f),
			vec4f(0.0078125000f, 0.3950617284f, 0.8960000000f, 0.1865889213f),
			vec4f(0.5078125000f, 0.7283950617f, 0.1360000000f, 0.3294460641f),
			vec4f(0.2578125000f, 0.1728395062f, 0.3360000000f, 0.4723032070f),
			vec4f(0.7578125000f, 0.5061728395f, 0.5360000000f, 0.6151603499f),
			vec4f(0.1328125000f, 0.8395061728f, 0.7360000000f, 0.7580174927f),
			vec4f(0.6328125000f, 0.2839506173f, 0.9360000000f, 0.9008746356f),
			vec4f(0.3828125000f, 0.6172839506f, 0.1760000000f, 0.0641399417f),
			vec4f(0.8828125000f, 0.9506172840f, 0.3760000000f, 0.2069970845f),
			vec4f(0.0703125000f, 0.0987654321f, 0.5760000000f, 0.3498542274f),
			vec4f(0.5703125000f, 0.4320987654f, 0.7760000000f, 0.4927113703f),
			vec4f(0.3203125000f, 0.7654320988f, 0.9760000000f, 0.6355685131f),
			vec4f(0.8203125000f, 0.2098765432f, 0.0240000000f, 0.7784256560f),
			vec4f(0.1953125000f, 0.5432098765f, 0.2240000000f, 0.9212827988f),
			vec4f(0.6953125000f, 0.8765432099f, 0.4240000000f, 0.0845481050f),
			vec4f(0.4453125000f, 0.3209876543f, 0.6240000000f, 0.2274052478f),
			vec4f(0.9453125000f, 0.6543209877f, 0.8240000000f, 0.3702623907f),
			vec4f(0.0390625000f, 0.9876543210f, 0.0640000000f, 0.5131195335f),
			vec4f(0.5390625000f, 0.0041152263f, 0.2640000000f, 0.6559766764f),
			vec4f(0.2890625000f, 0.3374485597f, 0.4640000000f, 0.7988338192f),
			vec4f(0.7890625000f, 0.6707818930f, 0.6640000000f, 0.9416909621f),
			vec4f(0.1640625000f, 0.1152263374f, 0.8640000000f, 0.1049562682f),
			vec4f(0.6640625000f, 0.4485596708f, 0.1040000000f, 0.2478134111f),
			vec4f(0.4140625000f, 0.7818930041f, 0.3040000000f, 0.3906705539f),
			vec4f(0.9140625000f, 0.2263374486f, 0.5040000000f, 0.5335276968f),
			vec4f(0.1015625000f, 0.5596707819f, 0.7040000000f, 0.6763848397f),
			vec4f(0.6015625000f, 0.8930041152f, 0.9040000000f, 0.8192419825f),
			vec4f(0.3515625000f, 0.0411522634f, 0.1440000000f, 0.9620991254f),
			vec4f(0.8515625000f, 0.3744855967f, 0.3440000000f, 0.1253644315f),
			vec4f(0.2265625000f, 0.7078189300f, 0.5440000000f, 0.2682215743f),
			vec4f(0.7265625000f, 0.1522633745f, 0.7440000000f, 0.4110787172f),
			vec4f(0.4765625000f, 0.4855967078f, 0.9440000000f, 0.5539358601f),
			vec4f(0.9765625000f, 0.8189300412f, 0.1840000000f, 0.6967930029f),
			vec4f(0.0234375000f, 0.2633744856f, 0.3840000000f, 0.8396501458f),
			vec4f(0.5234375000f, 0.5967078189f, 0.5840000000f, 0.9825072886f),
			vec4f(0.2734375000f, 0.9300411523f, 0.7840000000f, 0.0058309038f),
			vec4f(0.7734375000f, 0.0781893004f, 0.9840000000f, 0.1486880466f),
			vec4f(0.1484375000f, 0.4115226337f, 0.0320000000f, 0.2915451895f),
			vec4f(0.6484375000f, 0.7448559671f, 0.2320000000f, 0.4344023324f),
			vec4f(0.3984375000f, 0.1893004115f, 0.4320000000f, 0.5772594752f),
			vec4f(0.8984375000f, 0.5226337449f, 0.6320000000f, 0.7201166181f),
			vec4f(0.0859375000f, 0.8559670782f, 0.8320000000f, 0.8629737609f),
			vec4f(0.5859375000f, 0.3004115226f, 0.0720000000f, 0.0262390671f),
			vec4f(0.3359375000f, 0.6337448560f, 0.2720000000f, 0.1690962099f),
			vec4f(0.8359375000f, 0.9670781893f, 0.4720000000f, 0.3119533528f),
			vec4f(0.2109375000f, 0.0164609053f, 0.6720000000f, 0.4548104956f),
			vec4f(0.7109375000f, 0.3497942387f, 0.8720000000f, 0.5976676385f),
			vec4f(0.4609375000f, 0.6831275720f, 0.1120000000f, 0.7405247813f),
			vec4f(0.9609375000f, 0.1275720165f, 0.3120000000f, 0.8833819242f),
			vec4f(0.0546875000f, 0.4609053498f, 0.5120000000f, 0.0466472303f),
			vec4f(0.5546875000f, 0.7942386831f, 0.7120000000f, 0.1895043732f),
			vec4f(0.3046875000f, 0.2386831276f, 0.9120000000f, 0.3323615160f),
			vec4f(0.8046875000f, 0.5720164609f, 0.1520000000f, 0.4752186589f),
			vec4f(0.1796875000f, 0.9053497942f, 0.3520000000f, 0.6180758017f),
			vec4f(0.6796875000f, 0.0534979424f, 0.5520000000f, 0.7609329446f),
			vec4f(0.4296875000f, 0.3868312757f, 0.7520000000f, 0.9037900875f),
			vec4f(0.9296875000f, 0.7201646091f, 0.9520000000f, 0.0670553936f),
			vec4f(0.1171875000f, 0.1646090535f, 0.1920000000f, 0.2099125364f),
			vec4f(0.6171875000f, 0.4979423868f, 0.3920000000f, 0.3527696793f),
			vec4f(0.3671875000f, 0.8312757202f, 0.5920000000f, 0.4956268222f),
			vec4f(0.8671875000f, 0.2757201646f, 0.7920000000f, 0.6384839650f),
			vec4f(0.2421875000f, 0.6090534979f, 0.9920000000f, 0.7813411079f),
			vec4f(0.7421875000f, 0.9423868313f, 0.0016000000f, 0.9241982507f),
			vec4f(0.4921875000f, 0.0905349794f, 0.2016000000f, 0.0874635569f),
			vec4f(0.9921875000f, 0.4238683128f, 0.4016000000f, 0.2303206997f),
			vec4f(0.0039062500f, 0.7572016461f, 0.6016000000f, 0.3731778426f),
			vec4f(0.5039062500f, 0.2016460905f, 0.8016000000f, 0.5160349854f),
			vec4f(0.2539062500f, 0.5349794239f, 0.0416000000f, 0.6588921283f),
			vec4f(0.7539062500f, 0.8683127572f, 0.2416000000f, 0.8017492711f),
			vec4f(0.1289062500f, 0.3127572016f, 0.4416000000f, 0.9446064140f),
			vec4f(0.6289062500f, 0.6460905350f, 0.6416000000f, 0.1078717201f),
			vec4f(0.3789062500f, 0.9794238683f, 0.8416000000f, 0.2507288630f),
			vec4f(0.8789062500f, 0.0288065844f, 0.0816000000f, 0.3935860058f),
			vec4f(0.0664062500f, 0.3621399177f, 0.2816000000f, 0.5364431487f),
			vec4f(0.5664062500f, 0.6954732510f, 0.4816000000f, 0.6793002915f),
			vec4f(0.3164062500f, 0.1399176955f, 0.6816000000f, 0.8221574344f),
			vec4f(0.8164062500f, 0.4732510288f, 0.8816000000f, 0.9650145773f),
			vec4f(0.1914062500f, 0.8065843621f, 0.1216000000f, 0.1282798834f),
			vec4f(0.6914062500f, 0.2510288066f, 0.3216000000f, 0.2711370262f),
			vec4f(0.4414062500f, 0.5843621399f, 0.5216000000f, 0.4139941691f),
			vec4f(0.9414062500f, 0.9176954733f, 0.7216000000f, 0.5568513120f),
			vec4f(0.0351562500f, 0.0658436214f, 0.9216000000f, 0.6997084548f),
			vec4f(0.5351562500f, 0.3991769547f, 0.1616000000f, 0.8425655977f),
			vec4f(0.2851562500f, 0.7325102881f, 0.3616000000f, 0.9854227405f),
			vec4f(0.7851562500f, 0.1769547325f, 0.5616000000f, 0.0087463557f),
			vec4f(0.1601562500f, 0.5102880658f, 0.7616000000f, 0.1516034985f),
			vec4f(0.6601562500f, 0.8436213992f, 0.9616000000f, 0.2944606414f),
			vec4f(0.4101562500f, 0.2880658436f, 0.0096000000f, 0.4373177843f),
			vec4f(0.9101562500f, 0.6213991770f, 0.2096000000f, 0.5801749271f),
			vec4f(0.0976562500f, 0.9547325103f, 0.4096000000f, 0.7230320700f),
			vec4f(0.5976562500f, 0.1028806584f, 0.6096000000f, 0.8658892128f),
			vec4f(0.3476562500f, 0.4362139918f, 0.8096000000f, 0.0291545190f),
			vec4f(0.8476562500f, 0.7695473251f, 0.0496000000f, 0.1720116618f),
			vec4f(0.2226562500f, 0.2139917695f, 0.2496000000f, 0.3148688047f),
			vec4f(0.7226562500f, 0.5473251029f, 0.4496000000f, 0.4577259475f),
			vec4f(0.4726562500f, 0.8806584362f, 0.6496000000f, 0.6005830904f),
			vec4f(0.9726562500f, 0.3251028807f, 0.8496000000f, 0.7434402332f),
			vec4f(0.0195312500f, 0.6584362140f, 0.0896000000f, 0.8862973761f),
			vec4f(0.5195312500f, 0.9917695473f, 0.2896000000f, 0.0495626822f),
			vec4f(0.2695312500f, 0.0082304527f, 0.4896000000f, 0.1924198251f),
			vec4f(0.7695312500f, 0.3415637860f, 0.6896000000f, 0.3352769679f),
			vec4f(0.1445312500f, 0.6748971193f, 0.8896000000f, 0.4781341108f),
			vec4f(0.6445312500f, 0.1193415638f, 0.1296000000f, 0.6209912536f),
			vec4f(0.3945312500f, 0.4526748971f, 0.3296000000f, 0.7638483965f),
			vec4f(0.8945312500f, 0.7860082305f, 0.5296000000f, 0.9067055394f),
			vec4f(0.0820312500f, 0.2304526749f, 0.7296000000f, 0.0699708455f),
			vec4f(0.5820312500f, 0.5637860082f, 0.9296000000f, 0.2128279883f),
			vec4f(0.3320312500f, 0.8971193416f, 0.1696000000f, 0.3556851312f),
			vec4f(0.8320312500f, 0.0452674897f, 0.3696000000f, 0.4985422741f),
			vec4f(0.2070312500f, 0.3786008230f, 0.5696000000f, 0.6413994169f),
			vec4f(0.7070312500f, 0.7119341564f, 0.7696000000f, 0.7842565598f),
			vec4f(0.4570312500f, 0.1563786008f, 0.9696000000f, 0.9271137026f),
			vec4f(0.9570312500f, 0.4897119342f, 0.0176000000f, 0.0903790087f),
			vec4f(0.0507812500f, 0.8230452675f, 0.2176000000f, 0.2332361516f),
			vec4f(0.5507812500f, 0.2674897119f, 0.4176000000f, 0.3760932945f),
			vec4f(0.3007812500f, 0.6008230453f, 0.6176000000f, 0.5189504373f),
			vec4f(0.8007812500f, 0.9341563786f, 0.8176000000f, 0.6618075802f),
			vec4f(0.1757812500f, 0.0823045267f, 0.0576000000f, 0.8046647230f),
			vec4f(0.6757812500f, 0.4156378601f, 0.2576000000f, 0.9475218659f),
			vec4f(0.4257812500f, 0.7489711934f, 0.4576000000f, 0.1107871720f),
			vec4f(0.9257812500f, 0.1934156379f, 0.6576000000f, 0.2536443149f),
			vec4f(0.1132812500f, 0.5267489712f, 0.8576000000f, 0.3965014577f),
			vec4f(0.6132812500f, 0.8600823045f, 0.0976000000f, 0.5393586006f),
			vec4f(0.3632812500f, 0.3045267490f, 0.2976000000f, 0.6822157434f),
			vec4f(0.8632812500f, 0.6378600823f, 0.4976000000f, 0.8250728863f),
			vec4f(0.2382812500f, 0.9711934156f, 0.6976000000f, 0.9679300292f),
			vec4f(0.7382812500f, 0.0205761317f, 0.8976000000f, 0.1311953353f),
			vec4f(0.4882812500f, 0.3539094650f, 0.1376000000f, 0.2740524781f),
			vec4f(0.9882812500f, 0.6872427984f, 0.3376000000f, 0.4169096210f),
			vec4f(0.0117187500f, 0.1316872428f, 0.5376000000f, 0.5597667638f),
			vec4f(0.5117187500f, 0.4650205761f, 0.7376000000f, 0.7026239067f),
			vec4f(0.2617187500f, 0.7983539095f, 0.9376000000f, 0.8454810496f),
			vec4f(0.7617187500f, 0.2427983539f, 0.1776000000f, 0.9883381924f),
			vec4f(0.1367187500f, 0.5761316872f, 0.3776000000f, 0.0116618076f),
			vec4f(0.6367187500f, 0.9094650206f, 0.5776000000f, 0.1545189504f),
			vec4f(0.3867187500f, 0.0576131687f, 0.7776000000f, 0.2973760933f),
			vec4f(0.8867187500f, 0.3909465021f, 0.9776000000f, 0.4402332362f),
			vec4f(0.0742187500f, 0.7242798354f, 0.0256000000f, 0.5830903790f),
			vec4f(0.5742187500f, 0.1687242798f, 0.2256000000f, 0.7259475219f),
			vec4f(0.3242187500f, 0.5020576132f, 0.4256000000f, 0.8688046647f),
			vec4f(0.8242187500f, 0.8353909465f, 0.6256000000f, 0.0320699708f),
			vec4f(0.1992187500f, 0.2798353909f, 0.8256000000f, 0.1749271137f),
			vec4f(0.6992187500f, 0.6131687243f, 0.0656000000f, 0.3177842566f),
			vec4f(0.4492187500f, 0.9465020576f, 0.2656000000f, 0.4606413994f),
			vec4f(0.9492187500f, 0.0946502058f, 0.4656000000f, 0.6034985423f),
			vec4f(0.0429687500f, 0.4279835391f, 0.6656000000f, 0.7463556851f),
			vec4f(0.5429687500f, 0.7613168724f, 0.8656000000f, 0.8892128280f),
			vec4f(0.2929687500f, 0.2057613169f, 0.1056000000f, 0.0524781341f),
			vec4f(0.7929687500f, 0.5390946502f, 0.3056000000f, 0.1953352770f),
			vec4f(0.1679687500f, 0.8724279835f, 0.5056000000f, 0.3381924198f),
			vec4f(0.6679687500f, 0.3168724280f, 0.7056000000f, 0.4810495627f),
			vec4f(0.4179687500f, 0.6502057613f, 0.9056000000f, 0.6239067055f),
			vec4f(0.9179687500f, 0.9835390947f, 0.1456000000f, 0.7667638484f),
			vec4f(0.1054687500f, 0.0329218107f, 0.3456000000f, 0.9096209913f),
			vec4f(0.6054687500f, 0.3662551440f, 0.5456000000f, 0.0728862974f),
			vec4f(0.3554687500f, 0.6995884774f, 0.7456000000f, 0.2157434402f),
			vec4f(0.8554687500f, 0.1440329218f, 0.9456000000f, 0.3586005831f),
			vec4f(0.2304687500f, 0.4773662551f, 0.1856000000f, 0.5014577259f),
			vec4f(0.7304687500f, 0.8106995885f, 0.3856000000f, 0.6443148688f),
			vec4f(0.4804687500f, 0.2551440329f, 0.5856000000f, 0.7871720117f),
			vec4f(0.9804687500f, 0.5884773663f, 0.7856000000f, 0.9300291545f),
			vec4f(0.0273437500f, 0.9218106996f, 0.9856000000f, 0.0932944606f),
			vec4f(0.5273437500f, 0.0699588477f, 0.0336000000f, 0.2361516035f),
			vec4f(0.2773437500f, 0.4032921811f, 0.2336000000f, 0.3790087464f),
			vec4f(0.7773437500f, 0.7366255144f, 0.4336000000f, 0.5218658892f),
			vec4f(0.1523437500f, 0.1810699588f, 0.6336000000f, 0.6647230321f),
			vec4f(0.6523437500f, 0.5144032922f, 0.8336000000f, 0.8075801749f),
			vec4f(0.4023437500f, 0.8477366255f, 0.0736000000f, 0.9504373178f),
			vec4f(0.9023437500f, 0.2921810700f, 0.2736000000f, 0.1137026239f),
			vec4f(0.0898437500f, 0.6255144033f, 0.4736000000f, 0.2565597668f),
			vec4f(0.5898437500f, 0.9588477366f, 0.6736000000f, 0.3994169096f),
			vec4f(0.3398437500f, 0.1069958848f, 0.8736000000f, 0.5422740525f),
			vec4f(0.8398437500f, 0.4403292181f, 0.1136000000f, 0.6851311953f),
			vec4f(0.2148437500f, 0.7736625514f, 0.3136000000f, 0.8279883382f),
			vec4f(0.7148437500f, 0.2181069959f, 0.5136000000f, 0.9708454810f),
			vec4f(0.4648437500f, 0.5514403292f, 0.7136000000f, 0.1341107872f),
			vec4f(0.9648437500f, 0.8847736626f, 0.9136000000f, 0.2769679300f),
			vec4f(0.0585937500f, 0.3292181070f, 0.1536000000f, 0.4198250729f),
			vec4f(0.5585937500f, 0.6625514403f, 0.3536000000f, 0.5626822157f),
			vec4f(0.3085937500f, 0.9958847737f, 0.5536000000f, 0.7055393586f),
			vec4f(0.8085937500f, 0.0013717421f, 0.7536000000f, 0.8483965015f),
			vec4f(0.1835937500f, 0.3347050754f, 0.9536000000f, 0.9912536443f),
			vec4f(0.6835937500f, 0.6680384088f, 0.1936000000f, 0.0145772595f),
			vec4f(0.4335937500f, 0.1124828532f, 0.3936000000f, 0.1574344023f),
			vec4f(0.9335937500f, 0.4458161866f, 0.5936000000f, 0.3002915452f),
			vec4f(0.1210937500f, 0.7791495199f, 0.7936000000f, 0.4431486880f),
			vec4f(0.6210937500f, 0.2235939643f, 0.9936000000f, 0.5860058309f),
			vec4f(0.3710937500f, 0.5569272977f, 0.0032000000f, 0.7288629738f),
			vec4f(0.8710937500f, 0.8902606310f, 0.2032000000f, 0.8717201166f),
			vec4f(0.2460937500f, 0.0384087791f, 0.4032000000f, 0.0349854227f),
			vec4f(0.7460937500f, 0.3717421125f, 0.6032000000f, 0.1778425656f),
			vec4f(0.4960937500f, 0.7050754458f, 0.8032000000f, 0.3206997085f),
			vec4f(0.9960937500f, 0.1495198903f, 0.0432000000f, 0.4635568513f),
			vec4f(0.0019531250f, 0.4828532236f, 0.2432000000f, 0.6064139942f),
		};
		return HALTON[index % PEN_ARRAY_SIZE(HALTON)];
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
        
        // temporal jitter wip
        if(0)
        {
            s32 w, h;
            pen::window_get_size(w, h);
            vec2f size = vec2f(w, h);
            vec2f inv_size = vec2f::one() / size;
            vec4f hh = halton(pen::_renderer_frame_index());
            mat4 jitter = mat::create_translation(vec3f(hh.x, hh.y, 0.0f) * (vec3f)inv_size.xyy);
            wvp.view_projection = jitter * p_camera->proj * p_camera->view;
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
        static const vec3f at[] = {
            vec3f(-1.0, 0.0, 0.0), //+x
            vec3f(1.0, 0.0, 0.0),  //-x
            vec3f(0.0, -1.0, 0.0), //+y
            vec3f(0.0, 1.0, 0.0),  //-y
            vec3f(0.0, 0.0, 1.0),  //+z
            vec3f(0.0, 0.0, -1.0)  //-z
        };

        static const vec3f right[] = {vec3f(0.0, 0.0, 1.0), vec3f(0.0, 0.0, -1.0), vec3f(1.0, 0.0, 0.0),
                                      vec3f(1.0, 0.0, 0.0), vec3f(1.0, 0.0, 0.0),  vec3f(-1.0, 0.0, -0.0)};

        static const vec3f up[] = {vec3f(0.0, 1.0, 0.0),  vec3f(0.0, 1.0, 0.0), vec3f(0.0, 0.0, 1.0),
                                   vec3f(0.0, 0.0, -1.0), vec3f(0.0, 1.0, 0.0), vec3f(0.0, 1.0, 0.0)};

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
        for(u32 i = 0; i < 8; ++i)
            dbg::add_point(corners[i], 5.0f, vec4f::green());
        
        dbg::add_aabb(min, max, vec4f::white());
        dbg::add_frustum(p_camera->camera_frustum.corners[0], p_camera->camera_frustum.corners[1], vec4f::white());
    }
} // namespace put
