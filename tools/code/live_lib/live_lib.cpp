#include "cr/cr.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "ecs/ecs_resources.h"

#define DLL 1
#include "ecs/ecs_live.h"
#include "str/Str.cpp"

#include "renderer.h"
#include "data_struct.h"

#include "maths/maths.h"
#include "../../shader_structs/forward_render.h"

#include <stdio.h>

using namespace pen;
using namespace put;
using namespace ecs;
using namespace dbg;
using namespace cam;
using namespace pmfx;

namespace
{
    s32 randri(s32 min, s32 max)
    {
        s32 range = max - min;
        if(range == 0)
            return min;
        return min + rand()%range;
    }
    
    struct lane
    {
        u32*     entities;
        u32*     entity_target;
        vec3f*   targets;
    };
}

struct live_lib : public __ecs, public __dbg, public __cam, public __pmfx
{
    ecs_scene*          scene;
    u32                 box_start = 0;
    u32                 box_end;
    u32                 quadrant_size = 0;
    u32                 box_count = 0;
    static const s32    lanes = 5;
    lane                lane_info[lanes] = { 0 };
    
    camera              cull_cam;
    
    void init(live_context* ctx)
    {
        scene = ctx->scene;
        memcpy(&__ecs_start, &ctx->ecs_funcs->__ecs_start, (intptr_t)&ctx->ecs_funcs->__ecs_end - (intptr_t)&ctx->ecs_funcs->__ecs_start);
        memcpy(&__dbg_start, &ctx->dbg_funcs->__dbg_start, (intptr_t)&ctx->dbg_funcs->__dbg_end - (intptr_t)&ctx->dbg_funcs->__dbg_start);
        memcpy(&__cam_start, &ctx->cam_funcs->__cam_start, (intptr_t)&ctx->cam_funcs->__cam_end - (intptr_t)&ctx->cam_funcs->__cam_start);
        memcpy(&__pmfx_start, &ctx->pmfx_funcs->__pmfx_start, (intptr_t)&ctx->pmfx_funcs->__pmfx_end - (intptr_t)&ctx->pmfx_funcs->__pmfx_start);
    }
    
    int on_load(live_context* ctx)
    {
        init(ctx);
        
        camera_create_perspective(&cull_cam, 60.0f, 16.0f/9.0f, 0.01f, 1000.0f);
        
        return 0;
    }
    
    int on_update(f32 dt)
    {
        camera_update_look_at(&cull_cam);
        camera_update_frustum(&cull_cam);
        
        add_frustum(&cull_cam.camera_frustum.corners[0][0], &cull_cam.camera_frustum.corners[1][0], vec4f::green());
        
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


