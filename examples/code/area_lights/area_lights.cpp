#include "../example_common.h"

using namespace put;
using namespace put::ecs;

pen::window_creation_params pen_window{
    1280,        // width
    720,         // height
    4,           // MSAA samples
    "area_lights" // window title / process name
};

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    // add model
    u32 model = load_pmm("data/models/lucy.pmm", scene, PMM_ALL);
    scene->transforms[model].scale = vec3f(0.05f);
    scene->entities[model] |= CMP_TRANSFORM;
    
    // sdf
    
    // ground
    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* quad = get_geometry_resource(PEN_HASH("quad"));
    
    u32 ground = get_new_entity(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(50.0f);
    scene->transforms[ground].translation = vec3f::zero();
    scene->entities[ground] |= CMP_TRANSFORM;
    scene->parents[ground] = ground;
    
    instantiate_geometry(quad, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
