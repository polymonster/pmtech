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
    ecs_scene*          scene;
    
    void init(live_context* ctx)
    {
        scene = ctx->scene;
    }
    
    int on_load(live_context* ctx)
    {
        init(ctx);
        
        return 0;
    }
            
    int on_update(f32 dt)
    {
        add_line(vec3f::zero(), vec3f::unit_y() * 20.0f, vec4f::green());
        add_line(vec3f::unit_y() * 20.0f, vec3f::unit_y() * 20.0f + vec3f::unit_x() * 30.0f, vec4f::magenta());
        add_line(vec3f::unit_y() * 20.0f, vec3f::unit_z() * 50.0f, vec4f::blue());
        add_line(vec3f::unit_y() * 10.0f, vec3f::unit_x() * 50.0f, vec4f::red());

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

