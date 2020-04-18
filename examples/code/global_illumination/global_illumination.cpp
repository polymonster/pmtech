#include "../example_common.h"

using namespace put;
using namespace ecs;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "global_illumination";
        p.window_sample_count = 4;
        p.user_thread_function = user_entry;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

void example_setup(ecs_scene* scene, camera& cam)
{
    pmfx::set_view_set("editor_gi");

    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));
    
    // create material for volume ray trace
    material_resource* volume_material = new material_resource;
    volume_material->material_name = "volume_material";
    volume_material->shader_name = "pmfx_utility";
    volume_material->id_shader = PEN_HASH("pmfx_utility");
    volume_material->id_technique = PEN_HASH("volume_texture");
    add_material_resource(volume_material);
    
    // create scene node for gi
    u32 new_prim = get_new_entity(scene);
    scene->names[new_prim] = "volume_gi";
    scene->names[new_prim].appendf("%i", new_prim);
    scene->transforms[new_prim].rotation = quat();
    scene->transforms[new_prim].scale = vec3f(100.0f);
    scene->transforms[new_prim].translation = vec3f(0.0f, 0.0f, 0.0f);
    scene->entities[new_prim] |= e_cmp::transform;
    scene->parents[new_prim] = new_prim;
    scene->samplers[new_prim].sb[0].handle = pmfx::get_render_target(PEN_HASH("volume_gi"))->handle;
    scene->samplers[new_prim].sb[0].sampler_unit = e_texture::volume;
    scene->samplers[new_prim].sb[0].sampler_state =
        pmfx::get_render_state(PEN_HASH("clamp_point"), pmfx::e_render_state::sampler);

    instantiate_geometry(cube, scene, new_prim);
    instantiate_material(volume_material, scene, new_prim);
    instantiate_model_cbuffer(scene, new_prim);

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
    
    vec2f dim = vec2f(100.0f, 100.0f);
    u32 ground = get_new_entity(scene);
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(dim.x, 1.0f, dim.y);
    scene->transforms[ground].translation = vec3f::zero();
    scene->parents[ground] = ground;
    scene->entities[ground] |= e_cmp::transform;
    instantiate_geometry(cube, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);
    
    u32 wall = get_new_entity(scene);
    scene->transforms[wall].rotation = quat();
    scene->transforms[wall].scale = vec3f(1.0f, dim.x, dim.y);
    scene->transforms[wall].translation = vec3f(50.0f, 0.0f, 0.0f);
    scene->parents[wall] = wall;
    scene->entities[wall] |= e_cmp::transform;
    instantiate_geometry(cube, scene, wall);
    instantiate_material(default_material, scene, wall);
    instantiate_model_cbuffer(scene, wall);
    
    u32 box = get_new_entity(scene);
    scene->transforms[box].rotation = quat();
    scene->transforms[box].scale = vec3f(10.0f, 10.0f, 10.0f);
    scene->transforms[box].translation = vec3f(0.0f, 10.0f, 0.0f);
    scene->parents[box] = box;
    scene->entities[box] |= e_cmp::transform;

    instantiate_geometry(cube, scene, box);
    instantiate_material(default_material, scene, box);
    instantiate_model_cbuffer(scene, box);

    bake_material_handles();
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{

}
