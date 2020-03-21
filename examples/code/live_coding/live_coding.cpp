#define PMTECH_LIVE 1
#include "cr/cr.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "ecs/ecs_resources.h"

#include <stdio.h>

using namespace put;
using namespace ecs;

CR_EXPORT int cr_main(struct cr_plugin *ctx, enum cr_op operation)
{
    return 0;
    
    ecs_scene* scene = (ecs_scene*)ctx->userdata;
    
    scene->lights[0].colour = vec3f(1.0f, 1.0f, 1.0f);
    scene->lights[0].flags &= ~e_light_flags::shadow_map;
    
    scene->transforms[1].translation = vec3f(0.0f, 0.0f, 0.0f);
    scene->entities[1] |= e_cmp::transform;
    
    scene->transforms[2].translation = vec3f(0.0f, 50.0f, 50.0f);
    scene->entities[2] |= e_cmp::transform;
    
    static bool add = true;
    if(add)
    {
        // directional
        u32 light = get_new_entity(scene);
        instantiate_light(scene, light);
        scene->names[light] = "front_light";
        scene->id_name[light] = PEN_HASH("front_light");
        scene->lights[light].colour = vec3f::one() * 0.3f;
        scene->lights[light].direction = vec3f::one();
        scene->lights[light].type = e_light_type::dir;
        scene->lights[light].flags |= e_light_flags::shadow_map;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        add = false;
    }
    
    return 0;
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

