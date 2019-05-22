#include "../example_common.h"
#include "../../shader_structs/forward_render.h"

#include "bussik/Node.h"
#include "bussik/Tree.h"
#include "bussik/Jacobian.h"
#include "bussik/VectorRn.h"

using namespace put;
using namespace put::ecs;

pen::window_creation_params pen_window{
    1280,        // width
    720,         // height
    4,           // MSAA samples
    "ik"         // window title / process name
};

namespace {
    Tree ik_tree;
    Node* ik_nodes[8];
    Jacobian* ik_jacobian;
    u32 root;
}

vec3f ik_v3(const VectorR3& v)
{
    return vec3f(v.x, v.y, v.z);
}

VectorR3 v3_ik(const vec3f& v)
{
    return VectorR3(v.x, v.y, v.z);
}

void get_local_transform(const Node* node, maths::transform& t)
{
    vec3f axis = ik_v3(node->v);
    
    t.scale = vec3f::one();
    t.translation = ik_v3(node->r);
    
    if(mag2(axis))
        t.rotation.axis_angle(axis, node->theta);
}

void reset(Tree& tree, Jacobian* jac)
{
    tree.Init();
    tree.Compute();
    jac->Reset();
}

void example_setup(ecs::ecs_scene* scene, camera& cam)
{
    //const VectorR3& unitx = VectorR3::UnitX;
    const VectorR3& unity = VectorR3::UnitY;
    const VectorR3& unitz = VectorR3::UnitZ;
    const VectorR3 unit1(sqrt(14.0) / 8.0, 1.0 / 8.0, 7.0 / 8.0);
    const VectorR3& zero = VectorR3::Zero;
    
    float minTheta = -4 * PI;
    float maxTheta = 4 * PI;
    
    ik_nodes[0] = new Node(VectorR3(0.100000, 0.000000, 0.087500), unitz, 0.08, JOINT, -1e30, 1e30, maths::deg_to_rad(0.));
    ik_tree.InsertRoot(ik_nodes[0]);
    
    ik_nodes[1] = new Node(VectorR3(0.100000, -0.000000, 0.290000), unity, 0.08, JOINT, -0.5, 0.4, maths::deg_to_rad(0.));
    ik_tree.InsertLeftChild(ik_nodes[0], ik_nodes[1]);
    
    ik_nodes[2] = new Node(VectorR3(0.100000, -0.000000, 0.494500), unitz, 0.08, JOINT, minTheta, maxTheta, maths::deg_to_rad(0.));
    ik_tree.InsertLeftChild(ik_nodes[1], ik_nodes[2]);
    
    ik_nodes[3] = new Node(VectorR3(0.100000, 0.000000, 0.710000), -unity, 0.08, JOINT, minTheta, maxTheta, maths::deg_to_rad(0.));
    ik_tree.InsertLeftChild(ik_nodes[2], ik_nodes[3]);
    
    ik_nodes[4] = new Node(VectorR3(0.100000, 0.000000, 0.894500), unitz, 0.08, JOINT, minTheta, maxTheta, maths::deg_to_rad(0.));
    ik_tree.InsertLeftChild(ik_nodes[3], ik_nodes[4]);
    
    ik_nodes[5] = new Node(VectorR3(0.100000, 0.000000, 1.110000), unity, 0.08, JOINT, minTheta, maxTheta, maths::deg_to_rad(0.));
    ik_tree.InsertLeftChild(ik_nodes[4], ik_nodes[5]);
    
    ik_nodes[6] = new Node(VectorR3(0.100000, 0.000000, 1.191000), unitz, 0.08, JOINT, minTheta, maxTheta, maths::deg_to_rad(0.));
    ik_tree.InsertLeftChild(ik_nodes[5], ik_nodes[6]);
    
    ik_nodes[7] = new Node(VectorR3(0.100000, 0.000000, 1.20000), zero, 0.08, EFFECTOR);
    ik_tree.InsertLeftChild(ik_nodes[6], ik_nodes[7]);
    
    ik_jacobian = new Jacobian(&ik_tree);
    reset(ik_tree, ik_jacobian);
    
    
    for(u32 i = 0; i < 8; ++i)
    {
        u32 e = ecs::get_new_entity(scene);
        if(i == 0)
            root = e;
        else
            scene->parents[e] = e - 1;
    }
}

void example_update(ecs::ecs_scene* scene, camera& cam, f32 dt)
{
    for(u32 i = 0; i < 8; ++i)
    {
        u32 ii = root + i;
        dbg::add_point(scene->world_matrices[ii].get_translation(), 0.1f);
    }
    
    for(u32 i = 0; i < 8; ++i)
    {
        u32 ii = root + i;
        get_local_transform(ik_nodes[i], scene->transforms[ii]);
        scene->entities[ii] |= CMP_TRANSFORM;
    }
}
