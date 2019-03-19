#include "../example_common.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,        // width
    720,         // height
    4,           // MSAA samples
    "entities"   // window title / process name
};

void example_setup(ecs::ecs_scene* scene)
{
    clear_scene(scene);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));
    geometry_resource* box_resource = get_geometry_resource(PEN_HASH("cube"));

    // add lights
    u32 light = get_new_node(scene);
    scene->names[light] = "front_light";
    scene->id_name[light] = PEN_HASH("front_light");
    scene->lights[light].colour = vec3f::cyan();
    scene->lights[light].direction = vec3f::one();
    scene->lights[light].type = LIGHT_TYPE_DIR;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;
    
    light = get_new_node(scene);
    scene->names[light] = "back_light";
    scene->id_name[light] = PEN_HASH("back_light");
    scene->lights[light].colour = vec3f::magenta();
    scene->lights[light].direction = vec3f(-1.0f, 1.0f, 1.0f);
    scene->lights[light].type = LIGHT_TYPE_DIR;
    scene->transforms[light].translation = vec3f::zero();
    scene->transforms[light].rotation = quat();
    scene->transforms[light].scale = vec3f::one();
    scene->entities[light] |= CMP_LIGHT;
    scene->entities[light] |= CMP_TRANSFORM;

    //floor for shadow casting
    u32 floor = get_new_node(scene);
    scene->names[floor] = "floor";
    scene->transforms[floor].rotation = quat();
    scene->transforms[floor].rotation.euler_angles(0.0f, 0.0f, 0.0f);
    scene->transforms[floor].scale = vec3f(100000.0f, 1.0f, 100000.0f);
    scene->transforms[floor].translation = vec3f(0.0f, -100.0f, 0.0f);
    scene->entities[floor] |= CMP_TRANSFORM;
    scene->parents[floor] = floor;
    instantiate_geometry(box_resource, scene, floor);
    instantiate_material(default_material, scene, floor);
    instantiate_model_cbuffer(scene, floor);
    
    u32 master_node = get_new_node(scene);
    scene->names[master_node] = "master";
    scene->transforms[master_node].rotation = quat();
    scene->transforms[master_node].rotation.euler_angles(0.0f, 0.0f, 0.0f);
    scene->transforms[master_node].scale = vec3f::one();
    scene->transforms[master_node].translation = vec3f::zero();
    scene->entities[master_node] |= CMP_TRANSFORM;
    scene->parents[master_node] = master_node;
    instantiate_geometry(box_resource, scene, master_node);
    instantiate_material(default_material, scene, master_node);
    instantiate_model_cbuffer(scene, master_node);

    s32 num = 128; // 16k instances;
    f32 angle = 0.0f;
    f32 inner_angle = 0.0f;
    f32 rad = 50.0f;
    
    for(u32 i = 0; i < num; ++i)
    {
        vec2f xz = vec2f(cos(angle), sin(angle));
        vec3f pos = vec3f(xz.x, 0.0f, xz.y) * rad;
        
        for(u32 j = 0; j < num; ++j)
        {
            vec3f a = normalised(pos);
            
            mat4 rot = mat::create_rotation(cross(vec3f::unit_y(), a), inner_angle);
            
            vec3f iv = rot.transform_vector(a);
            
            vec3f inner_pos = pos + iv * (rad/2.0f);
            
            f32 off = sin(angle);
            inner_pos.y += off * (rad/2.0);
            
            u32 new_prim = get_new_node(scene);
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
            scene->entities[new_prim] |= CMP_TRANSFORM;
            scene->parents[new_prim] = master_node;
            
            scene->bounding_volumes[new_prim] = scene->bounding_volumes[master_node];
            
            scene->entities[new_prim] |= CMP_GEOMETRY;
            scene->entities[new_prim] |= CMP_MATERIAL;
            scene->entities[new_prim] |= CMP_SUB_INSTANCE;
            
            scene->draw_call_data[new_prim].v2 = vec4f::white();
            
            inner_angle += (M_PI * 2.0f) / num;
        }
        
        angle += (M_PI * 2.0f) / num;
    }

    instance_node_range(scene, master_node, pow(num, 2));
}

void example_update(ecs::ecs_scene* scene)
{
    quat q;
    q.euler_angles(0.01f, 0.01f, 0.01f);

    static u32 timer = pen::timer_create("perf");

    pen::timer_start(timer);
    for (s32 i = 4; i < scene->num_nodes; ++i)
    {
        scene->transforms.data[i].rotation = scene->transforms.data[i].rotation * q;
        scene->entities.data[i] |= CMP_TRANSFORM;
    }

#if 0 // debug / test array cost vs operator [] in component entity system
    f32 array_cost = pen::timer_elapsed_ms(timer);
    pen::timer_start(timer);
    for (s32 i = 2; i < scene->num_nodes; ++i)
    {
        scene->transforms[i].rotation = scene->transforms[i].rotation * q;
        scene->entities[i] |= CMP_TRANSFORM;
    }
    f32 operator_cost = pen::timer_elapsed_ms(timer);

    PEN_LOG("operator: %f, array: %f\n", operator_cost, array_cost);
#endif
}
