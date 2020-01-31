#include "../example_common.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,                  // width
    720,                   // height
    4,                     // MSAA samples
    "complex_rigid_bodies" // window title / process name
};

u32 convex;
u32 concave;

void gen_convex_shape(physics::collision_mesh_data& cmd)
{
    // pentagon corners
    static f32 ps = 2.0f;

    vec3f pc[] = {vec3f(0.0f, -ps, 0.0f), vec3f(-ps / 2.0f, -ps / 2.0f, 0.0f), vec3f(-ps / 3.0f, ps / 2.0f, 0.0f),
                  vec3f(ps / 3.0f, ps / 2.0f, 0.0f), vec3f(ps / 2.0f, -ps / 2.0, 0.0f)};

    for (auto& pp : pc)
    {
        dbg::add_point(pp, 0.2f);
    }

    // pentagon triangles
    vec3f* verts = nullptr;

    for (u32 i = 0; i < 5; ++i)
    {
        u32 n = (i + 1) % 5;

        vec3f v0 = vec3f::zero();
        vec3f v1 = pc[i];
        vec3f v2 = pc[n];

        // face 0
        sb_push(verts, v0);
        sb_push(verts, v1);
        sb_push(verts, v2);

        // face 1. flip wind
        vec3f w0 = v0 + vec3f::unit_z();
        vec3f w1 = v2 + vec3f::unit_z();
        vec3f w2 = v1 + vec3f::unit_z();

        sb_push(verts, w0);
        sb_push(verts, w1);
        sb_push(verts, w2);

        // edges
        vec3f ev0 = w1;
        vec3f ev1 = v2;
        vec3f ev2 = v1;

        sb_push(verts, ev0);
        sb_push(verts, ev1);
        sb_push(verts, ev2);

        vec3f ew0 = v1;
        vec3f ew1 = w2;
        vec3f ew2 = w1;

        sb_push(verts, ew0);
        sb_push(verts, ew1);
        sb_push(verts, ew2);
    }

    // indices are just 1 to 1 with verts
    u32 num_verts = sb_count(verts);
    u32 vb_size = num_verts * 3 * sizeof(f32);
    u32 ib_size = num_verts * sizeof(u32);

    cmd.vertices = (f32*)pen::memory_alloc(vb_size);
    memcpy(cmd.vertices, verts, vb_size);

    cmd.indices = (u32*)pen::memory_alloc(ib_size);
    for (u32 i = 0; i < num_verts; ++i)
    {
        cmd.indices[i] = i;
    }

    cmd.num_floats = num_verts * 3;
    cmd.num_indices = num_verts;

    // free sb
    sb_free(verts);
}

void gen_concave_shape(physics::collision_mesh_data& cmd)
{
    vec3f* verts = nullptr;

    vec3f v0 = vec3f(0.0f, 0.0f, -50.0f);
    vec3f v1 = vec3f(0.0f, 0.0f, 50.0f);
    vec3f v2 = vec3f(50.0f, 20.0f, 50.0f);

    vec3f w0 = vec3f(0.0f, 0.0f, -50.0f);
    vec3f w1 = vec3f(0.0f, 0.0f, 50.0f);
    vec3f w2 = vec3f(-50.0f, 20.0f, 50.0f);

    sb_push(verts, v0);
    sb_push(verts, v1);
    sb_push(verts, v2);

    sb_push(verts, w0);
    sb_push(verts, w1);
    sb_push(verts, w2);

    // indices are just 1 to 1 with verts
    u32 num_verts = sb_count(verts);
    u32 vb_size = num_verts * 3 * sizeof(f32);
    u32 ib_size = num_verts * sizeof(u32);

    cmd.vertices = (f32*)pen::memory_alloc(vb_size);
    memcpy(cmd.vertices, verts, vb_size);

    cmd.indices = (u32*)pen::memory_alloc(ib_size);
    for (u32 i = 0; i < num_verts; ++i)
    {
        cmd.indices[i] = i;
    }

    cmd.num_floats = num_verts * 3;
    cmd.num_indices = num_verts;

    // free sb
    sb_free(verts);
}

