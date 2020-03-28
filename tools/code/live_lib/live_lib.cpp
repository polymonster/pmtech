#include "cr/cr.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "ecs/ecs_resources.h"

#define DLL 1
#include "ecs/ecs_live.h"
#include "str/Str.cpp"

#include "renderer.h"
#include "maths/maths.h"

#include <stdio.h>

using namespace pen;
using namespace put;
using namespace ecs;
using namespace dbg;

struct live_lib : public __ecs, public __dbg
{
    ecs_scene* scene;
    
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
        scene->lights[light].colour = vec3f(0.5f);
        scene->lights[light].direction = vec3f::one();
        scene->lights[light].type = e_light_type::dir;
        scene->lights[light].flags |= e_light_flags::shadow_map;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
                    
        // ground
        f32 ground_size = 50.0f;
        u32 ground = get_new_entity(scene);
        scene->names[ground] = "ground";
        scene->id_name[ground] = PEN_HASH("ground");
        scene->transforms[ground].rotation = quat();
        scene->transforms[ground].scale = vec3f(ground_size, 1.0f, ground_size);
        scene->transforms[ground].translation = vec3f(0.0f, -1.0f, 0.0f);
        scene->parents[ground] = ground;
        scene->entities[ground] |= e_cmp::transform;
        instantiate_geometry(box_resource, scene, ground);
        instantiate_material(default_material, scene, ground);
        instantiate_model_cbuffer(scene, ground);
        
        // box
        u32 box = get_new_entity(scene);
        scene->names[box] = "box";
        scene->id_name[box] = PEN_HASH("box");
        scene->transforms[box].rotation = quat();
        scene->transforms[box].scale = vec3f(10.0f);
        scene->transforms[box].translation = vec3f(0.0f, 5.0f, 0.0f);
        scene->parents[box] = box;
        scene->entities[box] |= e_cmp::transform;
        instantiate_geometry(box_resource, scene, box);
        instantiate_material(default_material, scene, box);
        instantiate_model_cbuffer(scene, box);
                
        return 0;
    }
    
    int on_update()
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
    
    return ll.on_update();
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

