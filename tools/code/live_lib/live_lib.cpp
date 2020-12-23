#include "cr/cr.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_cull.h"

#include "imgui/imgui.h"

#define DLL 1
#include "ecs/ecs_live.h"
#include "str/Str.cpp"

#include "renderer.h"
#include "data_struct.h"
#include "timer.h"

#include "maths/maths.h"
#include "../../shader_structs/forward_render.h"

#include <stdio.h>

#define M_GOLDEN_RATIO 0.61803398875

struct live_lib
{
    u32                 box_start = 0;
    u32                 box_end;
    camera              cull_cam;
    
    void init(live_context* ctx)
    {
        //*(live_context*)this = *ctx;
        
        put::ecs::update(1.0/60.0f);
    }
    
    int on_load(live_context* ctx)
    {
        init(ctx);
        
        return 0;
    }
    
    void basis_from_axis(const vec3d axis, vec3d& right, vec3d& up, vec3d& at)
    {
        right = cross(axis, vec3d::unit_y());
        
        if (mag(right) < 0.1)
            right = cross(axis, vec3d::unit_z());
        
        if (mag(right) < 0.1)
            right = cross(axis, vec3d::unit_x());
            
        normalise(right);
        up = normalised(cross(axis, right));
        right = normalised(cross(axis, up));
        at = cross(right, up);
    }
    
    void pentagon_in_axis(const vec3d axis, const vec3d pos, f64 start_angle, bool recurse)
    {
        vec3d right, up, at;
        basis_from_axis(axis, right, up, at);
        
        f64 half_gr = 1.61803398875l/2.0;
            
        f64 internal_angle = 0.309017 * 1.5;
        f64 angle_step = M_PI / 2.5;
        f64 a = start_angle;
        for(u32 i = 0; i < 5; ++i)
        {
            f64 x = sin(a) * M_GOLDEN_RATIO;
            f64 y = cos(a) * M_GOLDEN_RATIO;
            
            vec3d p = pos + right * x + up * y;
            
            a += angle_step;
            f64 x2 = sin(a) * M_GOLDEN_RATIO;
            f64 y2 = cos(a) * M_GOLDEN_RATIO;
            
            vec3d np = pos + right * x2 + up * y2;
            add_line((vec3f)p, (vec3f)np, vec4f::green());
                        
            vec3d ev = normalised(np - p);
            vec3d cp = normalised(cross(ev, axis));

            vec3d mid = p + (np - p) * 0.5;
            
            f64 rx = sin((M_PI*2.0)+internal_angle) * M_GOLDEN_RATIO;
            f64 ry = cos((M_PI*2.0)+internal_angle) * M_GOLDEN_RATIO;
            vec3d xp = mid + cp * rx + axis * ry;
            
            vec3d xv = normalised(xp - mid);

            if(recurse)
            {
                vec3d next_axis = normalised(cross(xv, ev));
                pentagon_in_axis(next_axis, mid + xv * half_gr * M_GOLDEN_RATIO, M_PI + start_angle, false);
            }
        }
    }
    
    void penatgon_icosa(const vec3d axis, const vec3d pos, f64 start_angle)
    {
        vec3d right, up, at;
        basis_from_axis(axis, right, up, at);
        
        vec3d tip = pos - at * M_GOLDEN_RATIO;
        vec3d dip = pos + at * 0.5 * 2.0;
        
        f64 angle_step = M_PI / 2.5;
        f64 a = start_angle;
        for(u32 i = 0; i < 5; ++i)
        {
            f64 x = sin(a);
            f64 y = cos(a);
            
            vec3d p = pos + right * x + up * y;
            
            a += angle_step;
            f64 x2 = sin(a);
            f64 y2 = cos(a);
            
            vec3d np = pos + right * x2 + up * y2;
            add_line((vec3f)p, (vec3f)np, vec4f::green());
            add_line((vec3f)p, (vec3f)tip, vec4f::yellow());
            add_line((vec3f)np, (vec3f)tip, vec4f::cyan());
            
            vec3d side_dip = dip + cross(normalized(p-np), at);
            add_line((vec3f)np, (vec3f)side_dip, vec4f::magenta());
            add_line((vec3f)p, (vec3f)side_dip, vec4f::magenta());
        }
    }
    
