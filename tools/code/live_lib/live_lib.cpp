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

namespace
{
    s32 randri(s32 min, s32 max)
    {
        s32 range = max - min;
        if(range == 0)
            return min;
        return min + rand()%range;
    }
}

struct live_lib : public __ecs, public __dbg
{
    ecs_scene*          scene;
    u32                 box_start = 0;
    u32                 box_end;
    u32                 quadrant_size = 0;
    u32                 box_count = 0;
    
    void init(live_context* ctx)
    {
        scene = ctx->scene;
        memcpy(&__ecs_start, &ctx->ecs_funcs->__ecs_start, (intptr_t)&ctx->ecs_funcs->__ecs_end - (intptr_t)&ctx->ecs_funcs->__ecs_start);
        memcpy(&__dbg_start, &ctx->dbg_funcs->__dbg_start, (intptr_t)&ctx->dbg_funcs->__dbg_end - (intptr_t)&ctx->dbg_funcs->__dbg_start);
    }
    
    int on_load(live_context* ctx)
    {
        init(ctx);
        
        box_count = 0;
        
        material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
        geometry_resource* box_resource = get_geometry_resource(PEN_HASH("cube"));
            
        clear_scene(scene);

        // directional light
        u32 light = get_new_entity(scene);
        instantiate_light(scene, light);
        scene->names[light] = "front_light";
        scene->id_name[light] = PEN_HASH("front_light");
        scene->lights[light].colour = vec3f(0.8f, 0.8f, 0.8f);
        scene->lights[light].direction = normalised(vec3f(-0.7f, 0.6f, -0.4f));
        scene->lights[light].type = e_light_type::dir;
        scene->lights[light].flags |= e_light_flags::shadow_map;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        f32 dim = 128.0f;
        f32 inset = 16.0f;
        
        u32 ground = get_new_entity(scene);
        scene->transforms[ground].rotation = quat();
        scene->transforms[ground].scale = vec3f(dim, 1.0f, dim);
        scene->transforms[ground].translation = vec3f(0.0f, -dim + inset, 0.0f);
        scene->parents[ground] = ground;
        scene->entities[ground] |= e_cmp::transform;
        instantiate_geometry(box_resource, scene, ground);
        instantiate_material(default_material, scene, ground);
        instantiate_model_cbuffer(scene, ground);
        
        forward_render::forward_lit* mat = (forward_render::forward_lit*) & scene->material_data[ground].data[0];
        //mat->m_albedo = vec4f(0.0f, 0.5f, 0.5f, 1.0f);
        
        u32 wall = get_new_entity(scene);
        scene->transforms[wall].rotation = quat();
        scene->transforms[wall].scale = vec3f(1.0f, dim, dim);
        scene->transforms[wall].translation = vec3f(dim - inset, 0.0f, 0.0f);
        scene->parents[wall] = wall;
        scene->entities[wall] |= e_cmp::transform;
        instantiate_geometry(box_resource, scene, wall);
        instantiate_material(default_material, scene, wall);
        instantiate_model_cbuffer(scene, wall);
        
        wall = get_new_entity(scene);
        scene->transforms[wall].rotation = quat();
        scene->transforms[wall].scale = vec3f(dim, dim, 1.0);
        scene->transforms[wall].translation = vec3f(0.0f, 0.0f, dim - inset);
        scene->parents[wall] = wall;
        scene->entities[wall] |= e_cmp::transform;
        instantiate_geometry(box_resource, scene, wall);
        instantiate_material(default_material, scene, wall);
        instantiate_model_cbuffer(scene, wall);
        
        for(u32 side = 0; side < 8; ++side)
        {
            for(u32 i = 0; i < 3; ++i)
            {
                for(int j = 0; j < 3; ++j)
                {
                    u32 box = get_new_entity(scene);
                    scene->transforms[box].rotation = quat();
                    scene->transforms[box].scale = vec3f(5.0f, 5.0f, 5.0f);
                    
                    scene->transforms[box].translation = vec3f(i * 20.0f, -dim + inset + 12.0f *((f32)side+1), j * 20.0f);
                    scene->transforms[box].translation  += vec3f(60.0f, 0.0f, 60.0f);
                    scene->transforms[box].scale = vec3f::zero();
                    
                    scene->parents[box] = box;
                    scene->entities[box] |= e_cmp::transform;

                    instantiate_geometry(box_resource, scene, box);
                    instantiate_material(default_material, scene, box);
                    instantiate_model_cbuffer(scene, box);
                    
                    forward_render::forward_lit* mat = (forward_render::forward_lit*) & scene->material_data[box].data[0];
                    mat->m_albedo = vec4f(1.0f, 1.0f - side/8.0f, 1.0f - side/8.0f, 1.0f);
                    
                    vec4f rr = vec4f(1.0f - side/8.0f, 1.0f, 1.0f, 1.0f);
                    
                    mat->m_albedo = rr;
                    mat->m_albedo = rr.yxxw;
                    
                    if(box_start == 0)
                        box_start = box;
                    
                    box_count++;
                }
            }
        }

                    
        return 0;
    }
    
    int on_update(f32 dt)
    {
        static f32 track = box_start - 1;
        static f32 dir = 1.0f;
        for(u32 i = box_start; i < box_start + box_count; ++i)
        {
            if(track > i)
            {
                scene->transforms[i].scale = lerp(scene->transforms[i].scale, vec3f(5.0f, 5.0f, 5.0f), 0.8f);
            }
            else
            {
                scene->transforms[i].scale = lerp(scene->transforms[i].scale, vec3f(0.0f, 0.0f, 0.0f), 0.8f);
            }
                        
            scene->entities[i] |= e_cmp::transform;
        }
        
        if(track > box_start + box_count)
            dir = -1.0f;
        
        if(track <= box_start - 1)
            dir = 1.0f;
        
        track +=  dt * 10.0f * dir;
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


