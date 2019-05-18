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
    
    geometry_resource* sphere_res = get_geometry_resource(PEN_HASH("sphere"));
    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* quad = get_geometry_resource(PEN_HASH("quad"));
    
    // add sphere
    u32 sphere = get_new_entity(scene);
    scene->names[sphere] = "sphere";
    scene->transforms[sphere].rotation = quat();
    scene->transforms[sphere].scale = vec3f(4.0f);
    scene->transforms[sphere].translation = vec3f(0.0f, 2.0f, 0.0f);
    scene->entities[sphere] |= CMP_TRANSFORM;
    scene->parents[sphere] = sphere;
    
    instantiate_geometry(sphere_res, scene, sphere);
    instantiate_material(default_material, scene, sphere);
    instantiate_model_cbuffer(scene, sphere);
    
    /*
    u32 model = load_pmm("data/models/lucy.pmm", scene, PMM_ALL);
    scene->transforms[model].scale = vec3f(0.07f);
    scene->transforms[model].rotation = quat(0.0f, -M_PI/4.0f, 0.0f);
    scene->transforms[model].translation = vec3f(0.0f, -0.35f, 0.0f);
    scene->entities[model] |= CMP_TRANSFORM;
     */
    
    // ground
    
    u32 ground = get_new_entity(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(60.0f);
    scene->transforms[ground].translation = vec3f::zero();
    scene->entities[ground] |= CMP_TRANSFORM;
    scene->parents[ground] = ground;
    scene->material_permutation[ground] |= FORWARD_LIT_UV_SCALE;
    
    instantiate_geometry(quad, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);
    
    // checkerboard roughness
    scene->samplers[ground].sb[2].handle = put::load_texture("data/textures/roughness_checker.dds");
    forward_lit_uv_scale* mat = (forward_lit_uv_scale*)&scene->material_data[ground];
    mat->m_uv_scale = float2(0.1, 0.1);
    
    // area lights
    material_resource area_light_material;
    area_light_material.id_shader = PEN_HASH("pmfx_utility");
    area_light_material.id_technique = PEN_HASH("area_light_texture");
    area_light_material.material_name = "area_light_texture";
    area_light_material.shader_name = "pmfx_utility";
    
    quat al_rots[] = {
        quat(0.0f, 0.0f, M_PI/2.0f) * quat(-M_PI/4.0f, 0.0f, 0.0f),
        quat(0.0f, 0.0f, M_PI/2.0f),
        quat(0.0f, 0.0f, M_PI/2.0f) * quat( M_PI/4.0f, 0.0f, 0.0f),
    };
    
    vec3f al_scales[] = {
        vec3f(10.0f, 1.0f, 10.0f),
        vec3f(10.0f, 1.0f, 10.0f),
        vec3f(10.0f, 1.0f, 10.0f)
    };
    
    vec3f al_trans[] = {
        vec3f(-25.0f, 20.0f, -20.0f),
        vec3f(0.0f, 20.0f, -30.0f),
        vec3f(25.0f, 20.0f, -20.0f)
    };
    
    area_light_resource alrs[] = {
        {"trace", "box", "", ""},
        {"trace", "torus", "", ""},
        {"trace", "octahedron", "", ""},
    };
    
    for(u32 i = 0; i < 3; ++i)
    {
        u32 al = get_new_entity(scene);
        scene->names[al].setf("area_light_%i", i);
        
        scene->transforms[al].rotation = al_rots[i];
        scene->transforms[al].scale = al_scales[i];
        scene->transforms[al].translation = al_trans[i];
        scene->entities[al] |= CMP_TRANSFORM;
        scene->parents[al] = al;
        
        instantiate_area_light_ex(scene, al, alrs[i]);
    }

    bake_material_handles();
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
