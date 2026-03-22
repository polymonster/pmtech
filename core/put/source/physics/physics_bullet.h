// physics_bullet.h
// Copyright 2014 - 2023 Alex Dixon.
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

    extern readable_data g_readable_data;

    btRigidBody* create_rb_internal(physics_entity& entity, const rigid_body_params& params, u32 ghost,
                                    btCollisionShape* p_existing_shape = NULL);

    void add_dof6_internal(const constraint_params& params, u32 resource_slot, btRigidBody* rb, btRigidBody* fixed_body);

} // namespace physics
