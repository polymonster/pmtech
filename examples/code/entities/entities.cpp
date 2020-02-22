#include "../example_common.h"

using namespace put;
using namespace ecs;

static u32 master_node = 0;

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "entities";
        p.window_sample_count = 4;
        p.user_thread_function = user_entry;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

void example_setup(ecs_scene* scene, camera& cam)
{
    cam.zoom = 180;
    cam.rot = vec2f(-0.6, 2.2);

    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* box_resource = get_geometry_resource(PEN_HASH("cube"));

    // add lights
    u32 light = get_new_entity(scene);
    scene->names[light] = "cyan_light";
    scene->id_name[light] = PEN_HASH("cyan_light");
    scene->lights[light].colour = vec3f(250.0f, 162.0f, 117.0f) / 255.0f;
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = e_light_type::dir;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    light = get_new_entity(scene);
    scene->names[light] = "magenta_light";
    scene->id_name[light] = PEN_HASH("magenta_light");
    scene->lights[light].colour = vec3f(206.0f, 106.0f, 84.0f) / 255.0f;
    scene->lights[light].direction = vec3f(-1.0f, 1.0f, 1.0f);
    scene->lights[light].type = e_light_type::dir;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    light = get_new_entity(scene);
    scene->names[light] = "yellow_light";
    scene->id_name[light] = PEN_HASH("yellow_light");
    scene->lights[light].colour = vec3f(152.0f, 82.0f, 119.0f) / 255.0f;
    scene->lights[light].direction = vec3f(0.0f, 1.0f, 1.0f);
    scene->lights[light].type = e_light_type::dir;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    light = get_new_entity(scene);
    scene->names[light] = "red_light";
    scene->id_name[light] = PEN_HASH("red_light");
    scene->lights[light].colour = vec3f(222.0f, 50.0f, 97.0f) / 255.0f;
    scene->lights[light].direction = vec3f(0.0f, 1.0f, 1.0f);
    scene->lights[light].type = e_light_type::dir;
    scene->lights[light].shadow_map = true;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= e_cmp::light;
    scene->entities[light] |= e_cmp::transform;

    //floor for shadow casting
    u32 floor = get_new_entity(scene);
    scene->names[floor] = "floor";
    scene->transforms[floor].rotation = quat();
    scene->transforms[floor].rotation.euler_angles(0.0f, 0.0f, 0.0f);
    scene->transforms[floor].scale = vec3f(100.0f, 1.0f, 100.0f);
    scene->transforms[floor].translation = vec3f(0.0f, -60.0f, 0.0f);
    scene->entities[floor] |= e_cmp::transform;
    scene->parents[floor] = floor;
    instantiate_geometry(box_resource, scene, floor);
    instantiate_material(default_material, scene, floor);
    instantiate_model_cbuffer(scene, floor);

    // back walls for shadows
    vec3f offset[] = {vec3f(150.0f, 0.0f, 0.0f), vec3f(0.0f, 0.0f, 150.0f)};

    vec3f scale[] = {vec3f(1.0f, 50.0f, 100.0f), vec3f(100.0f, 50.0f, 1.0f)};

    for (u32 i = 0; i < 2; ++i)
    {
        u32 wall = get_new_entity(scene);
        scene->names[wall] = "floor";
        scene->transforms[wall].rotation = quat();
        scene->transforms[wall].rotation.euler_angles(0.0f, 0.0f, 0.0f);
        scene->transforms[wall].scale = scale[i];
        scene->transforms[wall].translation = offset[i];
        scene->entities[wall] |= e_cmp::transform;
        scene->parents[wall] = wall;
        instantiate_geometry(box_resource, scene, wall);
        instantiate_material(default_material, scene, wall);
        instantiate_model_cbuffer(scene, wall);
    }

    master_node = get_new_entity(scene);
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

    s32 num = 256; // 64k instances;
    f32 angle = 0.0f;
    f32 inner_angle = 0.0f;
    f32 rad = 50.0f;

    for (u32 i = 0; i < num; ++i)
    {
        vec2f xz = vec2f(cos(angle), sin(angle));
        vec3f pos = vec3f(xz.x, 0.0f, xz.y) * rad;

        for (u32 j = 0; j < num; ++j)
        {
            vec3f a = normalised(pos);

            mat4 rot = mat::create_rotation(cross(vec3f::unit_y(), a), inner_angle);

            vec3f iv = rot.transform_vector(a);

            vec3f inner_pos = pos + iv * (rad / 2.0f);

            f32 off = sin(angle);
            inner_pos.y += off * (rad / 2.0);

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
            scene->transforms[new_prim].translation = inner_pos;
            scene->entities[new_prim] |= e_cmp::transform;
            scene->parents[new_prim] = master_node;

            scene->bounding_volumes[new_prim] = scene->bounding_volumes[master_node];

            scene->entities[new_prim] |= e_cmp::geometry;
            scene->entities[new_prim] |= e_cmp::material;
            scene->entities[new_prim] |= e_cmp::sub_instance;

            scene->draw_call_data[new_prim].v2 = vec4f::white();

            inner_angle += (M_PI * 2.0f) / num;
        }

        angle += (M_PI * 2.0f) / num;
    }

    instance_entity_range(scene, master_node, pow(num, 2));
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    quat q;
    q.euler_angles(0.01f, 0.01f, 0.01f);

    static f32 lr[] = {0.0f, M_PI * 0.5f, M_PI, M_PI * 1.5f};

    for (u32 i = 0; i < 4; ++i)
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

    for (s32 i = master_node + 1; i < scene->num_entities; ++i)
    {
        // animate boxes
        scene->transforms[i].rotation = scene->transforms[i].rotation * q;
        scene->entities[i] |= e_cmp::transform;
    }

#if 0 // debug / test array cost vs operator [] in component entity system
    static pen::timer* timer = pen::timer_create("perf");
    pen::timer_start(timer);
    for (s32 i = 0; i < scene->num_nodes; ++i)
    {
        if(!(scene->entities.data[i] & e_cmp::sdf_shadow))
            continue;
        
        scene->transforms.data[i].rotation = scene->transforms.data[i].rotation * q;
        scene->entities.data[i] |= e_cmp::transform;
    }
    f32 operator_cost = pen::timer_elapsed_ms(timer);
    pen::timer_start(timer);
    f32 array_cost = pen::timer_elapsed_ms(timer);
    PEN_LOG("operator: %f, array: %f\n", operator_cost, array_cost);
#endif
}
