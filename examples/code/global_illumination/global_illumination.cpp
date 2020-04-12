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

    geometry_resource* cube = get_geometry_resource(PEN_HASH("cube"));
    
    // create material for volume ray trace
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

    instantiate_geometry(cube, scene, new_prim);
    instantiate_material(volume_material, scene, new_prim);
    instantiate_model_cbuffer(scene, new_prim);

    bake_material_handles();
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{

}
