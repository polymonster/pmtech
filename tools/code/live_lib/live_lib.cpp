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
        scene->lights[light].colour = vec3f(0.8f, 0.8f, 0.8f);
        scene->lights[light].direction = normalised(vec3f(-0.7f, 0.6f, -0.4f));
        scene->lights[light].type = e_light_type::dir;
        scene->lights[light].flags |= e_light_flags::global_illumination;
        scene->transforms[light].translation = vec3f::zero();
        scene->transforms[light].rotation = quat();
        scene->transforms[light].scale = vec3f::one();
        scene->entities[light] |= e_cmp::light;
        scene->entities[light] |= e_cmp::transform;
        
        vec2f dim = vec2f(100.0f, 100.0f);
        u32 ground = get_new_entity(scene);
        scene->transforms[ground].rotation = quat();
        scene->transforms[ground].scale = vec3f(dim.x, 1.0f, dim.y);
        scene->transforms[ground].translation = vec3f::zero();
        scene->parents[ground] = ground;
        scene->entities[ground] |= e_cmp::transform;
        instantiate_geometry(box_resource, scene, ground);
        instantiate_material(default_material, scene, ground);
        instantiate_model_cbuffer(scene, ground);
        
        u32 wall = get_new_entity(scene);
        scene->transforms[wall].rotation = quat();
        scene->transforms[wall].scale = vec3f(1.0f, dim.x, dim.y);
        scene->transforms[wall].translation = vec3f(50.0f, 0.0f, 0.0f);
        scene->parents[wall] = wall;
        scene->entities[wall] |= e_cmp::transform;
        instantiate_geometry(box_resource, scene, wall);
        instantiate_material(default_material, scene, wall);
        instantiate_model_cbuffer(scene, wall);
        
        u32 box = get_new_entity(scene);
        scene->transforms[box].rotation = quat();
        scene->transforms[box].scale = vec3f(10.0f, 10.0f, 10.0f);
        scene->transforms[box].translation = vec3f(0.0f, 10.0f, 0.0f);
        scene->parents[box] = box;
        scene->entities[box] |= e_cmp::transform;

        instantiate_geometry(box_resource, scene, box);
        instantiate_material(default_material, scene, box);
        instantiate_model_cbuffer(scene, box);
        
    
        // create material for volume ray trace
        /*
        material_resource* volume_material = new material_resource;
        volume_material->material_name = "volume_material";
        volume_material->shader_name = "pmfx_utility";
        volume_material->id_shader = PEN_HASH("pmfx_utility");
        volume_material->id_technique = PEN_HASH("volume_texture");
        add_material_resource(volume_material);

        // create scene node
        u32 new_prim = get_new_entity(scene);
        scene->names[new_prim] = "volume_gi";
        scene->names[new_prim].appendf("%i", new_prim);
        scene->transforms[new_prim].rotation = quat();
        scene->transforms[new_prim].scale = vec3f(10.0f);
        scene->transforms[new_prim].translation = vec3f::zero();
        scene->entities[new_prim] |= e_cmp::transform;
        scene->parents[new_prim] = new_prim;
        scene->samplers[new_prim].sb[0].handle = pmfx::get_render_target(PEN_HASH("volume_gi"))->handle;
        scene->samplers[new_prim].sb[0].sampler_unit = e_texture::volume;
        scene->samplers[new_prim].sb[0].sampler_state =
            pmfx::get_render_state(PEN_HASH("clamp_point"), pmfx::e_render_state::sampler);

        instantiate_geometry(box_resource, scene, new_prim);
        instantiate_material(volume_material, scene, new_prim);
        instantiate_model_cbuffer(scene, new_prim);
        */
            
        return 0;
    }
    
    int on_update(f32 dt)
    {

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


