#include "../example_common.h"

using namespace put;
using namespace ecs;

pen::window_creation_params pen_window{
    1280,                    // width
    720,                     // height
    4,                       // MSAA samples
    "advanced_rigid_bodies"  // window title / process name
};

void triangle_normal_dbg(const vec3f& v0, const vec3f& v1, const vec3f& v2)
{
    vec3f vc = (v0 + v1 + v2) / 3.0f;
    
    dbg::add_triangle(v0, v1, v2);
    
    vec3f vn = maths::get_normal(v0, v1, v2);
    
    dbg::add_line(vc, vc + vn * 0.1f, vec4f::green());
}

u32 convex;

void gen_convex_shape(physics::collision_mesh_data& cmd)
{
    // pentagon corners
    static f32 ps = 2.0f;
    
    vec3f pc[] = {
        vec3f(0.0f, -ps, 0.0f),
        vec3f(-ps/2.0f, -ps/2.0f, 0.0f),
        vec3f(-ps/3.0f, ps/2.0f, 0.0f),
        vec3f( ps/3.0f, ps/2.0f, 0.0f),
        vec3f( ps/2.0f, -ps/2.0, 0.0f)
    };
    
    for(auto& pp : pc)
    {
        dbg::add_point(pp, 0.2f);
    }
    
    // pentagon triangles
    vec3f* verts = nullptr;
    
    for(u32 i = 0; i < 5; ++i)
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
    u32 vb_size = num_verts*3*sizeof(f32);
    u32 ib_size = num_verts*sizeof(u32);
    
    cmd.vertices = (f32*)pen::memory_alloc(vb_size);
    memcpy(cmd.vertices, verts, vb_size);
    
    cmd.indices = (u32*)pen::memory_alloc(ib_size);
    for(u32 i = 0; i < num_verts; ++i)
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

    // ground
    u32 ground = get_new_entity(scene);
    scene->names[ground] = "ground";
    scene->transforms[ground].translation = vec3f::zero();
    scene->transforms[ground].rotation = quat();
    scene->transforms[ground].scale = vec3f(50.0f, 1.0f, 50.0f);
    scene->entities[ground] |= CMP_TRANSFORM;
    scene->parents[ground] = ground;
    scene->physics_data[ground].rigid_body.shape = physics::BOX;
    scene->physics_data[ground].rigid_body.mass = 0.0f;
    instantiate_geometry(box, scene, ground);
    instantiate_material(default_material, scene, ground);
    instantiate_model_cbuffer(scene, ground);
    instantiate_rigid_body(scene, ground);
    
    // convex rb
    convex = get_new_entity(scene);
    scene->names[convex] = "convex";
    scene->transforms[convex].translation = vec3f(0.0f, 8.0f, 0.0f);
    scene->transforms[convex].rotation = quat();
    scene->transforms[convex].scale = vec3f(1.0f, 1.0f, 1.0f);
    scene->entities[convex] |= CMP_TRANSFORM;
    scene->parents[convex] = convex;
    scene->physics_data[convex].rigid_body.shape = physics::HULL;
    scene->physics_data[convex].rigid_body.mass = 1.0f;
    
    physics::collision_mesh_data& cmd = scene->physics_data[convex].rigid_body.mesh_data;
    gen_convex_shape(cmd);

    instantiate_rigid_body(scene, convex);
    
    // concave rb
    
    // load physics stuff before calling update
    physics::physics_consume_command_buffer();
    pen::thread_sleep_ms(16);
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    // debug draw shape
    physics::collision_mesh_data& convex_cmd = scene->physics_data[convex].rigid_body.mesh_data;
    mat4& convex_mat = scene->world_matrices[convex];
    
    for(u32 i = 0; i < convex_cmd.num_floats; i+=9)
    {
        vec3f* verts = (vec3f*)&convex_cmd.vertices[i];
        
        // transform
        vec3f v0 = convex_mat.transform_vector(verts[0]);
        vec3f v1 = convex_mat.transform_vector(verts[1]);
        vec3f v2 = convex_mat.transform_vector(verts[2]);
        
        triangle_normal_dbg(v0, v1, v2);
    }
}
