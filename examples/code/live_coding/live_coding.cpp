#include "cr/cr.h"
#include "ecs/ecs_scene.h"

#include <stdio.h>

using namespace put;
using namespace ecs;

CR_EXPORT int cr_main(struct cr_plugin *ctx, enum cr_op operation)
{
    ecs_scene* scene = (ecs_scene*)ctx->userdata;
    
    scene->lights[0].colour = vec3f(1.0f, 0.0f, 1.0f);
    
    scene->transforms[1].translation = vec3f(0.0f, 20.0f, 0.0f);
    scene->entities[1] |= e_cmp::transform;
    
    scene->transforms[2].translation = vec3f(0.0f, 0.0f, 50.0f);
    scene->entities[2] |= e_cmp::transform;
    
    return 0;
}
