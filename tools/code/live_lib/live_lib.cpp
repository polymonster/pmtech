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
    
    struct lane
    {
        u32*     entities;
        u32*     entity_target;
        vec3f*   targets;
    };
}

struct live_lib : public __ecs, public __dbg
{
    ecs_scene*          scene;
    u32                 box_start = 0;
    u32                 box_end;
    u32                 quadrant_size = 0;
    u32                 box_count = 0;
    static const s32    lanes = 5;
    lane                lane_info[lanes] = { 0 };
    
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

        // directional lights
        u32 light = get_new_entity(scene);
        instantiate_light(scene, light);
        scene->names[light] = "front_light";
        scene->id_name[light] = PEN_HASH("front_light");
        scene->lights[light].colour = vec3f(0.8f, 0.8f, 0.8f) * 0.5f;
        scene->lights[light].direction = normalised(vec3f(-0.7f, 0.6f, -0.4f));
        scene->lights[light].type = e_light_type::dir;
        scene->lights[light].flags |= e_light_flags::shadow_map | e_light_flags::global_illumination;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        light = get_new_entity(scene);
        instantiate_light(scene, light);
        scene->names[light] = "opposite_light";
        scene->id_name[light] = PEN_HASH("opposite_light");
        scene->lights[light].colour = vec3f(0.8f, 0.8f, 0.8f) * 0.5f;
        scene->lights[light].direction = normalised(vec3f(0.7f, 0.8f, 0.3f));
        scene->lights[light].type = e_light_type::dir;
        scene->lights[light].flags |= e_light_flags::shadow_map | e_light_flags::global_illumination;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        f32 dim = 64.0f;
        
        u32 ground = get_new_entity(scene);
        scene->transforms[ground].rotation = quat();
        scene->transforms[ground].scale = vec3f(dim, 1.0f, dim);
        scene->transforms[ground].translation = vec3f(0.0f, -1.0f, 0.0f);
        scene->parents[ground] = ground;
        scene->entities[ground] |= e_cmp::transform;
        scene->material_permutation[ground] |= FORWARD_LIT_GI;
        instantiate_geometry(box_resource, scene, ground);
        instantiate_material(default_material, scene, ground);
        instantiate_model_cbuffer(scene, ground);
        
        vec3f lane_space = vec3f(0.0f, 0.0f, 8.0f);
                
        // x wall
        u32 wall = get_new_entity(scene);
        scene->transforms[wall].rotation = quat();
        scene->transforms[wall].scale = vec3f(10.0f, 10.0f, 3.0f);
        scene->transforms[wall].translation = vec3f(0.0f, 0.0f, -35.0f);
        scene->parents[wall] = wall;
        scene->entities[wall] |= e_cmp::transform;
        scene->material_permutation[wall] |= FORWARD_LIT_GI;
        instantiate_geometry(box_resource, scene, wall);
        instantiate_material(default_material, scene, wall);
        instantiate_model_cbuffer(scene, wall);
        
        // z wall
        wall = get_new_entity(scene);
        scene->transforms[wall].rotation = quat();
        scene->transforms[wall].scale = vec3f(3.0f, 15.0f, 15.0f);
        scene->transforms[wall].translation = vec3f(35.0f, 0.0f, 0.0f);
        scene->parents[wall] = wall;
        scene->entities[wall] |= e_cmp::transform;
        scene->material_permutation[wall] |= FORWARD_LIT_GI;
        instantiate_geometry(box_resource, scene, wall);
        instantiate_material(default_material, scene, wall);
        instantiate_model_cbuffer(scene, wall);
        
        vec4f lane_colours[] = {
            {180.0f, 237.0f, 210.0f, 255.0f},
            {200.0f, 40.0f, 255.0f, 255.0f},
            {160.0f, 207.0f, 211.0f, 255.0f},
            {255.0f, 128.0f, 40.0f, 255.0f},
            {141.0f, 148.0f, 186.0f, 255.0f}
        };
        
        vec3f space = vec3f(8.0f, 0.0f, 0.0);
        vec3f start = vec3f(0.0f, 3.0f, 0.0) - (space * ((f32)lanes-1.0f) * 0.5f) - (lane_space * ((f32)lanes-1.0f) * 0.5f);
        for(u32 i = 0; i < lanes; ++i)
        {
            for(u32 j = 0; j < lanes; ++j)
            {
                u32 box = get_new_entity(scene);
                scene->transforms[box].rotation = quat();
                scene->transforms[box].scale = vec3f(3.0f, 3.0f, 3.0f);
                scene->transforms[box].translation = start + space * (f32)j;
                scene->parents[box] = box;
                scene->entities[box] |= e_cmp::transform;
                scene->material_permutation[box] |= FORWARD_LIT_GI;
                instantiate_geometry(box_resource, scene, box);
                instantiate_material(default_material, scene, box);
                instantiate_model_cbuffer(scene, box);
                
                forward_render::forward_lit* mat = (forward_render::forward_lit*) & scene->material_data[box].data[0];
                mat->m_albedo = lane_colours[i]/255.0f;
                box_count++;
                
                sb_push(lane_info[i].entities, box);
                sb_push(lane_info[i].entity_target, 0);
                
            }
            
            start += lane_space;
        }
        
        start = vec3f(0.0f, 3.0f, 0.0) - (space * ((f32)lanes-1.0f) * 0.5f) - (lane_space * ((f32)lanes-1.0f) * 0.5f);
        
        // setup movement targets
        sb_push(lane_info[1].targets, start + lane_space * 1.0f - space);
        sb_push(lane_info[1].targets, start + lane_space * 5.0f - space);
        sb_push(lane_info[1].targets, start + lane_space * 5.0f + space * 5.0f);
        sb_push(lane_info[1].targets, start + lane_space * 1.0f + space * 5.0f);

        sb_push(lane_info[3].targets, start + lane_space * 3.0f + space * 5.0f);
        sb_push(lane_info[3].targets, start - lane_space * 1.0f + space * 5.0f);
        sb_push(lane_info[3].targets, start - lane_space * 1.0f - space);
        sb_push(lane_info[3].targets, start + lane_space * 3.0f - space);
        
        
        return 0;
    }
    
    int on_update(f32 dt)
    {
        // animate second lane
        for(u32 l = 0; l < lanes; ++l)
        {
            if(l != 1 && l != 3)
                continue;
                
            u32 num = sb_count(lane_info[l].entities);
            u32 num_targets = sb_count(lane_info[l].targets);
            for(u32 i = 0; i < num; ++i)
            {
                u32 e = lane_info[l].entities[i];
                vec3f target = lane_info[l].targets[lane_info[l].entity_target[i]];
                vec3f& t = scene->transforms[e].translation;
                vec3f v = target - t;
                f32 d = mag2(v);
                if(d < 36.0f)
                {
                    t = lerp(t, target, 0.2f);
                    
                    if(d < 0.01f)
                    {
                        lane_info[l].entity_target[i] = (lane_info[l].entity_target[i] + 1) % num_targets;
                        continue;
                    }

                }
                
                t += normalised(v) * 1.0f/60.0f * 10.0f;
                
                scene->entities[e] |= e_cmp::transform;
            }
        }

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


