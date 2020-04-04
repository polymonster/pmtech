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
    
    void xy2d_morton(u64 x, u64 y, u64 *d)
    {
        x = (x | (x << 16)) & 0x0000FFFF0000FFFF;
        x = (x | (x << 8)) & 0x00FF00FF00FF00FF;
        x = (x | (x << 4)) & 0x0F0F0F0F0F0F0F0F;
        x = (x | (x << 2)) & 0x3333333333333333;
        x = (x | (x << 1)) & 0x5555555555555555;

        y = (y | (y << 16)) & 0x0000FFFF0000FFFF;
        y = (y | (y << 8)) & 0x00FF00FF00FF00FF;
        y = (y | (y << 4)) & 0x0F0F0F0F0F0F0F0F;
        y = (y | (y << 2)) & 0x3333333333333333;
        y = (y | (y << 1)) & 0x5555555555555555;

        *d = x | (y << 1);
    }

    // morton_1 - extract even bits

    u32 morton_1(u64 x)
    {
        x = x & 0x5555555555555555;
        x = (x | (x >> 1))  & 0x3333333333333333;
        x = (x | (x >> 2))  & 0x0F0F0F0F0F0F0F0F;
        x = (x | (x >> 4))  & 0x00FF00FF00FF00FF;
        x = (x | (x >> 8))  & 0x0000FFFF0000FFFF;
        x = (x | (x >> 16)) & 0x00000000FFFFFFFF;
        return (uint32_t)x;
    }

    void d2xy_morton(u64 d, u64 &x, u64 &y)
    {
        x = morton_1(d);
        y = morton_1(d >> 1);
    }
}

struct live_lib : public __ecs, public __dbg
{
    ecs_scene*          scene;
    u32                 box_start;
    u32                 box_end;
    u32                 quadrant_size = 0;
    
    u32 box_count = 0;
    
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
        scene->lights[light].colour = vec3f(0.3f, 0.5f, 0.8f) * 0.75f;
        scene->lights[light].direction = normalised(vec3f(-0.7f, 0.6f, -0.4f));
        scene->lights[light].type = e_light_type::dir;
        scene->lights[light].flags |= e_light_flags::shadow_map;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        light = get_new_entity(scene);
        instantiate_light(scene, light);
        scene->names[light] = "front_light";
        scene->id_name[light] = PEN_HASH("front_light");
        scene->lights[light].colour = vec3f(0.3f, 0.4f, 0.8f) * 0.5f;
        scene->lights[light].direction = normalised(vec3f(-0.5f, 0.4f, -1.0f));
        scene->lights[light].type = e_light_type::dir;
        scene->lights[light].flags |= e_light_flags::shadow_map;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        light = get_new_entity(scene);
        instantiate_light(scene, light);
        scene->names[light] = "front_light";
        scene->id_name[light] = PEN_HASH("front_light");
        scene->lights[light].colour = vec3f(0.8f, 0.5f, 0.5f) * 0.75f;
        scene->lights[light].direction = normalised(vec3f(-0.5f, 0.5f, -0.2f));
        scene->lights[light].type = e_light_type::dir;
        scene->lights[light].flags |= e_light_flags::shadow_map;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        vec3f quadrant[] = {
            vec3f::zero(),
            vec3f(-1.0f, 0.0f, 0.0f),
            vec3f(-1.0f, 0.0f, -1.0f),
            vec3f(0.0f, 0.0f, -1.0f),
            
            vec3f(-2.0f, 0.0f, 0.0f),
            vec3f(-2.0f, 0.0f, -1.0f),
            
            vec3f(-0.0f, 0.0f, -2.0f),
            vec3f(-1.0f, 0.0f, -2.0f),
            vec3f(-0.0f, 0.0f, 1.0f),
            vec3f(-1.0f, 0.0f, 1.0f),
            
            vec3f(1.0f, 0.0f, 0.0f),
            vec3f(1.0f, 0.0f, -1.0f),
        };
        
        for(u32 q = 0; q < PEN_ARRAY_SIZE(quadrant); ++q)
        {
            vec3f* p = nullptr;
            vec3f* cube_size = nullptr;
                            
            u32 num_orders = 5;
            f32 order_size = 2.0f;
            f32 order_num = (f32)((1<<num_orders)+1);
            f32 fpad = 0.12f;
            
            vec3f qq = quadrant[q] * ((order_num-1)*order_size);
            
            for(u32 o = 0; o < num_orders; ++o)
            {
                vec3f vstart = (order_size * 0.5f);
                vstart.y = 0.0f;
                vstart += qq;
            
                for(u32 j = 0; j < 2; ++j)
                {
                    for(u32 i = 0; i < 2; ++i)
                    {
                        if(o > 0 && i == 0 && j == 0)
                            continue;
                            
                        vec3f pos = vstart + vec3f(i * order_size, 0.0f, j * order_size);
                        f32 size = (order_size-fpad)*0.5f;
                        
                        sb_push(p, pos);
                        sb_push(cube_size, vec3f(size, 1.0f, size));
                    }
                }
                order_size *= 2.0;
                order_num /= 2.0;
            }
            
            u32 c = sb_count(p);
            for(u32 i = 0; i < c; ++i)
            {
                u32 box = get_new_entity(scene);
                scene->names[box] = "box";
                scene->id_name[box] = PEN_HASH("box");
                scene->transforms[box].rotation = quat();
                scene->transforms[box].scale = cube_size[i];
                scene->transforms[box].translation = p[i];
                scene->parents[box] = box;
                scene->entities[box] |= e_cmp::transform;
                instantiate_geometry(box_resource, scene, box);
                instantiate_material(default_material, scene, box);
                instantiate_model_cbuffer(scene, box);
                
                vec3f hsv = vec3f((f32)i/(f32)c/0.9f, 1.0f, 0.5f);
                        
                forward_render::forward_lit* mat = (forward_render::forward_lit*)&scene->material_data[box].data[0];
                mat->m_albedo = vec4f(hsv, 1.0f);
                
                if(i == 0 && q == 0)
                    box_start = box;
            }
            box_end = box_start + c*(q+1);
            sb_free(p);
            sb_free(cube_size);
            quadrant_size = c;
        }
            
        return 0;
    }
    
    int on_update(f32 dt)
    {
        static f32 t = 0;
        static u32 mode = 0;
        
        f32 rise = 4.0f;
                    
        // rise up
        for(u32 i = box_start; i < box_end; ++i)
        {
            u32 ii = ((i-box_start)%quadrant_size);
            
            f32 fx = 1.0f - ((f32)ii/quadrant_size);
            fx *= rise;
            
            f32 l = min(max((f32)ii - t, 0.0f), 1.0f);
            scene->transforms[i].translation.y = -(sin(l) * rise);
            scene->entities[i] |= e_cmp::transform;
        }
        
        if(mode == 0)
        {
            t += dt * 4.0f;
        }
        else if(mode == 1)
        {
            t -= dt * 9.0f;
        }
        
        if(t > (f32)(box_end/12))
        {
            mode = 1;
        }
        
        if(t < -2)
        {
            mode = 0;
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


