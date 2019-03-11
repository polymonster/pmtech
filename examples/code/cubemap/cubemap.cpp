#include "../example_common.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,     // width
    720,      // height
    4,        // MSAA samples
    "cubemap" // window title / process name
};

void example_setup(ecs::ecs_scene* scene)
{
    clear_scene(scene);

    // create material for cubemap
    material_resource* cubemap_material = new material_resource;
    cubemap_material->material_name = "volume_material";
    cubemap_material->shader_name = "pmfx_utility";
    cubemap_material->id_shader = PEN_HASH("pmfx_utility");
    cubemap_material->id_technique = PEN_HASH("cubemap");
    add_material_resource(cubemap_material);

    geometry_resource* sphere = get_geometry_resource(PEN_HASH("sphere"));

    u32 new_prim = get_new_node(scene);
    scene->names[new_prim] = "sphere";
    scene->names[new_prim].appendf("%i", new_prim);
    scene->transforms[new_prim].rotation = quat();
    scene->transforms[new_prim].scale = vec3f(10.0f);
    scene->transforms[new_prim].translation = vec3f::zero();
    scene->entities[new_prim] |= CMP_TRANSFORM;
    scene->parents[new_prim] = new_prim;
    instantiate_geometry(sphere, scene, new_prim);
    instantiate_material(cubemap_material, scene, new_prim);
    instantiate_model_cbuffer(scene, new_prim);

    scene->samplers[new_prim].sb[0].handle = put::load_texture("data/textures/cubemap.dds");
    scene->samplers[new_prim].sb[0].sampler_unit = 3;
    scene->samplers[new_prim].sb[0].sampler_state = pmfx::get_render_state(PEN_HASH("clamp_linear"), pmfx::RS_SAMPLER);
}

void example_update(ecs::ecs_scene* scene)
{
}