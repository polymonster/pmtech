#include "../example_common.h"

using namespace put;
using namespace ecs;

/*
const pen::music_item* k_music_items;
k_music_items = pen::music_get_items();

pen::music_file mf = music_open_file(k_music_items[0]);
sound_index = put::audio_create_sound(mf);
*/

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "game";
        p.window_sample_count = 4;
        p.user_thread_function = user_entry;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

void example_setup(ecs_scene* scene, camera& cam)
{
    scene->view_flags &= ~e_scene_view_flags::hide_debug;
    scene->view_flags |= e_scene_view_flags::physics;
    editor_set_transform_mode(e_transform_mode::physics);

    clear_scene(scene);
    
    cam.focus = vec3f(5.0f, 1.0f, 5.0f);
    cam.rot = vec2f(-0.9f, 0.0f);
    cam.zoom = 20.0f;
    camera_update_look_at(&cam);

    material_resource* default_material = get_material_resource(PEN_HASH("default_material"));

    geometry_resource* box = get_geometry_resource(PEN_HASH("cube"));
    geometry_resource* cyl = get_geometry_resource(PEN_HASH("cylinder"));

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

    //
    vec3f pitch_pos = vec3f(15.0f, 1.0f, 4.0f);

    u32 pitch = get_new_entity(scene);
    scene->names[pitch] = "slider_x_body";
    scene->parents[pitch] = pitch;
    scene->transforms[pitch].translation = pitch_pos;
    scene->transforms[pitch].rotation = quat();
    scene->transforms[pitch].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[pitch] |= e_cmp::transform;
    instantiate_geometry(box, scene, pitch);
    instantiate_material(default_material, scene, pitch);
    instantiate_model_cbuffer(scene, pitch);
    scene->physics_data[pitch].rigid_body.shape = physics::e_shape::box;
    scene->physics_data[pitch].rigid_body.mass = 1.0f;
    instantiate_rigid_body(scene, pitch);

    u32 pitch_constraint = get_new_entity(scene);
    scene->names[pitch_constraint] = "pitch_constraint";
    scene->parents[pitch_constraint] = pitch;
    scene->transforms[pitch_constraint].translation = pitch_pos;
    scene->transforms[pitch_constraint].rotation = quat();
    scene->transforms[pitch_constraint].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[pitch_constraint] |= e_cmp::transform;
    scene->physics_data[pitch_constraint].constraint.type = physics::e_constraint::dof6;
    scene->physics_data[pitch_constraint].constraint.rb_indices[0] = scene->physics_handles[pitch];
    scene->physics_data[pitch_constraint].constraint.lower_limit_rotation = vec3f::zero();
    scene->physics_data[pitch_constraint].constraint.upper_limit_rotation = vec3f::zero();
    scene->physics_data[pitch_constraint].constraint.lower_limit_translation = -vec3f::unit_z() * 5.0f;
    scene->physics_data[pitch_constraint].constraint.upper_limit_translation = vec3f::unit_z() * 5.0f;
    scene->physics_data[pitch_constraint].constraint.linear_damping = 0.999f;
    instantiate_constraint(scene, pitch_constraint);
    
    //
    vec3f platter_pos = vec3f(0.0f, 1.0f, 00.0f);
    
    u32 platter = get_new_entity(scene);
    scene->names[platter] = "platter";
    scene->parents[platter] = platter;
    scene->transforms[platter].translation = platter_pos;
    scene->transforms[platter].rotation = quat();
    scene->transforms[platter].scale = vec3f(10.0f, 1.0f, 10.0f);
    scene->entities[platter] |= e_cmp::transform;
    instantiate_geometry(cyl, scene, platter);
    instantiate_material(default_material, scene, platter);
    instantiate_model_cbuffer(scene, platter);
    scene->physics_data[platter].rigid_body.shape = physics::e_shape::cylinder;
    scene->physics_data[platter].rigid_body.mass = 1.0f;
    instantiate_rigid_body(scene, platter);

    u32 platter_constraint = get_new_entity(scene);
    scene->names[platter_constraint] = "platter_constraint";
    scene->parents[platter_constraint] = platter_constraint;
    scene->transforms[platter_constraint].translation = platter_pos;
    scene->transforms[platter_constraint].rotation = quat();
    scene->transforms[platter_constraint].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[platter_constraint] |= e_cmp::transform;
    scene->physics_data[platter_constraint].constraint.type = physics::e_constraint::hinge;
    scene->physics_data[platter_constraint].constraint.axis = vec3f::unit_y();
    scene->physics_data[platter_constraint].constraint.rb_indices[0] = scene->physics_handles[platter];
    scene->physics_data[platter_constraint].constraint.lower_limit_rotation.x = -M_PI;
    scene->physics_data[platter_constraint].constraint.upper_limit_rotation.x = M_PI;
    scene->physics_data[platter_constraint].constraint.angular_damping = 1.0f;
    instantiate_constraint(scene, platter_constraint);

    // load physics stuff before calling update
    physics::physics_consume_command_buffer();
    pen::thread_sleep_ms(16);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
}
