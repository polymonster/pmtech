// physics_bullet.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "data_struct.h"
#include "maths/maths.h"
#include "physics.h"

// for multi body bullet
#include "BulletDynamics/Featherstone/btMultiBody.h"
#include "BulletDynamics/Featherstone/btMultiBodyConstraintSolver.h"
#include "BulletDynamics/Featherstone/btMultiBodyDynamicsWorld.h"
#include "BulletDynamics/Featherstone/btMultiBodyJointLimitConstraint.h"
#include "BulletDynamics/Featherstone/btMultiBodyJointMotor.h"
#include "BulletDynamics/Featherstone/btMultiBodyLink.h"
#include "BulletDynamics/Featherstone/btMultiBodyLinkCollider.h"
#include "BulletDynamics/Featherstone/btMultiBodyPoint2Point.h"
#include "btBulletDynamicsCommon.h"

namespace physics
{
    enum e_entity_type
    {
        ENTITY_NULL = 0,
        ENTITY_RIGID_BODY,
        ENTITY_MULTI_BODY,
        ENTITY_CONSTRAINT,
        ENTITY_COMPOUND_RIGID_BODY,
        ENTITY_COMPOUND_RIGID_BODY_CHILD
    };

    struct rigid_body_entity
    {
        btRigidBody* rigid_body;
        u32          rigid_body_in_world;

        // for attaching and detaching rbs into compounds
        void* p_attach_user_data;
        s32   attach_shape_index;
        u32   call_attach = 0;
        void (*attach_function)(void* user_data, s32 attach_index);

        rigid_body_entity(){};
        ~rigid_body_entity(){};
    };

    struct mutli_body_entity
    {
        btMultiBody*                                           multi_body;
        btAlignedObjectArray<btMultiBodyJointMotor*>           joint_motors;
        btAlignedObjectArray<btMultiBodyJointLimitConstraint*> joint_limits;
        btAlignedObjectArray<btMultiBodyLinkCollider*>         link_colliders;
    };

    struct constraint_entity
    {
        constraint_type type;

        union {
            btTypedConstraint*       generic;
            btHingeConstraint*       hinge;
            btGeneric6DofConstraint* dof6;
            btFixedConstraint*       fixed;
            btPoint2PointConstraint* point;
            btMultiBodyPoint2Point*  point_multi;
        };
    };

    struct physics_entity
    {
        e_entity_type type = ENTITY_NULL;

        union {
            rigid_body_entity rb;
            mutli_body_entity mb;
            constraint_entity constraint;
        };

        btDefaultMotionState* default_motion_state;
        btCollisionShape*     collision_shape;
        btCompoundShape*      compound_shape;
        u32                   num_base_compound_shapes;

        u32 group;
        u32 mask;

        physics_entity(){};
        ~physics_entity(){};
    };

    struct bullet_systems
    {
        btDefaultCollisionConfiguration* collision_config;
        btCollisionDispatcher*           dispatcher;
        btBroadphaseInterface*           olp_cache;
        btConstraintSolver*              solver;
        btDynamicsWorld*                 dynamics_world;
    };

    struct bullet_objects
    {
        bullet_objects(){};
        ~bullet_objects(){};
    };

    struct detach_callback
    {
        void (*attach_function)(void* user_data, s32 attach_index);
        void* p_attach_user_data;
        s32   attach_shape_index;
        u32   call_attach;
    };

    struct readable_data
    {
        readable_data()
        {
            b_paused = 0;
        }

        a_u32                                   b_paused;
        pen::multi_buffer<mat4*, 2>             output_matrices;
        pen::multi_buffer<maths::transform*, 2> output_transforms;
    };

    extern readable_data g_readable_data;

    void physics_update(f32 dt);
    void physics_initialise();
    void physics_shutdown();

    btRigidBody* create_rb_internal(physics_entity& entity, const rigid_body_params& params, u32 ghost,
                                    btCollisionShape* p_existing_shape = NULL);

    void add_rb_internal(const rigid_body_params& params, u32 resource_slot, bool ghost = false);
    void add_compound_rb_internal(const compound_rb_cmd& cmd, u32 resource_slot);
    void add_compound_shape_internal(const compound_rb_params& params, u32 resource_slot);
    void add_dof6_internal(const constraint_params& params, u32 resource_slot, btRigidBody* rb, btRigidBody* fixed_body);
    void add_hinge_internal(const constraint_params& params, u32 resource_slot);
    void add_constraint_internal(const constraint_params& params, u32 resource_slot);
    void add_p2p_constraint_internal(const add_p2p_constraint_params& cmd, u32 resource_slot);

    void set_linear_velocity_internal(const set_v3_params& cmd);
    void set_angular_velocity_internal(const set_v3_params& cmd);
    void set_linear_factor_internal(const set_v3_params& cmd);
    void set_angular_factor_internal(const set_v3_params& cmd);
    void set_transform_internal(const set_transform_params& cmd);
    void set_gravity_internal(const set_v3_params& cmd);
    void set_friction_internal(const set_float_params& cmd);
    void set_hinge_motor_internal(const set_v3_params& cmd);
    void set_button_motor_internal(const set_v3_params& cmd);
    void set_multi_joint_motor_internal(const set_multi_v3_params& cmd);
    void set_multi_joint_pos_internal(const set_multi_v3_params& cmd);
    void set_multi_joint_limit_internal(const set_multi_v3_params& cmd);
    void set_multi_base_velocity_internal(const set_multi_v3_params& cmd);
    void set_multi_base_pos_internal(const set_multi_v3_params& cmd);
    void set_group_internal(const set_group_params& cmd);
    void set_damping_internal(const set_v3_params& cmd);
    void set_p2p_constraint_pos_internal(const set_v3_params& cmd);

    void sync_rigid_bodies_internal(const sync_rb_params& cmd);
    void sync_rigid_body_velocity_internal(const sync_rb_params& cmd);
    void sync_compound_multi_internal(const sync_compound_multi_params& cmd);
    void attach_rb_to_compound_internal(const attach_to_compound_params& params);

    mat4 get_rb_start_matrix(u32 rb_index);

    void add_to_world_internal(u32 entity_index);
    void remove_from_world_internal(u32 entity_index);
    void release_entity_internal(u32 entity_index);

    cast_result cast_ray_internal(const ray_cast_params& rcp);
    cast_result cast_sphere_internal(const sphere_cast_params& ccp);
    void        contact_test_internal(const contact_test_params& ctp);

    void add_central_force(const set_v3_params& cmd);
    void add_central_impulse(const set_v3_params& cmd);
    void add_force(const set_v3_v3_params& cmd);

} // namespace physics
