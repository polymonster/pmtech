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
    
    void frustum_cull_sphere_simd256(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** visible_entities_out)
    {
#ifdef __AVX__
        static timer* ct = timer_create();
        timer_start(ct);
        
        const frustum& frust = cam->camera_frustum;
        
        // splat constants
        __m256 zero = _mm256_set1_ps(0.0f);
        
        // sphere radius and position
        __m256 radius;
        __m256 posx;
        __m256 posy;
        __m256 posz;
        
        // plane normal
        __m256 pnx[6];
        __m256 pny[6];
        __m256 pnz[6];
        
        // plane distance
        __m256 pd[6];
        
        f32 result[8];
        u32 e[8];
        
        // load camera planes
        for (s32 p = 0; p < 6; ++p)
        {
            f32 ppd = maths::plane_distance(frust.p[p], frust.n[p]);
            pnx[p] = _mm256_set1_ps(frust.n[p].x);
            pny[p] = _mm256_set1_ps(frust.n[p].y);
            pnz[p] = _mm256_set1_ps(frust.n[p].z);
            pd[p] = _mm256_set1_ps(ppd);
        }
        
        u32 n = sb_count(entities_in);
        for(u32 i = 0; i < n; i+=8)
        {
            // unpack entities
            for(u32 j = 0; j < 8; ++j)
                e[j] = entities_in[i+j];

            // load entities values
            auto& p0 = scene->pos_extent[e[0]].pos;
            auto& p1 = scene->pos_extent[e[1]].pos;
            auto& p2 = scene->pos_extent[e[2]].pos;
            auto& p3 = scene->pos_extent[e[3]].pos;
            auto& p4 = scene->pos_extent[e[4]].pos;
            auto& p5 = scene->pos_extent[e[5]].pos;
            auto& p6 = scene->pos_extent[e[6]].pos;
            auto& p7 = scene->pos_extent[e[7]].pos;
            auto& r0 = scene->pos_extent[e[0]].extent;
            auto& r1 = scene->pos_extent[e[1]].extent;
            auto& r2 = scene->pos_extent[e[2]].extent;
            auto& r3 = scene->pos_extent[e[3]].extent;
            auto& r4 = scene->pos_extent[e[4]].extent;
            auto& r5 = scene->pos_extent[e[5]].extent;
            auto& r6 = scene->pos_extent[e[6]].extent;
            auto& r7 = scene->pos_extent[e[7]].extent;
            
            radius = _mm256_set_ps(r0.w, r1.w, r2.w, r3.w, r4.w, r5.w, r6.w, r7.w);
                        
            posx = _mm256_set_ps(p0.x, p1.x, p2.x, p3.x, p4.x, p5.x, p6.x, p7.x);
            posy = _mm256_set_ps(p0.y, p1.y, p2.y, p3.y, p4.y, p5.y, p6.y, p7.y);
            posz = _mm256_set_ps(p0.z, p1.z, p2.z, p3.z, p4.z, p5.z, p6.z, p7.z);
            
            __m256 inside = _mm256_set1_ps(0.0f);
            
            for (s32 p = 0; p < 6; ++p)
            {
                // get distance to plane
                // dot product with plane normal and also add plane distance
                __m256 dd = _mm256_fmadd_ps(posx, pnx[p], pd[p]);
                dd = _mm256_fmadd_ps(posy, pny[p], dd);
                dd = _mm256_fmadd_ps(posz, pnz[p], dd);
                
                // compare if dd is greater than radius, if so we are outside
                //__m256 ge = _mm256_cmpge_ps(dd, radius);
                //inside = _mm256_add_ps(ge, inside);
                
                __m256 diff = _mm256_sub_ps(dd, radius);
                diff = _mm256_max_ps(diff, zero);
                inside = _mm256_add_ps(diff, inside);
            }
                 
            _mm256_store_ps(result, inside);
            for(u32 j = 0; j < 8; ++j)
                if(!inside[j])
                    sb_push(*visible_entities_out, e[7-j]);
        }
        
        f64 us = timer_elapsed_us(ct);
        PEN_LOG("sphere simd 256 cull time %f (us)", us);
#endif
    }
         
    void frustum_cull_aabb_simd128(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** visible_entities_out)
    {
#ifdef __SSE2__
        static timer* ct = timer_create();
        timer_start(ct);
        
        const frustum& frust = cam->camera_frustum;
        
        // sphere radius and position
        __m128 posx;
        __m128 posy;
        __m128 posz;
        __m128 extx;
        __m128 exty;
        __m128 extz;

        // plane normal
        __m128 pnx[6];
        __m128 pny[6];
        __m128 pnz[6];
        
        // plane distance
        __m128 pd[6];
        
        // plane sign flip
        __m128 sfx[6];
        __m128 sfy[6];
        __m128 sfz[6];
        
        f32 result[4];
        u32 e[4];
        
        // load camera planes
        for (s32 p = 0; p < 6; ++p)
        {
            f32 ppd = maths::plane_distance(frust.p[p], frust.n[p]);
            pnx[p] = _mm_set1_ps(frust.n[p].x);
            pny[p] = _mm_set1_ps(frust.n[p].y);
            pnz[p] = _mm_set1_ps(frust.n[p].z);
            pd[p] = _mm_set1_ps(-ppd);
            
            sfx[p] = _mm_set1_ps(sgn(frust.n[p].x));
            sfy[p] = _mm_set1_ps(sgn(frust.n[p].y));
            sfz[p] = _mm_set1_ps(sgn(frust.n[p].z));
        }
                
        u32 n = sb_count(entities_in);
        for(u32 i = 0; i < n; ++i)
        {
            // unpack entities
            for(u32 j = 0; j < 4; ++j)
                e[j] = entities_in[i+j];
                        
            auto& p0 = scene->pos_extent[e[0]].pos;
            auto& p1 = scene->pos_extent[e[1]].pos;
            auto& p2 = scene->pos_extent[e[2]].pos;
            auto& p3 = scene->pos_extent[e[3]].pos;
            auto& e0 = scene->pos_extent[e[0]].extent;
            auto& e1 = scene->pos_extent[e[1]].extent;
            auto& e2 = scene->pos_extent[e[2]].extent;
            auto& e3 = scene->pos_extent[e[3]].extent;
            
            posx = _mm_set_ps(p0.x, p1.x, p2.x, p3.x);
            posy = _mm_set_ps(p0.y, p1.y, p2.y, p3.y);
            posz = _mm_set_ps(p0.z, p1.z, p2.z, p3.z);
            
            extx = _mm_set_ps(e0.x, e1.x, e2.x, e3.x);
            exty = _mm_set_ps(e0.y, e1.y, e2.y, e3.y);
            extz = _mm_set_ps(e0.z, e1.z, e2.z, e3.z);
            
            __m128 inside = _mm_set1_ps(0.0f);
            
            for (s32 p = 0; p < 6; ++p)
            {
                // get distance to plane
                // dot product with plane normal and also add plane distance
                __m128 dd = _mm_fmadd_ps(posx, pnx[p], pd[p]);
                dd = _mm_fmadd_ps(posy, pny[p], dd);
                dd = _mm_fmadd_ps(posz, pnz[p], dd);
                
                // pos + extent * sign_flip
                __m128 dpx = _mm_fmadd_ps(extx, sfx[p], posx);
                __m128 dpy = _mm_fmadd_ps(exty, sfy[p], posy);
                __m128 dpz = _mm_fmadd_ps(extz, sfz[p], posz);
                
                // dot(pos + extent * sign_flip, frust.n[p]);
                __m128 r = _mm_mul_ps(dpx, pnx[p]);
                r = _mm_fmadd_ps(dpy, pny[p], r);
                r = _mm_fmadd_ps(dpz, pnz[p], r);
                
                // if(r > pd) inside = false
                __m128 ge = _mm_cmpge_ps(r, pd[p]);
                inside = _mm_add_ps(ge, inside);
            }
            
            _mm_store_ps(result, inside);
            for(u32 j = 0; j < 4; ++j)
                if(!inside[j])
                    sb_push(*visible_entities_out, e[3-j]);
        }
        
        f64 us = timer_elapsed_us(ct);
        PEN_LOG("aabb simd 128 cull time %f (us)", us);
#endif
    }
    
    void frustum_cull_sphere_simd128(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** visible_entities_out)
    {
#ifdef __SSE2__
        static timer* ct = timer_create();
        timer_start(ct);
        
        const frustum& frust = cam->camera_frustum;
                
        // sphere radius and position
        __m128 radius;
        __m128 posx;
        __m128 posy;
        __m128 posz;

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
            auto& p0 = scene->pos_extent[e[0]].pos;
            auto& p1 = scene->pos_extent[e[1]].pos;
            auto& p2 = scene->pos_extent[e[2]].pos;
            auto& p3 = scene->pos_extent[e[3]].pos;
            auto& r0 = scene->pos_extent[e[0]].extent;
            auto& r1 = scene->pos_extent[e[1]].extent;
            auto& r2 = scene->pos_extent[e[2]].extent;
            auto& r3 = scene->pos_extent[e[3]].extent;
            
            radius = _mm_set_ps(r0.w, r1.w, r2.w, r3.w);
            
            posx = _mm_set_ps(p0.x, p1.x, p2.x, p3.x);
            posy = _mm_set_ps(p0.y, p1.y, p2.y, p3.y);
            posz = _mm_set_ps(p0.z, p1.z, p2.z, p3.z);
            
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
            for(u32 j = 0; j < 4; ++j)
                if(!inside[j])
                    sb_push(*visible_entities_out, e[3-j]);
        }
        
        f64 us = timer_elapsed_us(ct);
        PEN_LOG("sphere simd 128 cull time %f (us)", us);
