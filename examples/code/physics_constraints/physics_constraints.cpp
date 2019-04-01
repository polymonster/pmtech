#include "../example_common.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,                 // width
    720,                  // height
    4,                    // MSAA samples
    "physics_constraints" // window title / process name
};

void example_setup(ecs_scene* scene, camera& cam)
{
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

    // add a hinge in the x-axis
    vec3f hinge_x_pos = vec3f(-30.0f, 0.0f, 0.0f);

    u32 hinge_x_body = get_new_entity(scene);
    scene->names[hinge_x_body] = "hinge_x_body";
    scene->parents[hinge_x_body] = hinge_x_body;
    scene->transforms[hinge_x_body].translation = hinge_x_pos;
    scene->transforms[hinge_x_body].rotation = quat();
    scene->transforms[hinge_x_body].scale = vec3f(10.0f, 1.0f, 10.0f);
    scene->entities[hinge_x_body] |= CMP_TRANSFORM;
    instantiate_geometry(box, scene, hinge_x_body);
    instantiate_material(default_material, scene, hinge_x_body);
    instantiate_model_cbuffer(scene, hinge_x_body);
    scene->physics_data[hinge_x_body].rigid_body.shape = physics::BOX;
    scene->physics_data[hinge_x_body].rigid_body.mass = 1.0f;
    instantiate_rigid_body(scene, hinge_x_body);

    u32 hinge_x_constraint = get_new_entity(scene);
    scene->names[hinge_x_constraint] = "hinge_x_constraint";
    scene->parents[hinge_x_constraint] = hinge_x_constraint;
    scene->transforms[hinge_x_constraint].translation = hinge_x_pos - vec3f(0.0f, 0.0f, -10.0f);
    scene->transforms[hinge_x_constraint].rotation = quat();
    scene->transforms[hinge_x_constraint].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[hinge_x_constraint] |= CMP_TRANSFORM;
    scene->physics_data[hinge_x_constraint].constraint.type = physics::CONSTRAINT_HINGE;
    scene->physics_data[hinge_x_constraint].constraint.axis = vec3f::unit_x();
    scene->physics_data[hinge_x_constraint].constraint.rb_indices[0] = scene->physics_handles[hinge_x_body];
    scene->physics_data[hinge_x_constraint].constraint.lower_limit_rotation.x = -M_PI;
    scene->physics_data[hinge_x_constraint].constraint.upper_limit_rotation.x = M_PI;
    instantiate_constraint(scene, hinge_x_constraint);

    // add hinge in the y-axis with rotational limits
    vec3f hinge_y_pos = vec3f(-30.0f, 10.0f, -30.0f);

    u32 hinge_y_body = get_new_entity(scene);
    scene->names[hinge_y_body] = "hinge_x_body";
    scene->parents[hinge_y_body] = hinge_y_body;
    scene->transforms[hinge_y_body].translation = hinge_y_pos;
    scene->transforms[hinge_y_body].rotation = quat();
    scene->transforms[hinge_y_body].scale = vec3f(10.0f, 10.0f, 1.0f);
    scene->entities[hinge_y_body] |= CMP_TRANSFORM;
    instantiate_geometry(box, scene, hinge_y_body);
    instantiate_material(default_material, scene, hinge_y_body);
    instantiate_model_cbuffer(scene, hinge_y_body);
    scene->physics_data[hinge_y_body].rigid_body.shape = physics::BOX;
    scene->physics_data[hinge_y_body].rigid_body.mass = 1.0f;
    instantiate_rigid_body(scene, hinge_y_body);

    u32 hinge_y_constraint = get_new_entity(scene);
    scene->names[hinge_y_constraint] = "hinge_y_constraint";
    scene->parents[hinge_y_constraint] = hinge_y_constraint;
    scene->transforms[hinge_y_constraint].translation = hinge_y_pos - vec3f(10.0f, 0.0f, 0.0f);
    scene->transforms[hinge_y_constraint].rotation = quat();
    scene->transforms[hinge_y_constraint].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[hinge_y_constraint] |= CMP_TRANSFORM;
    scene->physics_data[hinge_y_constraint].constraint.type = physics::CONSTRAINT_HINGE;
    scene->physics_data[hinge_y_constraint].constraint.axis = vec3f::unit_y();
    scene->physics_data[hinge_y_constraint].constraint.rb_indices[0] = scene->physics_handles[hinge_y_body];
    scene->physics_data[hinge_y_constraint].constraint.lower_limit_rotation.x = -M_PI / 2;
    scene->physics_data[hinge_y_constraint].constraint.upper_limit_rotation.x = M_PI / 2;
    instantiate_constraint(scene, hinge_y_constraint);

    // add box with a point to point constraint
    vec3f p2p_pos = vec3f(0.0f, 5.0f, -10.0f);

    u32 p2p_body = get_new_entity(scene);
    scene->names[p2p_body] = "p2p_body";
    scene->parents[p2p_body] = p2p_body;
    scene->transforms[p2p_body].translation = p2p_pos;
    scene->transforms[p2p_body].rotation = quat();
    scene->transforms[p2p_body].scale = vec3f(2.0f, 2.0f, 2.0f);
    scene->entities[p2p_body] |= CMP_TRANSFORM;
    instantiate_geometry(box, scene, p2p_body);
    instantiate_material(default_material, scene, p2p_body);
    instantiate_model_cbuffer(scene, p2p_body);
    scene->physics_data[p2p_body].rigid_body.shape = physics::BOX;
    scene->physics_data[p2p_body].rigid_body.mass = 1.0f;
    instantiate_rigid_body(scene, p2p_body);

    u32 p2p_constraint = get_new_entity(scene);
    scene->names[p2p_constraint] = "p2p_constraint";
    scene->parents[p2p_constraint] = p2p_constraint;
    scene->transforms[p2p_constraint].translation = p2p_pos + vec3f(0.0f, 10.0f, 0.0f);
    scene->transforms[p2p_constraint].rotation = quat();
    scene->transforms[p2p_constraint].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[p2p_constraint] |= CMP_TRANSFORM;
    scene->physics_data[p2p_constraint].constraint.type = physics::CONSTRAINT_P2P;
    scene->physics_data[p2p_constraint].constraint.rb_indices[0] = scene->physics_handles[p2p_body];
    instantiate_constraint(scene, p2p_constraint);

    // add slider constraint (six degrees of freedom in an axis)
    vec3f slider_x_pos = vec3f(5.0f, 3.0f, 20.0f);

    u32 slider_x_body = get_new_entity(scene);
    scene->names[slider_x_body] = "slider_x_body";
    scene->parents[slider_x_body] = slider_x_body;
    scene->transforms[slider_x_body].translation = slider_x_pos;
    scene->transforms[slider_x_body].rotation = quat();
    scene->transforms[slider_x_body].scale = vec3f(2.0f, 2.0f, 2.0f);
    scene->entities[slider_x_body] |= CMP_TRANSFORM;
    instantiate_geometry(box, scene, slider_x_body);
    instantiate_material(default_material, scene, slider_x_body);
    instantiate_model_cbuffer(scene, slider_x_body);
    scene->physics_data[slider_x_body].rigid_body.shape = physics::BOX;
    scene->physics_data[slider_x_body].rigid_body.mass = 1.0f;
    instantiate_rigid_body(scene, slider_x_body);

    u32 slider_x_constraint = get_new_entity(scene);
    scene->names[slider_x_constraint] = "slider_x_constraint";
    scene->parents[slider_x_constraint] = slider_x_constraint;
    scene->transforms[slider_x_constraint].translation = slider_x_pos;
    scene->transforms[slider_x_constraint].rotation = quat();
    scene->transforms[slider_x_constraint].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[slider_x_constraint] |= CMP_TRANSFORM;
    scene->physics_data[slider_x_constraint].constraint.type = physics::CONSTRAINT_DOF6;
    scene->physics_data[slider_x_constraint].constraint.rb_indices[0] = scene->physics_handles[slider_x_body];
    scene->physics_data[slider_x_constraint].constraint.lower_limit_rotation = vec3f::zero();
    scene->physics_data[slider_x_constraint].constraint.upper_limit_rotation = vec3f::zero();
    scene->physics_data[slider_x_constraint].constraint.lower_limit_translation = -vec3f::unit_x() * 10.0f;
    scene->physics_data[slider_x_constraint].constraint.upper_limit_translation = vec3f::unit_x() * 10.0f;
    instantiate_constraint(scene, slider_x_constraint);

    // load physics stuff before calling update
    physics::physics_consume_command_buffer();
    pen::thread_sleep_ms(16);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    
}
