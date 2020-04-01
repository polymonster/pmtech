#include "cr/cr.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "ecs/ecs_resources.h"

#define DLL 1
#include "ecs/ecs_live.h"
#include "str/Str.cpp"

#include "renderer.h"
#include "maths/maths.h"
#include "../../shader_structs/forward_render.h"

#include <stdio.h>

using namespace pen;
using namespace put;
using namespace ecs;
using namespace dbg;

s32 randri(s32 min, s32 max)
{
    s32 range = max - min;
    if(range == 0)
        return min;
    return min + rand()%range;
}

struct live_lib : public __ecs, public __dbg
{
    const f32           cube_pad = 0.5f;
    const f32           cube_size = 10.0f;
    const f32           grid_cube_size = cube_size*2;
    const vec3f         base_offset = vec3f(-(f32)grid_size*cube_size, 0.0f, -(f32)grid_size*cube_size);
    static const u32    grid_size = 10;
    static const u32    num_boxes = grid_size*grid_size;
    
    ecs_scene*          scene;
    u32                 occ[grid_size][grid_size];
    u32                 box_start;
    
    u32 box_count = 0;
    
    void init(live_context* ctx)
    {
        scene = ctx->scene;
        memcpy(&__ecs_start, &ctx->ecs_funcs->__ecs_start, (intptr_t)&ctx->ecs_funcs->__ecs_end - (intptr_t)&ctx->ecs_funcs->__ecs_start);
        memcpy(&__dbg_start, &ctx->dbg_funcs->__dbg_start, (intptr_t)&ctx->dbg_funcs->__dbg_end - (intptr_t)&ctx->dbg_funcs->__dbg_start);
    }
    
    int on_load(live_context* ctx)
    {
        init(ctx);
        
        material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
        geometry_resource* box_resource = get_geometry_resource(PEN_HASH("cube"));
            
        clear_scene(scene);

        // directional light
        u32 light = get_new_entity(scene);
        instantiate_light(scene, light);
        scene->names[light] = "front_light";
        scene->id_name[light] = PEN_HASH("front_light");
        scene->lights[light].colour = vec3f(0.3f, 0.5f, 0.8f);
        scene->lights[light].direction = normalised(vec3f(-1.0f, 0.5f, -0.5f));
        scene->lights[light].type = e_light_type::dir;
        scene->lights[light].flags |= e_light_flags::shadow_map;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        load_pmm("/Users/alex.dixon/dev/dr_scientist/bin/osx/data/models/characters/doctor/doctor.pmm", scene, e_pmm_load_flags::all);
   
        return 0;
    }
    
    int on_update(f32 dt)
    {
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


