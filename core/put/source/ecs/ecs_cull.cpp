#include "ecs_cull.h"

#include "timer.h"
#include "ecs_scene.h"

#include <xmmintrin.h>
#include <immintrin.h>

using namespace::pen;

namespace put
{
    namespace ecs
    {
        //
        // scalar float implementation
        //
        
        void frustum_cull_aabb_scalar(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out)
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
                    sb_push(*entities_out, e);
                }
            }
            
            f64 us = timer_elapsed_us(ct);
            PEN_LOG("aabb scalar cull time %f (us)", us);
        }
        
        void frustum_cull_sphere_scalar(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out)
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
                    sb_push(*entities_out, e);
                }
            }
            
            f64 us = timer_elapsed_us(ct);
            PEN_LOG("spahere scalar cull time %f (us)", us);
        }
        
        void filter_entities_scalar(const ecs_scene* scene, u32** entities_out)
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
                            
                sb_push(*entities_out, i);
            }

            f64 us = timer_elapsed_us(ct);
            PEN_LOG("filter time %f (us)", us);
        }
        
        //
        // sse2 128 implementation
        //
#ifdef __SSE2__
        void frustum_cull_aabb_simd128(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out)
        {
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
            __m128 pd_neg[6];
            
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
                pd[p] = _mm_set1_ps(ppd);
                pd_neg[p] = _mm_set1_ps(-ppd);
                
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
                    
                    // if(r > -pd) inside = false
                    __m128 ge = _mm_cmpge_ps(r, pd_neg[p]);
                    inside = _mm_add_ps(ge, inside);
                }
                
                _mm_store_ps(result, inside);
                for(u32 j = 0; j < 4; ++j)
                    if(!inside[j])
                        sb_push(*entities_out, e[3-j]);
            }
            
            f64 us = timer_elapsed_us(ct);
            PEN_LOG("aabb simd 128 cull time %f (us)", us);
        }
        
        void frustum_cull_sphere_simd128(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out)
        {
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
                        sb_push(*entities_out, e[3-j]);
            }
            
            f32 us = timer_elapsed_us(ct);
            PEN_LOG("sphere simd 128 cull time %f (us)", us);
        }
#endif
        
        //
        // avx 256 implementation
        //
#ifdef __AVX__
        void frustum_cull_sphere_simd256(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out)
        {

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
                    
                    __m256 diff = _mm256_sub_ps(dd, radius);
                    diff = _mm256_max_ps(diff, zero);
                    inside = _mm256_add_ps(diff, inside);
                }
                     
                _mm256_store_ps(result, inside);
                for(u32 j = 0; j < 8; ++j)
                    if(!inside[j])
                        sb_push(*entities_out, e[7-j]);
            }
            
            f64 us = timer_elapsed_us(ct);
            PEN_LOG("sphere simd 256 cull time %f (us)", us);
        }
        
        void frustum_cull_aabb_simd256(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out)
        {
            static timer* ct = timer_create();
            timer_start(ct);
            
            const frustum& frust = cam->camera_frustum;
            
            // splat constants
            __m256 zero = _mm256_set1_ps(0.0f);
            
            // sphere radius and position
            __m256 posx;
            __m256 posy;
            __m256 posz;
            __m256 extx;
            __m256 exty;
            __m256 extz;
            
            // plane normal
            __m256 pnx[6];
            __m256 pny[6];
            __m256 pnz[6];
            
            // sign flip
            __m256 sfx[6];
            __m256 sfy[6];
            __m256 sfz[6];
            
            // plane distance
            __m256 pd[6];
            __m256 pd_neg[6];
            
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
                pd_neg[p] = _mm256_set1_ps(-ppd);
                
                sfx[p] = _mm256_set1_ps(sgn(frust.n[p].x));
                sfy[p] = _mm256_set1_ps(sgn(frust.n[p].y));
                sfz[p] = _mm256_set1_ps(sgn(frust.n[p].z));
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
                auto& e0 = scene->pos_extent[e[0]].extent;
                auto& e1 = scene->pos_extent[e[1]].extent;
                auto& e2 = scene->pos_extent[e[2]].extent;
                auto& e3 = scene->pos_extent[e[3]].extent;
                auto& e4 = scene->pos_extent[e[4]].extent;
                auto& e5 = scene->pos_extent[e[5]].extent;
                auto& e6 = scene->pos_extent[e[6]].extent;
                auto& e7 = scene->pos_extent[e[7]].extent;
                                        
                posx = _mm256_set_ps(p0.x, p1.x, p2.x, p3.x, p4.x, p5.x, p6.x, p7.x);
                posy = _mm256_set_ps(p0.y, p1.y, p2.y, p3.y, p4.y, p5.y, p6.y, p7.y);
                posz = _mm256_set_ps(p0.z, p1.z, p2.z, p3.z, p4.z, p5.z, p6.z, p7.z);
                extx = _mm256_set_ps(e0.x, e1.x, e2.x, e3.x, e4.x, e5.x, e6.x, e7.x);
                exty = _mm256_set_ps(e0.y, e1.y, e2.y, e3.y, e4.y, e5.y, e6.y, e7.y);
                extz = _mm256_set_ps(e0.z, e1.z, e2.z, e3.z, e4.z, e5.z, e6.z, e7.z);
                
                __m256 inside = _mm256_set1_ps(0.0f);
                
                for (s32 p = 0; p < 6; ++p)
                {
                    // get distance to plane
                    // dot product with plane normal and also add plane distance
                    __m256 dd = _mm256_fmadd_ps(posx, pnx[p], pd[p]);
                    dd = _mm256_fmadd_ps(posy, pny[p], dd);
                    dd = _mm256_fmadd_ps(posz, pnz[p], dd);
                    
                    // pos + extent * sign_flip
                    __m256 dpx = _mm256_fmadd_ps(extx, sfx[p], posx);
                    __m256 dpy = _mm256_fmadd_ps(exty, sfy[p], posy);
                    __m256 dpz = _mm256_fmadd_ps(extz, sfz[p], posz);
                    
                    // dot(pos + extent * sign_flip, frust.n[p]);
                    __m256 r = _mm256_mul_ps(dpx, pnx[p]);
                    r = _mm256_fmadd_ps(dpy, pny[p], r);
                    r = _mm256_fmadd_ps(dpz, pnz[p], r);
                    
                    // if(r > -pd) inside = false
                    __m256 diff = _mm256_sub_ps(r, pd_neg[p]);
                    diff = _mm256_max_ps(diff, zero);
                    inside = _mm256_add_ps(diff, inside);
                }
                     
                _mm256_store_ps(result, inside);
                for(u32 j = 0; j < 8; ++j)
                    if(!inside[j])
                        sb_push(*entities_out, e[7-j]);
            }
            
            f64 us = timer_elapsed_us(ct);
            PEN_LOG("sphere simd 256 cull time %f (us)", us);
        }
#endif

#ifdef __ARM_NEON__
    void frustum_cull_aabb_simd128(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out)
    {
    
    }
    
    void frustum_cull_aabb_simd256(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out)
    {
    
    }
    
    void frustum_cull_sphere_simd128(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out)
    {
    
    }
    
    void frustum_cull_sphere_simd256(const ecs_scene* scene, const camera* cam, const u32* entities_in, u32** entities_out)
    {
    
    }
#endif
    }
}