void example_setup(ecs_scene* scene, camera& cam)
{
    scene->view_flags &= ~e_scene_view_flags::hide_debug;
    scene->view_flags |= e_scene_view_flags::physics;
    editor_set_transform_mode(e_transform_mode::physics);
    
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

    // boxes
    vec3f bp = vec3f(-20.0f, 20.0f, 0.0f);
    for (u32 i = 0; i < 10; ++i)
    {
        u32 bb = get_new_entity(scene);
        scene->names[bb].setf("bpx_%i", i);
        scene->transforms[bb].translation = bp;
        scene->transforms[bb].rotation = quat();
        scene->transforms[bb].scale = vec3f(0.5f, 0.5f, 0.5f);
        scene->entities[bb] |= CMP_TRANSFORM;
        scene->parents[bb] = bb;
        scene->physics_data[bb].rigid_body.shape = physics::e_shape::box;
        scene->physics_data[bb].rigid_body.mass = 1.0f;
        instantiate_geometry(box, scene, bb);
        instantiate_material(default_material, scene, bb);
        instantiate_model_cbuffer(scene, bb);
        instantiate_rigid_body(scene, bb);

        bp += vec3f(40.0f / 10.0f, 0.0f, 0.0f);
    }

    // convex rb
    convex = get_new_entity(scene);
    scene->names[convex] = "convex";
    scene->transforms[convex].translation = vec3f(0.0f, 8.0f, 0.0f);
    scene->transforms[convex].rotation = quat();
    scene->transforms[convex].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[convex] |= CMP_TRANSFORM;
    scene->parents[convex] = convex;
    scene->physics_data[convex].rigid_body.shape = physics::e_shape::hull;
    scene->physics_data[convex].rigid_body.mass = 1.0f;

    gen_convex_shape(scene->physics_data[convex].rigid_body.mesh_data);

    instantiate_rigid_body(scene, convex);

    // concave rb
    concave = get_new_entity(scene);
    scene->names[concave] = "concave";
    scene->transforms[concave].translation = vec3f(0.0f, 0.0f, 0.0f);
    scene->transforms[concave].rotation = quat();
    scene->transforms[concave].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[concave] |= CMP_TRANSFORM;
    scene->parents[concave] = concave;
    scene->physics_data[concave].rigid_body.shape = physics::e_shape::mesh;
    scene->physics_data[concave].rigid_body.mass = 0.0f;

    gen_concave_shape(scene->physics_data[concave].rigid_body.mesh_data);

    instantiate_rigid_body(scene, concave);

    // compound shape
    u32 compound = get_new_entity(scene);
    scene->names[compound] = "compound";
    scene->transforms[compound].translation = vec3f(1.0f, 8.0f, 4.0f);
    scene->transforms[compound].rotation = quat();
    scene->transforms[compound].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[compound] |= CMP_TRANSFORM;
    scene->parents[compound] = compound;
    scene->physics_data[compound].rigid_body.shape = physics::e_shape::compound;
    scene->physics_data[compound].rigid_body.mass = 1.0f;

    //instantiate_rigid_body(scene, compound);

    u32 cc = get_new_entity(scene);
    scene->names[cc] = "compound_child0";
    scene->transforms[cc].translation = vec3f(1.0f, 8.0f, 4.0f);
    scene->transforms[cc].rotation = quat();
    scene->transforms[cc].scale = vec3f(0.5f, 2.0f, 0.5f);
    scene->entities[cc] |= CMP_TRANSFORM;
    scene->parents[cc] = cc;
    scene->physics_data[cc].rigid_body.shape = physics::e_shape::box;
    scene->physics_data[cc].rigid_body.mass = 1.0f;
    instantiate_geometry(box, scene, cc);
    instantiate_material(default_material, scene, cc);
    instantiate_model_cbuffer(scene, cc);

    u32* compound_children = nullptr;
    sb_push(compound_children, cc);

    cc = get_new_entity(scene);
    scene->names[cc] = "compound_child1";
    scene->transforms[cc].translation = vec3f(3.0f, 8.0f, 4.0f);
    scene->transforms[cc].rotation = quat();
    scene->transforms[cc].scale = vec3f(2.0f, 0.5f, 0.5f);
    scene->entities[cc] |= CMP_TRANSFORM;
    scene->parents[cc] = cc;
    scene->physics_data[cc].rigid_body.shape = physics::e_shape::box;
    scene->physics_data[cc].rigid_body.mass = 1.0f;
    instantiate_geometry(box, scene, cc);
    instantiate_material(default_material, scene, cc);
    instantiate_model_cbuffer(scene, cc);

    sb_push(compound_children, cc);

    ecs::instantiate_compound_rigid_body(scene, compound, compound_children, 2);

    // load physics stuff before calling update
    physics::physics_consume_command_buffer();
    pen::thread_sleep_ms(16);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    // debug draw shape
    physics::collision_mesh_data& convex_cmd = scene->physics_data[convex].rigid_body.mesh_data;
    mat4&                         convex_mat = scene->world_matrices[convex];

    for (u32 i = 0; i < convex_cmd.num_floats; i += 9)
    {
        vec3f* verts = (vec3f*)&convex_cmd.vertices[i];

        // transform
        vec3f v0 = convex_mat.transform_vector(verts[0]);
        vec3f v1 = convex_mat.transform_vector(verts[1]);
        vec3f v2 = convex_mat.transform_vector(verts[2]);

        dbg::add_triangle_with_normal(v0, v1, v2);
    }

    // concave mesh
    vec3f v0 = vec3f(0.0f, 0.0f, -50.0f);
    vec3f v1 = vec3f(0.0f, 0.0f, 50.0f);
    vec3f v2 = vec3f(50.0f, 20.0f, 50.0f);

    dbg::add_triangle_with_normal(v0, v1, v2);

    vec3f w0 = vec3f(0.0f, 0.0f, -50.0f);
    vec3f w1 = vec3f(0.0f, 0.0f, 50.0f);
    vec3f w2 = vec3f(-50.0f, 20.0f, 50.0f);

    dbg::add_triangle_with_normal(w0, w1, w2);
}