    void icosahedron(vec3f axis, vec3f pos)
    {
        penatgon_icosa((vec3d)axis, (vec3d)(pos + axis * 0.5f), 0.0);
        penatgon_icosa((vec3d)-axis, (vec3d)(pos - axis * 0.5f), M_PI);
    }
    
    void dodecahedron(vec3f axis, vec3f pos)
    {
        f32 h = M_PI*0.83333333333f * 0.5f * M_GOLDEN_RATIO;
        pentagon_in_axis((vec3d)axis, (vec3d)pos + vec3d(0.0, -h, 0.0), 0.0f, true);
        pentagon_in_axis((vec3d)-axis, (vec3d)pos + vec3d(0.0, h, 0.0), M_PI, true);
    }
    
    void terahedron(vec3d axis, vec3d pos)
    {
        vec3d right, up, at;
        basis_from_axis(axis, right, up, at);
            
        vec3d tip = pos - at * sqrt(2.0); // sqrt 2 is pythagoras constant
        
        f64 angle_step = (M_PI*2.0) / 3.0;
        f64 a = 0.0f;
        for(u32 i = 0; i < 3; ++i)
        {
            f64 x = sin(a);
            f64 y = cos(a);
                        
            vec3d p = pos + right * x + up * y;
            
            a += angle_step;
            f64 x2 = sin(a);
            f64 y2 = cos(a);
            
            vec3d np = pos + right * x2 + up * y2;
            add_line((vec3f)p, (vec3f)np, vec4f::green());
            add_line((vec3f)p, (vec3f)tip, vec4f::yellow());
        }
    }
    
    void octahedron(vec3d axis, vec3d pos)
    {
        vec3f corner[] = {
            vec3f(-1.0, 0.0, -1.0),
            vec3f(-1.0, 0.0, 1.0),
            vec3f(1.0, 0.0, 1.0),
            vec3f(1.0, 0.0, -1.0)
        };
        
        f32 pc = sqrt(2.0);
        vec3f tip = vec3f(0.0f, pc, 0.0f);
        vec3f dip = vec3f(0.0f, -pc, 0.0f);
        
        vec3f fpos = (vec3f)pos;
        
        for(u32 i = 0; i < 4; ++i)
        {
            u32 n = (i + 1) % 4;
            
            add_line(fpos + corner[i], fpos + corner[n], vec4f::blue());
    
            add_line(fpos + corner[i], fpos + tip, vec4f::orange());
            add_line(fpos + corner[n], fpos + tip, vec4f::orange());
            
            add_line(fpos + corner[i], fpos + dip, vec4f::orange());
            add_line(fpos + corner[n], fpos + dip, vec4f::orange());
        }
    }
        
    int on_update(f32 dt)
    {
        icosahedron(vec3f::unit_y(), vec3f(-1.0f, 0.0f, 0.0f));
        dodecahedron(vec3f::unit_y(), vec3f(1.0f, 0.0f, 0.0f));
        terahedron(vec3d::unit_y(), vec3d(-5.0f, -M_GOLDEN_RATIO, 0.0f));
        octahedron(vec3d::unit_y(), vec3d(-3.0f, -0.0f, 0.0f));
                        
        return 0;
    }
    
    int on_unload()
    {
        return 0;
    }
};

CR_EXPORT int cr_main(struct cr_plugin *ctx, enum cr_op operation)
{
    live_context* live_ctx = (live_context*)ctx->userdata;
    static live_lib ll;
    
    switch (operation)
    {
        case CR_LOAD:
            return ll.on_load(live_ctx);
        case CR_UNLOAD:
            return ll.on_unload();
        case CR_CLOSE:
            return 0;
        default:
            break;
    }
    
    return ll.on_update(live_ctx->dt);
}

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        return {};
    }
    
    void* user_entry(void* params)
    {
        return nullptr;
    }
}


