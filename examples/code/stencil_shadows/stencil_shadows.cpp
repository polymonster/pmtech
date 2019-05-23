#include "../example_common.h"
#include "../../shader_structs/forward_render.h"

using namespace put;
using namespace put::ecs;

pen::window_creation_params pen_window{
    1280,               // width
    720,                // height
    4,                  // MSAA samples
    "stencil_shadows"   // window title / process name
};

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    pmfx::init("data/configs/stencil_shadows.jsn");
    
    clear_scene(scene);
    
    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    
    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));
    
    // add light
    u32 light = get_new_entity(scene);
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = LIGHT_TYPE_DIR;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;
    
    // ground
    u32 ground = get_new_entity(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].translation = vec3f::zero();
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(50.0f, 1.0f, 50.0f);
    scene->entities[ground] |= CMP_TRANSFORM;
    scene->parents[ground] = ground;
    instantiate_geometry(box, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);
    
    // cube
    u32 cube = get_new_entity(scene);
    scene->names[cube] = "ground";
    scene->transforms[cube].translation = vec3f(0.0f, 11.0f, 0.0f);
    scene->transforms[cube].rotation = quat();
    scene->transforms[cube].scale = vec3f(10.0f, 10.0f, 10.0f);
    scene->entities[cube] |= CMP_TRANSFORM;
    scene->parents[cube] = cube;
    instantiate_geometry(box, scene, cube);
    instantiate_material(default_material, scene, cube);
    instantiate_model_cbuffer(scene, cube);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    
}
