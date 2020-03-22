#include "cr/cr.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "ecs/ecs_resources.h"

#define DLL 1
#include "ecs/ecs_live.h"
#include "str/Str.cpp"

#include "renderer.h"

#include <stdio.h>

using namespace pen;
using namespace put;
using namespace ecs;

int on_load(ecs::live_context* live_ctx)
{
    material_resource* default_material = live_ctx->get_material_resource(PEN_HASH("default_material"));
    geometry_resource* box_resource = live_ctx->get_geometry_resource(PEN_HASH("cube"));
        
    live_ctx->clear_scene(live_ctx->scene);
        
    f32 ground_size = 100.0f;
    u32 ground = live_ctx->get_new_entity(live_ctx->scene);
    live_ctx->scene->transforms[ground].rotation = quat();
    live_ctx->scene->transforms[ground].scale = vec3f(ground_size, 1.0f, ground_size);
    live_ctx->scene->transforms[ground].translation = vec3f::zero();
    live_ctx->scene->parents[ground] = ground;
    live_ctx->scene->entities[ground] |= e_cmp::transform;
    live_ctx->instantiate_geometry(box_resource, live_ctx->scene, ground);
    live_ctx->instantiate_material(default_material, live_ctx->scene, ground);
    live_ctx->instantiate_model_cbuffer(live_ctx->scene, ground);

    // directional
    u32 light = live_ctx->get_new_entity(live_ctx->scene);
    live_ctx->instantiate_light(live_ctx->scene, light);
    live_ctx->scene->names[light] = "front_light";
    live_ctx->scene->id_name[light] = PEN_HASH("front_light");
    live_ctx->scene->lights[light].colour = vec3f::one() * 0.3f;
    live_ctx->scene->lights[light].direction = vec3f::one();
    live_ctx->scene->lights[light].type = e_light_type::dir;
    live_ctx->scene->lights[light].flags |= e_light_flags::shadow_map;
    live_ctx->scene->transforms[light].translation = vec3f::zero();
    live_ctx->scene->transforms[light].rotation = quat();
    live_ctx->scene->transforms[light].scale = vec3f::one();
    live_ctx->scene->entities[light] |= e_cmp::light;
    live_ctx->scene->entities[light] |= e_cmp::transform;
    
    return 0;
}

int on_unload()
{
    return 0;
}

int ion_update(ecs::live_context* live_ctx)
{
    live_ctx->scene->lights[0].colour = vec3f(0.0f, 0.0f, 1.0f);
    live_ctx->scene->lights[0].flags |= e_light_flags::shadow_map;
    
    live_ctx->scene->transforms[1].translation = vec3f(0.0f, 0.0f, 0.0f);
    live_ctx->scene->entities[1] |= e_cmp::transform;
    
    live_ctx->scene->transforms[2].translation = vec3f(20.0f, -20.0f, 50.0f);
    live_ctx->scene->entities[2] |= e_cmp::transform;
    
    static f32 dt = 0.0f;
    dt += 1.0 / 60.0f;
    live_ctx->scene->transforms[10].translation += vec3f(0.0f, sin(dt), 0.0f);
    live_ctx->scene->entities[10] |= e_cmp::transform;
    
    live_ctx->scene->transforms[7].translation += vec3f(0.0f, sin(dt + 60.0f), 0.0f);
    live_ctx->scene->entities[7] |= e_cmp::transform;
    
    live_ctx->scene->transforms[12].translation = vec3f(0.0f, 100.0f, 0.0f);
    live_ctx->scene->entities[12] |= e_cmp::transform;
    
    return 0;
}

CR_EXPORT int cr_main(struct cr_plugin *ctx, enum cr_op operation)
{
    ecs::live_context* live_ctx = (ecs::live_context*)ctx->userdata;
    
    switch (operation)
    {
        case CR_LOAD:
            return on_load(live_ctx);
        case CR_UNLOAD:
            return on_unload();
        case CR_CLOSE:
            return 0;
        default:
            break;
    }
    
    return ion_update(live_ctx);
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

