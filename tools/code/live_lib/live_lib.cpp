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
    
    void pentagon_in_axis(const vec3d axis, const vec3d pos, f64 start_angle, bool recurse)
    {
        vec3d right = cross(axis, vec3d::unit_y());
        if (mag(right) < 0.1)
            right = cross(axis, vec3d::unit_z());
        if (mag(right) < 0.1)
            right = cross(axis, vec3d::unit_x());
            
        normalise(right);
        vec3d up = normalised(cross(axis, right));
        right = normalised(cross(axis, up));
        
        //add_circle((vec3f)axis, (vec3f)pos, 1.0f, vec4f::yellow());
        
        f64 half_gr = 1.61803398875l/2.0;
            
        f64 internal_angle = 0.309017 * 1.5;
        f64 angle_step = M_PI / 2.5;
        f64 a = start_angle;
        for(u32 i = 0; i < 5; ++i)
        {
            f64 x = sin(a);
            f64 y = cos(a);
            
            vec3d p = pos + right * x + up * y;
            //add_point((vec3f)p, 0.01f, vec4f::magenta());
            
            a += angle_step;
            f64 x2 = sin(a);
            f64 y2 = cos(a);
            
            vec3d np = pos + right * x2 + up * y2;
            add_line((vec3f)p, (vec3f)np, vec4f::green());
                        
            vec3d ev = normalised(np - p);
            vec3d cp = normalised(cross(ev, axis));

            vec3d mid = p + (np - p) * 0.5;
            //add_line((vec3f)mid, (vec3f)(mid + cp), vec4f::cyan());
            
            f64 rx = sin((M_PI*2.0)+internal_angle);
            f64 ry = cos((M_PI*2.0)+internal_angle);
            vec3d xp = mid + cp * rx + axis * ry;
            
            vec3d xv = normalised(xp - mid);

            if(recurse)
            {
                //add_point((vec3f)xp, 0.1f, vec4f::yellow());
                vec3d next_axis = normalised(cross(xv, ev));
                pentagon_in_axis(next_axis, mid + xv * half_gr, M_PI + start_angle, false);
            }
        }
    }
        
    int on_update(f32 dt)
    {
        pentagon_in_axis(vec3d::unit_y(), vec3d::zero(), 0.0f, true);
        pentagon_in_axis(-vec3d::unit_y(), vec3d(0.0, M_PI*0.83333333333f, 0.0), M_PI, true);
        
        ImGui::Text("Hello Gurrls");
                        
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


