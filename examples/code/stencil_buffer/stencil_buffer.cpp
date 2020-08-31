#include "../example_common.h"

#include "../../shader_structs/forward_render.h"

using namespace put;
using namespace put::ecs;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "stencil_buffer";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    pmfx::init("data/configs/stencil_buffer.jsn");

    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));

    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));

    // add light
    u32 light = get_new_entity(scene);
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::one();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = e_light_type::dir;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    // cube
    u32 cube_entity = get_new_entity(scene);
    scene->names[cube_entity] = "cube";
    scene->transforms[cube_entity].translation = vec3f(0.0f, 11.0f, 0.0f);
    scene->transforms[cube_entity].rotation = quat();
    scene->transforms[cube_entity].scale = vec3f(10.0f, 10.0f, 10.0f);
    scene->entities[cube_entity] |= e_cmp::transform;
    scene->parents[cube_entity] = cube_entity;
    instantiate_geometry(box, scene, cube_entity);
    instantiate_material(default_material, scene, cube_entity);
    instantiate_model_cbuffer(scene, cube_entity);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