#endif
    }
    
    void frustum_cull_aabb_scalar(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** visible_entities_out)
    {
        static timer* ct = timer_create();
        timer_start(ct);
        
        const frustum& frust = cam->camera_frustum;
                
        u32 n = sb_count(entities_in);
        for(u32 i = 0; i < n; ++i)
        {
            u32 e = entities_in[i];
                        
            vec3f pos = scene->pos_extent[e].pos.xyz;
            vec3f extent = scene->pos_extent[e].extent.xyz;
            
            bool inside = true;
            for (s32 p = 0; p < 6; ++p)
            {
                vec3f sign_flip = sgn(frust.n[p]);
                f32 pd = maths::plane_distance(frust.p[p], frust.n[p]);
                f32 d = dot(pos + extent * sign_flip, frust.n[p]);
                if(d > -pd)
                {
                    inside = false;
                }
            }
            
            if(inside)
            {
                sb_push(*visible_entities_out, e);
            }
        }
        
        f64 us = timer_elapsed_us(ct);
        PEN_LOG("spahere aabb cull time %f (us)", us);
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
            
            vec3f pos = scene->pos_extent[e].pos.xyz;
            f32   radius = scene->pos_extent[e].extent.w;
                
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
        //frustum_cull_sphere_simd128(scene, &cull_cam, filtered_entities, &visible_entities);
        //frustum_cull_sphere_simd256(scene, &cull_cam, filtered_entities, &visible_entities);
        //frustum_cull_aabb_scalar(scene, &cull_cam, filtered_entities, &visible_entities);
        frustum_cull_aabb_simd128(scene, &cull_cam, filtered_entities, &visible_entities);
        
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


