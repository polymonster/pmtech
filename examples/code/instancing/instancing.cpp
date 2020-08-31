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
        p.window_title = "instancing";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

void example_setup(ecs_scene* scene, camera& cam)
{
    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* box_resource = get_geometry_resource(PEN_HASH("cube"));

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

    f32 spacing = 4.0f;
    s32 num = 32; // 32768 instances;

    f32 start = (spacing * (num - 1)) * 0.5f;

    vec3f start_pos = vec3f(-start, -start, -start);

    u32 master_node = get_new_entity(scene);
    scene->names[master_node] = "master";

    scene->transforms[master_node].rotation = quat();
    scene->transforms[master_node].rotation.euler_angles(0.0f, 0.0f, 0.0f);
    scene->transforms[master_node].scale = vec3f::one();
    scene->transforms[master_node].translation = vec3f::zero();

    scene->entities[master_node] |= e_cmp::transform;
    scene->parents[master_node] = master_node;

    instantiate_geometry(box_resource, scene, master_node);
    instantiate_material(default_material, scene, master_node);
    instantiate_model_cbuffer(scene, master_node);

    vec3f cur_pos = start_pos;
    for (s32 i = 0; i < num; ++i)
    {
        cur_pos.y = start_pos.y;

        for (s32 j = 0; j < num; ++j)
        {
            cur_pos.x = start_pos.x;

            for (s32 k = 0; k < num; ++k)
            {
                u32 new_prim = get_new_entity(scene);
                scene->names[new_prim] = "box";
                scene->names[new_prim].appendf("%i", new_prim);

                // random rotation offset
                f32 x = maths::deg_to_rad(rand() % 360);
                f32 y = maths::deg_to_rad(rand() % 360);
                f32 z = maths::deg_to_rad(rand() % 360);

                scene->transforms[new_prim].rotation = quat();
                scene->transforms[new_prim].rotation.euler_angles(z, y, x);

                scene->transforms[new_prim].scale = vec3f::one();
                scene->transforms[new_prim].translation = cur_pos;
                scene->entities[new_prim] |= e_cmp::transform;
                scene->parents[new_prim] = master_node;

                scene->bounding_volumes[new_prim] = scene->bounding_volumes[master_node];

                scene->entities[new_prim] |= e_cmp::geometry;
                scene->entities[new_prim] |= e_cmp::material;

                scene->entities[new_prim] |= e_cmp::sub_instance;

                ImColor ii = ImColor::HSV((rand() % 255) / 255.0f, (rand() % 255) / 255.0f, (rand() % 255) / 255.0f);
                scene->draw_call_data[new_prim].v2 = vec4f(ii.Value.x, ii.Value.y, ii.Value.z, 1.0f);

                cur_pos.x += spacing;
            }

            cur_pos.y += spacing;
        }

        cur_pos.z += spacing;
    }

    instance_entity_range(scene, 1, pow(num, 3));
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    quat q;
    q.euler_angles(0.01f, 0.01f, 0.01f);

    static pen::timer* timer = pen::timer_create();

    pen::timer_start(timer);
    for (s32 i = 2; i < scene->num_entities; ++i)
    {
        scene->transforms.data[i].rotation = scene->transforms.data[i].rotation * q;
        scene->entities.data[i] |= e_cmp::transform;
    }

#if 1 // debug / test array cost vs operator [] in component entity system
    f32 array_cost = pen::timer_elapsed_ms(timer);
    pen::timer_start(timer);
    for (s32 i = 2; i < scene->num_entities; ++i)
    {
        scene->transforms[i].rotation = scene->transforms[i].rotation * q;
        scene->entities[i] |= e_cmp::transform;
    }
    f32 operator_cost = pen::timer_elapsed_ms(timer);

   // PEN_LOG("operator: %f, array: %f\n", operator_cost, array_cost);
#endif
}
