#include "../example_common.h"

#include "shader_structs/forward_render.h"

using namespace put;
using namespace ecs;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "geometry_primitives";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    u32 prim_start = 0;
    u32 num_prims = 0;
}

void example_setup(ecs_scene* scene, camera& cam)
{
    pmfx::init("data/configs/geometry_primitives.jsn");
    
    ecs::clear_scene(scene);
            
    // primitive resources
    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));

    const c8* primitive_names[] = {
        "tetrahedron",
        "cube",
        "octahedron",
        "dodecahedron",
        "icosahedron",
        "sphere",
        "cone",
        "capsule",
        "cylinder",
        "torus"
    };
    
    geometry_resource** primitives = nullptr;
    
    for(u32 i = 0; i < PEN_ARRAY_SIZE(primitive_names); ++i)
    {
        sb_push(primitives, get_geometry_resource(PEN_HASH(primitive_names[i])));
    }
    
    // add lights
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
    
    // add primitve instances
    vec3f pos[] = {
        vec3f::unit_x() * - 6.0f,
        vec3f::unit_x() * - 3.0f,
        vec3f::unit_x() * 0.0f,
        vec3f::unit_x() * 3.0f,
        vec3f::unit_x() * 6.0f,
        
        vec3f::unit_x() * - 6.0f + vec3f::unit_y() * 3.0f,
        vec3f::unit_x() * - 3.0f + vec3f::unit_y() * 3.0f,
        vec3f::unit_x() * 0.0f + vec3f::unit_y() * 3.0f,
        vec3f::unit_x() * 3.0f + vec3f::unit_y() * 3.0f,
        vec3f::unit_x() * 6.0f + vec3f::unit_y() * 3.0f,
    };
    
    vec4f col[] = {
        vec4f::orange(),
        vec4f::yellow(),
        vec4f::green(),
        vec4f::cyan(),
        vec4f::magenta(),
        vec4f::white(),
        vec4f::red(),
        vec4f::blue(),
        vec4f::magenta(),
        vec4f::cyan()
    };

    for (s32 p = 0; p < PEN_ARRAY_SIZE(primitive_names); ++p)
    {
        u32 new_prim = get_new_entity(scene);
        scene->names[new_prim] = primitive_names[p];
        scene->names[new_prim].appendf("%i", new_prim);
        scene->transforms[new_prim].rotation = quat();
        scene->transforms[new_prim].scale = vec3f::one();
        scene->transforms[new_prim].translation = pos[p] + vec3f::unit_y() * 2.0f;
        scene->entities[new_prim] |= e_cmp::transform;
        scene->parents[new_prim] = new_prim;
        instantiate_geometry(primitives[p], scene, new_prim);
        instantiate_material(default_material, scene, new_prim);
        instantiate_model_cbuffer(scene, new_prim);
        
        if(p == 0)
            prim_start = new_prim;
        
        forward_render::forward_lit* mat = (forward_render::forward_lit*)&scene->material_data[new_prim].data[0];
        mat->m_albedo = col[p];
    }
     
    num_prims = PEN_ARRAY_SIZE(primitive_names);
    
    cam.focus = vec3f(0.0, 3.0, 0.0);
    cam.zoom = 13.0f;
    cam.flags |= e_camera_flags::invalidated;
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    static f32 r = 0.0f;
    r += dt;
    
    for(u32 p = prim_start; p < prim_start + num_prims; ++p)
    {
        f32 pr = r + (M_PI / 2.0) * p;
        scene->entities[p] |= e_cmp::transform;
        scene->transforms[p].rotation = quat(pr, pr, pr);
    }
}
