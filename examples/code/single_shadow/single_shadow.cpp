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
        p.window_title = "single_shadow";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

void example_setup(ecs_scene* scene, camera& cam)
{
    pmfx::set_view_set("editor_basic");

    scene->view_flags &= ~e_scene_view_flags::hide_debug;
    editor_set_transform_mode(e_transform_mode::physics);

    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));

    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));
    geometry_resource* cylinder = get_geometry_resource(PEN_HASH("cylinder"));
    geometry_resource* capsule = get_geometry_resource(PEN_HASH("capsule"));
    geometry_resource* sphere = get_geometry_resource(PEN_HASH("sphere"));
    geometry_resource* cone = get_geometry_resource(PEN_HASH("physics_cone"));

    // add light
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

    // ground
    u32 ground = get_new_entity(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].translation = vec3f::zero();
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(20.0f, 1.0f, 20.0f);
    scene->entities[ground] |= e_cmp::transform;
    scene->parents[ground] = ground;
    instantiate_geometry(box, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);

    scene->physics_data[ground].rigid_body.shape = physics::e_shape::box;
    scene->physics_data[ground].rigid_body.mass = 0.0f;
    instantiate_rigid_body(scene, ground);

    vec3f start_positions[] = {vec3f(-4.f, 2.0f, -4.f)};

    const c8*          primitive_names[] = {"box", "cylinder", "capsule", "cone", "sphere"};
    geometry_resource* primitive_resources[] = {box, cylinder, capsule, cone, sphere};

    s32 num_prims = 1;

    for (s32 p = 0; p < num_prims; ++p)
    {
        // add stack of cubes
        vec3f start_pos = start_positions[p];
        vec3f cur_pos = start_pos;
        for (s32 i = 0; i < 4; ++i)
        {
            cur_pos.y = start_pos.y;

            for (s32 j = 0; j < 4; ++j)
            {
                cur_pos.x = start_pos.x;

                for (s32 k = 0; k < 4; ++k)
                {
                    u32 new_prim = get_new_entity(scene);
                    scene->names[new_prim] = primitive_names[p];
                    scene->names[new_prim].appendf("%i", new_prim);
                    scene->transforms[new_prim].rotation = quat();
                    scene->transforms[new_prim].scale = vec3f::one();
                    scene->transforms[new_prim].translation = cur_pos;
                    scene->entities[new_prim] |= e_cmp::transform;
                    scene->parents[new_prim] = new_prim;
                    instantiate_geometry(primitive_resources[p], scene, new_prim);
                    instantiate_material(default_material, scene, new_prim);
                    instantiate_model_cbuffer(scene, new_prim);

                    cur_pos.x += 2.5f;
                }

                cur_pos.y += 2.5f;
            }

            cur_pos.z += 2.5f;
        }
    }

    // load physics stuff before calling update
    physics::physics_consume_command_buffer();
    pen::thread_sleep_ms(16);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    quat q;
    q.euler_angles(0.01f, 0.01f, 0.01f);

    dt *= 0.05f;

    static f32 lr[] = {M_PI};

    for (u32 i = 0; i < 1; ++i)
    {
        // animate lights
        f32 dirsign = 1.0f;
        if (i % 2 == 0)
            dirsign = -1.0f;

        lr[i] += (dt * 5.0f * dirsign * (i + 1));

        vec2f xz = vec2f(cos(lr[i]), sin(lr[i]));

        vec3f dir = vec3f(xz.x, 1.0f, xz.y);
        scene->lights[i].direction = dir;
    }
}
