#include "cr/cr.h"
#include "ecs/ecs_scene.h"
#include "ecs/ecs_utilities.h"
#include "ecs/ecs_resources.h"

#define DLL 1
#include "ecs/ecs_live.h"
#include "str/Str.cpp"

#include "renderer.h"
#include "data_struct.h"
#include "timer.h"

#include "maths/maths.h"
#include "../../shader_structs/forward_render.h"

#include <stdio.h>

struct live_lib : public live_context
{
    u32                 box_start = 0;
    u32                 box_end;
    camera              cull_cam;
    
    void init(live_context* ctx)
    {
        *(live_context*)this = *ctx;
    }
    
    int on_load(live_context* ctx)
    {
        init(ctx);
        
        camera_create_perspective(&cull_cam, 60.0f, 16.0f/9.0f, 0.01f, 1000.0f);
        
        f32 space = 10.0f;
        f32 size = 1.0f;
        f32 num = 16.0f;
        
        vec3f start = - (vec3f(space) * num) * 0.5f;
        vec3f pos = start;
        
        for(u32 i = 0; i < (u32)num; i++)
        {
            pos.y = start.y;
            
            for(u32 j = 0; j < (u32)num; j++)
            {
                pos.x = start.x;
                
                for(u32 k = 0; k < (u32)num; k++)
                {
                    u32 b = get_new_entity(scene);
                    scene->transforms[b].translation = pos;
                    scene->transforms[b].rotation = quat();
                    scene->transforms[b].scale = vec3f(size);
                    scene->bounding_volumes[b].min_extents = vec3f(-1.0f);
                    scene->bounding_volumes[b].max_extents = vec3f(1.0f);
                    scene->entities[b] |= e_cmp::transform;
                    
                    if(i == 0 && j == 0 && k == 0)
                    {
                        box_start = b;
                    }
                    
                    box_end = b;
                    pos.x += space;
                }
                
                pos.y += space;
            }
            
            pos.z += space;
        }
        
        return 0;
    }
    
    void frustum_cull_sphere_scalar(const ecs_scene* scene, const camera* cam, u32** visible_entities_out)
    {
        static timer* ct = timer_create();
        timer_start(ct);
        
        const frustum& camera_frustum = cam->camera_frustum;
        
        for(u32 i = box_start; i < box_end+1; ++i)
        {
            bool inside = true;
            for (s32 p = 0; p < 6; ++p)
            {
                const vec3f& min = scene->bounding_volumes[i].transformed_min_extents;
                const vec3f& max = scene->bounding_volumes[i].transformed_max_extents;

                vec3f pos = min + (max - min) * 0.5f;
                f32   radius = scene->bounding_volumes[i].radius;

                f32 d = maths::point_plane_distance(pos, camera_frustum.p[p], camera_frustum.n[p]);

                if (d > radius)
                {
                    inside = false;
                    break;
                }
            }
            
            if(inside)
            {
                sb_push(*visible_entities_out, i);
            }
        }
        
        f64 us = timer_elapsed_us(ct);
        PEN_LOG("cull time %f (ms)", us/1000);
    }
    
    int on_update(f32 dt)
    {
        camera_update_look_at(&cull_cam);
        camera_update_frustum(&cull_cam);
        
        add_frustum(&cull_cam.camera_frustum.corners[0][0], &cull_cam.camera_frustum.corners[1][0], vec4f::green());
        
        u32* visible_entities = nullptr;
        frustum_cull_sphere_scalar(scene, &cull_cam, &visible_entities);
        
        u32 n = sb_count(visible_entities);
        for(u32 i = 0; i < n; ++i)
        {
            u32 e = visible_entities[i];
            cmp_bounding_volume& bv = scene->bounding_volumes[e];
            add_aabb(bv.transformed_min_extents, bv.transformed_max_extents, vec4f::red());
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


