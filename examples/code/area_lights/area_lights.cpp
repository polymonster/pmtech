#include "../example_common.h"
#include "../../shader_structs/forward_render.h"

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
    clear_scene(scene);
    
    // add model
    u32 model = load_pmm("data/models/lucy.pmm", scene, PMM_ALL);
    scene->transforms[model].scale = vec3f(0.05f);
    scene->entities[model] |= CMP_TRANSFORM;
    
    // sdf
    u32 model_sdf = get_new_entity(scene);
    instantiate_sdf_shadow("data/models/lucy-volume.pmv", scene, model_sdf);
    scene->transforms[model_sdf].translation = vec3f::zero();
    scene->transforms[model_sdf].rotation = quat();
    scene->entities[model_sdf] |= CMP_TRANSFORM;
    
    // ground
    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* quad = get_geometry_resource(PEN_HASH("quad"));
    
    u32 ground = get_new_entity(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(60.0f);
    scene->transforms[ground].translation = vec3f::zero();
    scene->entities[ground] |= CMP_TRANSFORM;
    scene->parents[ground] = ground;
    
    scene->material_permutation[ground] |= FORWARD_LIT_SDF_SHADOW | FORWARD_LIT_UV_SCALE;
    
    instantiate_geometry(quad, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);
    
    // checkerboard roughness
    scene->samplers[ground].sb[2].handle = put::load_texture("data/textures/roughness_checker.dds");
    forward_lit_sdf_shadow_uv_scale* mat = (forward_lit_sdf_shadow_uv_scale*)&scene->material_data[ground];
    mat->m_uv_scale = float2(0.1, 0.1);
    
    // area lights
    u32 left = get_new_entity(scene);
    scene->names[left] = "left_light";
    scene->transforms[left].rotation = quat(-M_PI/8.0f, M_PI/4.0f, M_PI/2.0f);
    scene->transforms[left].scale = vec3f(10.0f, 1.0f, 10.0f);
    scene->transforms[left].translation = vec3f(-20.0f, 20.0f, -20.0f);
    scene->entities[left] |= CMP_TRANSFORM;
    scene->parents[left] = left;
    
    instantiate_geometry(quad, scene, left);
    instantiate_material(default_material, scene, left);
    instantiate_model_cbuffer(scene, left);
    
    scene->entities[left] |= CMP_LIGHT;
    scene->lights[left].type = LIGHT_TYPE_AREA;
    
    u32 right = get_new_entity(scene);
    scene->names[right] = "left_right";
    scene->transforms[right].rotation = quat(M_PI/8.0f, -M_PI/4.0f, M_PI/2.0f);
    scene->transforms[right].scale = vec3f(10.0f, 1.0f, 10.0f);
    scene->transforms[right].translation = vec3f(20.0f, 20.0f, -20.0f);
    scene->entities[right] |= CMP_TRANSFORM;
    scene->parents[right] = right;
    
    instantiate_geometry(quad, scene, right);
    instantiate_material(default_material, scene, right);
    instantiate_model_cbuffer(scene, right);
    
    scene->entities[right] |= CMP_LIGHT;
    scene->lights[right].type = LIGHT_TYPE_AREA;
    
    bake_material_handles();
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
