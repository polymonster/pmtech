#include "../example_common.h"
#include "shader_structs/forward_render.h"
#include "maths/vec.h"

using namespace put;
using namespace ecs;
using namespace forward_render;

void* pen::user_entry(void* params);
namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "dynamic_cubemap";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    f32 zpos[] = {3.0f, -3.0f};

    s32 chorme_ball = 0;
    s32 glass_ball = 0;

    put::camera chrome_camera;
    put::camera chrome2_camera;
    
    u32 orbit_balls[4];

} // namespace

void render_sky(const scene_view& view)
{
    pmfx::fullscreen_quad(view);
}

void example_setup(ecs_scene* scene, camera& cam)
{
    // sly renderer
    put::scene_view_renderer svr_render_sky;
    svr_render_sky.name = "ecs_render_sky";
    svr_render_sky.id_name = PEN_HASH(svr_render_sky.name.c_str());
    svr_render_sky.render_function = &render_sky;
    pmfx::register_scene_view_renderer(svr_render_sky);
    
    // chrome cameras
    put::camera_create_cubemap(&chrome_camera, 0.01f, 1000.0f);
    chrome_camera.pos = vec3f(0.0f, 0.0f, 0.0f);

    put::camera_create_cubemap(&chrome2_camera, 0.01f, 1000.0f);
    chrome2_camera.pos = vec3f(0.0f, 0.0f, 0.0f);

    pmfx::register_camera(&chrome_camera, "chrome_camera");
    pmfx::register_camera(&chrome2_camera, "chrome2_camera");

    pmfx::init("data/configs/dynamic_cubemap.jsn");
    
    u32 wrap_linear = pmfx::get_render_state(PEN_HASH("wrap_linear"), pmfx::e_render_state::sampler);

    // setup scene
    clear_scene(scene);
    
    material_resource* simple_light_material = new material_resource;
    simple_light_material->id_shader = PEN_HASH("forward_render");
    simple_light_material->id_technique = PEN_HASH("simple_lighting");
    simple_light_material->material_name = "simple_lighting";
    simple_light_material->shader_name = "forward_render";
    simple_light_material->hash = PEN_HASH("simple_lighting");
    
    static const u32 default_maps[] = {
        put::load_texture("data/textures/defaults/albedo.dds"),
        put::load_texture("data/textures/defaults/normal.dds"),
        put::load_texture("data/textures/defaults/spec.dds"),
        put::load_texture("data/textures/defaults/black.dds")};

    for (s32 i = 0; i < 4; ++i)
    {
        simple_light_material->texture_handles[i] = default_maps[i];
    }
        
    add_material_resource(simple_light_material);

    material_resource* default_material = get_material_resource(PEN_HASH("simple_lighting"));
    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));
    geometry_resource* sphere = get_geometry_resource(PEN_HASH("sphere"));
    geometry_resource* capsule = get_geometry_resource(PEN_HASH("capsule"));

    // front light
    u32 light = get_new_entity(scene);
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = e_light_type::dir;
    scene->lights[light].flags = e_light_flags::shadow_map;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    // back light
    light = get_new_entity(scene);
    scene->names[light] = "back_light";
    scene->id_name[light] = PEN_HASH("back_light");
    scene->lights[light].colour = vec3f(0.6f, 0.6f, 0.6f);
    scene->lights[light].direction = -vec3f::one();
    scene->lights[light].type = e_light_type::dir;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    // ground tiles
    vec3f start = vec3f(-9.0f * 7.0f * 0.5f, -4.0f, -9.0f * 7.0f * 0.5f);
    vec3f pos = start;
    for(u32 i = 0; i < 8; ++i)
    {
        pos.x = start.x;
        
        for(u32 i = 0; i < 8; ++i)
        {
            u32 ground = get_new_entity(scene);
            scene->names[ground] = "ground";
            scene->transforms[ground].translation = pos;
            scene->transforms[ground].rotation = quat();
            scene->transforms[ground].scale = vec3f(4.0f, ((rand()%255) / 255.0f) * 2.0f + 2.0f, 4.0f);
            scene->entities[ground] |= e_cmp::transform;
            scene->parents[ground] = ground;
            
            for(u32 i = 0; i < 4; ++i)
            {
                scene->samplers[ground].sb[i].sampler_unit = i;
                scene->samplers[ground].sb[i].sampler_state = wrap_linear;
            }
            
            instantiate_geometry(box, scene, ground);
            instantiate_material(default_material, scene, ground);
            instantiate_model_cbuffer(scene, ground);
            
            vec3f hsv = vec3f(lerp(0.1f, 0.4f, (f32)(pos.x+pos.y*8.0f)/(8.0f*8.0f*2.0f)) + 0.9f, 1.0f, 0.5f);
                                
            simple_lighting* m = (simple_lighting*)&scene->material_data[ground].data[0];
            m->m_albedo = vec4f(maths::hsv_to_rgb(hsv), 1.0f);
            m->m_roughness = 0.09f;
            m->m_reflectivity = 0.3f;

            pos.x += 9.0f;
        }
        
        pos.z += 9.0f;
    }
    
    for(u32 i = 0; i < 4; ++i)
    {
        u32 orbit = get_new_entity(scene);
        scene->names[orbit] = "orbit";
        scene->transforms[orbit].translation = pos;
        scene->transforms[orbit].rotation = quat();
        scene->transforms[orbit].scale = vec3f(0.5f);
        scene->entities[orbit] |= e_cmp::transform;
        scene->parents[orbit] = orbit;
        
        for(u32 i = 0; i < 4; ++i)
        {
            scene->samplers[orbit].sb[i].sampler_unit = i;
            scene->samplers[orbit].sb[i].sampler_state = wrap_linear;
        }
        
        instantiate_geometry(sphere, scene, orbit);
        instantiate_material(default_material, scene, orbit);
        instantiate_model_cbuffer(scene, orbit);
        
        vec3f hsv = vec3f((f32)(rand()%RAND_MAX)/(f32)RAND_MAX, 1.0f, 1.0f);
                            
        simple_lighting* m = (simple_lighting*)&scene->material_data[orbit].data[0];
        m->m_albedo = vec4f(maths::hsv_to_rgb(hsv), 1.0f);
        m->m_roughness = 0.09f;
        m->m_reflectivity = 0.3f;
        
        orbit_balls[i] = orbit;
    }
    
    u32 pillar = get_new_entity(scene);
    scene->names[pillar] = "pillar";
    scene->transforms[pillar].translation = vec3f(0.0f, 10.0f, 0.0f);
    scene->transforms[pillar].rotation = quat();
    scene->transforms[pillar].scale = vec3f(3.0f, 3.0f, 3.0f);
    scene->entities[pillar] |= e_cmp::transform;
    scene->parents[pillar] = pillar;
    
    for(u32 i = 0; i < 4; ++i)
    {
        scene->samplers[pillar].sb[i].sampler_unit = i;
        scene->samplers[pillar].sb[i].sampler_state = wrap_linear;
    }
    
    instantiate_geometry(capsule, scene, pillar);
    instantiate_material(default_material, scene, pillar);
    instantiate_model_cbuffer(scene, pillar);
    
    simple_lighting* m = (simple_lighting*)&scene->material_data[pillar].data[0];
    m->m_albedo = vec4f(1.0f, 0.75f, 0.1f, 1.0f);
    m->m_roughness = 0.09f;
    m->m_reflectivity = 0.3f;
    
    // chrome ball
    chorme_ball = get_new_entity(scene);
    scene->names[chorme_ball] = "chrome_ball";
    scene->transforms[chorme_ball].rotation = quat();
    scene->transforms[chorme_ball].scale = vec3f(5.0f, 5.0f, 5.0f);
    scene->transforms[chorme_ball].translation = vec3f(10.0f, 5.0f, zpos[0]);
    scene->entities[chorme_ball] |= e_cmp::transform;
    scene->parents[chorme_ball] = chorme_ball;

    instantiate_geometry(sphere, scene, chorme_ball);
    instantiate_model_cbuffer(scene, chorme_ball);
    instantiate_material(default_material, scene, chorme_ball);

    u32 chrome_cubemap_handle = pmfx::get_render_target(PEN_HASH("chrome"))->handle;

    // set material to cubemap
    scene->material_resources[chorme_ball].id_technique = PEN_HASH("cubemap");
    scene->material_resources[chorme_ball].shader_name = "pmfx_utility";
    scene->material_resources[chorme_ball].id_shader = PEN_HASH("pmfx_utility");

    scene->state_flags[chorme_ball] &= ~e_state::samplers_initialised;
    bake_material_handles(scene, chorme_ball);

    for (u32 s = 1; s < e_pmfx_constants::max_technique_sampler_bindings; ++s)
        scene->samplers[chorme_ball].sb[s].handle = 0;

    scene->samplers[chorme_ball].sb[0].handle = chrome_cubemap_handle;
    scene->samplers[chorme_ball].sb[0].sampler_unit = 3;
    scene->samplers[chorme_ball].sb[0].sampler_state =
        pmfx::get_render_state(PEN_HASH("clamp_linear"), pmfx::e_render_state::sampler);

    // glass ball
    glass_ball = get_new_entity(scene);
    scene->names[glass_ball] = "chrome2_ball";
    scene->transforms[glass_ball].rotation = quat();
    scene->transforms[glass_ball].scale = vec3f(5.0f, 5.0f, 5.0f);
    scene->transforms[glass_ball].translation = vec3f(-10.0f, 5.0f, zpos[1]);
    scene->entities[glass_ball] |= e_cmp::transform;
    scene->parents[glass_ball] = glass_ball;

    instantiate_geometry(sphere, scene, glass_ball);
    instantiate_model_cbuffer(scene, glass_ball);
    instantiate_material(default_material, scene, glass_ball);

    u32 chrome2_cubemap_handle = pmfx::get_render_target(PEN_HASH("chrome2"))->handle;

    // set material to cubemap
    scene->material_resources[glass_ball].id_technique = PEN_HASH("cubemap");
    scene->material_resources[glass_ball].shader_name = "pmfx_utility";
    scene->material_resources[glass_ball].id_shader = PEN_HASH("pmfx_utility");
    scene->state_flags[glass_ball] &= ~e_state::samplers_initialised;
    
    bake_material_handles(scene, glass_ball);

    scene->samplers[glass_ball].sb[0].handle = chrome2_cubemap_handle;
    scene->samplers[glass_ball].sb[0].sampler_unit = 3;
    scene->samplers[glass_ball].sb[0].sampler_state =
        pmfx::get_render_state(PEN_HASH("clamp_linear"), pmfx::e_render_state::sampler);

    for (u32 s = 1; s < e_pmfx_constants::max_technique_sampler_bindings; ++s)
        scene->samplers[glass_ball].sb[s].handle = 0;
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    chrome_camera.pos = scene->world_matrices[chorme_ball].get_translation();
    chrome2_camera.pos = scene->world_matrices[glass_ball].get_translation();
    
    static f32 acc = 0.0f;
    acc += dt;
    
    f32 rx = sin(acc);
    f32 ry = cos(acc);
    scene->transforms[chorme_ball].translation = vec3f(rx, 0.0f, ry) * 15.0f + vec3f(0.0f, 8.0f + sin(acc*2.0f) * 3.0f, 0.0f);
    scene->entities[chorme_ball] |= e_cmp::transform;
    
    rx = sin(acc+M_PI);
    ry = cos(acc+M_PI);
    scene->transforms[glass_ball].translation = vec3f(rx, 0.0f, ry) * 15.0f + vec3f(0.0f, 8.0f + cos(acc*2.0f) * 3.0f, 0.0f);
    scene->entities[glass_ball] |= e_cmp::transform;
    
    for(u32 i = 0; i < 2; ++i)
    {
        u32 b = orbit_balls[i];
        
        f32 rx = sin(acc+M_PI*i);
        f32 ry = cos(acc+M_PI*i);
        vec3f sp = scene->transforms[chorme_ball].translation;
        
        scene->transforms[b].translation = sp + vec3f(rx, ry, ry) * 6.0f;
        scene->entities[b] |= e_cmp::transform;
    }
    
    for(u32 i = 0; i < 2; ++i)
    {
        u32 b = orbit_balls[i] + 2;
        
        f32 rx = sin(acc+M_PI*i);
        f32 ry = cos(acc+M_PI*i);
        vec3f sp = scene->transforms[glass_ball].translation;
        
        scene->transforms[b].translation = sp + vec3f(rx, ry, ry) * 6.0f;
        scene->entities[b] |= e_cmp::transform;
    }
}
