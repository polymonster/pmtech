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
#include <xmmintrin.h>
#include <immintrin.h>

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
        
        clear_scene(scene);
        
        camera_create_perspective(&cull_cam, 60.0f, 16.0f/9.0f, 0.01f, 200.0f);
        
        f32 space = 10.0f;
        f32 size = 1.0f;
        f32 num = 32.0f;
        
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
                    
                    scene->entities[b] |= e_cmp::constraint;
                    scene->entities[b] |= e_cmp::volume;
                    
                    if((i & 1) && (j & 1) && (k & 1))
                    {
                        scene->entities[b] |= e_cmp::physics_multi;
                    }
                    
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
    
    void filter_entities_scalar(const ecs_scene* scene, u32** filtered_entities_out)
    {
        static timer* ct = timer_create();
        timer_start(ct);
        
        u32 accept_entities = e_cmp::constraint | e_cmp::volume;
        u32 reject_entities = e_cmp::physics_multi;
        
        for(u32 i = 0; i < scene->num_entities; ++i)
        {
            // entity flags accept
            if((scene->entities[i] & accept_entities) != accept_entities)
                continue;
                
            // entity flags reject
            if(reject_entities)
                if(scene->entities[i] & reject_entities)
                    continue;
                        
            sb_push(*filtered_entities_out, i);
        }

        f64 us = timer_elapsed_us(ct);
        PEN_LOG("filter time %f (us)", us);
    }
         
    void frustum_cull_sphere_simd128(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** visible_entities_out)
    {
        static timer* ct = timer_create();
        timer_start(ct);
        
        const frustum& frust = cam->camera_frustum;
        
        // splat constants
        __m128 point_five = _mm_set1_ps(0.5f);
        
        // min max extents
        __m128 minx;
        __m128 miny;
        __m128 minz;
        __m128 maxx;
        __m128 maxy;
        __m128 maxz;
        
        // sphere radius and position
        __m128 radius;
        __m128 posx;
        __m128 posy;
        __m128 posz;
        __m128 sizex;
        __m128 sizey;
        __m128 sizez;
        
        // plane normal
        __m128 pnx[6];
        __m128 pny[6];
        __m128 pnz[6];
        
        // plane distance
        __m128 pd[6];
        
        f32 result[4];
        
        u32 e[4];
        
        // load camera planes
        for (s32 p = 0; p < 6; ++p)
        {
            f32 ppd = maths::plane_distance(frust.p[p], frust.n[p]);
            pnx[p] = _mm_set1_ps(frust.n[p].x);
            pny[p] = _mm_set1_ps(frust.n[p].y);
            pnz[p] = _mm_set1_ps(frust.n[p].z);
            pd[p] = _mm_set1_ps(ppd);
        }
        
        u32 n = sb_count(entities_in);
        for(u32 i = 0; i < n; i+=4)
        {
            // unpack entities
            for(u32 j = 0; j < 4; ++j)
                e[j] = entities_in[i+j];

            // load entities values
            auto& bv_min_0 = scene->bounding_volumes[e[0]].transformed_min_extents;
            auto& bv_min_1 = scene->bounding_volumes[e[1]].transformed_min_extents;
            auto& bv_min_2 = scene->bounding_volumes[e[2]].transformed_min_extents;
            auto& bv_min_3 = scene->bounding_volumes[e[3]].transformed_min_extents;
            auto& bv_max_0 = scene->bounding_volumes[e[0]].transformed_max_extents;
            auto& bv_max_1 = scene->bounding_volumes[e[1]].transformed_max_extents;
            auto& bv_max_2 = scene->bounding_volumes[e[2]].transformed_max_extents;
            auto& bv_max_3 = scene->bounding_volumes[e[3]].transformed_max_extents;
            
            auto& r0 = scene->bounding_volumes[e[0]].radius;
            auto& r1 = scene->bounding_volumes[e[1]].radius;
            auto& r2 = scene->bounding_volumes[e[2]].radius;
            auto& r3 = scene->bounding_volumes[e[3]].radius;
            
            minx = _mm_set_ps(bv_min_0.x, bv_min_1.x, bv_min_2.x, bv_min_3.x);
            miny = _mm_set_ps(bv_min_0.y, bv_min_1.y, bv_min_2.y, bv_min_3.y);
            minz = _mm_set_ps(bv_min_0.z, bv_min_1.z, bv_min_2.z, bv_min_3.z);
            
            maxx = _mm_set_ps(bv_max_0.x, bv_max_1.x, bv_max_2.x, bv_max_3.x);
            maxy = _mm_set_ps(bv_max_0.y, bv_max_1.y, bv_max_2.y, bv_max_3.y);
            maxz = _mm_set_ps(bv_max_0.z, bv_max_1.z, bv_max_2.z, bv_max_3.z);
            
            radius = _mm_set_ps(r0, r1, r2, r3);
            
            sizex = _mm_sub_ps(maxx, minx);
            sizey = _mm_sub_ps(maxy, miny);
            sizez = _mm_sub_ps(maxz, minz);
            
            posx = _mm_fmadd_ps(sizex, point_five, minx);
            posy = _mm_fmadd_ps(sizex, point_five, miny);
            posz = _mm_fmadd_ps(sizex, point_five, minz);
            
            __m128 inside = _mm_set1_ps(0.0f);
            
            for (s32 p = 0; p < 6; ++p)
            {
                // get distance to plane
                // dot product with plane normal and also add plane distance
                __m128 dd = _mm_fmadd_ps(posx, pnx[p], pd[p]);
                dd = _mm_fmadd_ps(posy, pny[p], dd);
                dd = _mm_fmadd_ps(posz, pnz[p], dd);
                
                // compare if dd is greater than radius, if so we are outside
                __m128 ge = _mm_cmpge_ps(dd, radius);
                inside = _mm_add_ps(ge, inside);
            }
                 
            _mm_store_ps(result, inside);
            
            if(!inside[0])
                sb_push(*visible_entities_out, e[3]);
            if(!inside[1])
                sb_push(*visible_entities_out, e[2]);
            if(!inside[2])
                sb_push(*visible_entities_out, e[1]);
            if(!inside[3])
                sb_push(*visible_entities_out, e[0]);
        }
        
        f64 us = timer_elapsed_us(ct);
        PEN_LOG("sphere simd 128 cull time %f (us)", us);
    }
    
    void frustum_cull_sphere_scalar(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** visible_entities_out)
    {
        static timer* ct = timer_create();
        timer_start(ct);
        
        const frustum& camera_frustum = cam->camera_frustum;
        
        u32 n = sb_count(entities_in);
        for(u32 i = 0; i < n; ++i)
        {
            u32 e = entities_in[i];
            
            const vec3f& min = scene->bounding_volumes[e].transformed_min_extents;
            const vec3f& max = scene->bounding_volumes[e].transformed_max_extents;

            vec3f pos = min + (max - min) * 0.5f;
            f32   radius = scene->bounding_volumes[e].radius;
                
            bool inside = true;
            for (s32 p = 0; p < 6; ++p)
            {
                f32 d = maths::point_plane_distance(pos, camera_frustum.p[p], camera_frustum.n[p]);

                if (d > radius)
                {
                    inside = false;
                    break;
                }
            }
            
            if(inside)
            {
                sb_push(*visible_entities_out, e);
            }
        }
        
        f64 us = timer_elapsed_us(ct);
        PEN_LOG("spahere scalar cull time %f (us)", us);
    }
    
    int on_update(f32 dt)
    {
        camera_update_look_at(&cull_cam);
        camera_update_frustum(&cull_cam);
        
        add_frustum(&cull_cam.camera_frustum.corners[0][0], &cull_cam.camera_frustum.corners[1][0], vec4f::green());
        
        u32* filtered_entities = nullptr;
        filter_entities_scalar(scene, &filtered_entities);
        
        u32* visible_entities = nullptr;
        //frustum_cull_sphere_scalar(scene, &cull_cam, filtered_entities, &visible_entities);
        frustum_cull_sphere_simd128(scene, &cull_cam, filtered_entities, &visible_entities);
        
        u32 n = sb_count(visible_entities);
        for(u32 i = 0; i < n; ++i)
        {
            u32 e = visible_entities[i];
            cmp_bounding_volume& bv = scene->bounding_volumes[e];
            
            vec4f col = vec4f::white();
            if(scene->entities[e] & e_cmp::physics_multi)
                col = vec4f::red();
                
            add_aabb(bv.transformed_min_extents, bv.transformed_max_extents, col);
        }
        
        sb_free(visible_entities);
        sb_free(filtered_entities);
        
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


