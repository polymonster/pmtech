// physics_bullet.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "physics_bullet.h"
#include "console.h"
#include "pen_string.h"
#include "slot_resource.h"
#include "timer.h"

namespace physics
{
    pen_inline btVector3 from_vec3(const vec3f& v3)
    {
        return btVector3(v3.x, v3.y, v3.z);
    }

    pen_inline vec3f from_btvector(const btVector3& bt)
    {
        return vec3f(bt.getX(), bt.getY(), bt.getZ());
    }

    pen_inline quat from_btquat(const btQuaternion& bt)
    {
        quat q;
        q.x = bt.getX();
        q.y = bt.getY();
        q.z = bt.getZ();
        q.w = bt.getW();

        return q;
    }

    pen_inline maths::transform from_bttransform(const btTransform& bt)
    {
        maths::transform t;
        t.translation = from_btvector(bt.getOrigin());
        t.rotation = from_btquat(bt.getRotation());
        t.scale = vec3f::one();
        return t;
    }

    readable_data                        g_readable_data;
    static bullet_systems                s_bullet_systems;
    static pen::res_pool<physics_entity> s_entities;

    btTransform get_bttransform(const vec3f& p, const quat& q)
    {
        btTransform trans;
        trans.setIdentity();
        trans.setOrigin(btVector3(btScalar(p.x), btScalar(p.y), btScalar(p.z)));

        btQuaternion bt_quat;
        memcpy(&bt_quat, &q, sizeof(quat));
        trans.setRotation(bt_quat);

        return trans;
    }

    btTransform get_bttransform_from_params(const rigid_body_params& params)
    {
        btTransform trans;
        trans.setIdentity();
        trans.setOrigin(btVector3(btScalar(params.position.x), btScalar(params.position.y), btScalar(params.position.z)));

        btQuaternion bt_quat;
        memcpy(&bt_quat, &params.rotation, sizeof(quat));
        trans.setRotation(bt_quat);

        return trans;
    }

    btCollisionShape* create_collision_shape(physics_entity& entity, const rigid_body_params& params,
                                             const compound_rb_params* p_compound = NULL)
    {
        btCollisionShape* shape = NULL;

        switch (params.shape)
        {
            case physics::e_shape::box:
                shape = new btBoxShape(
                    btVector3(btScalar(params.dimensions.x), btScalar(params.dimensions.y), btScalar(params.dimensions.z)));
                break;
            case physics::e_shape::cylinder:
                if (params.shape_up_axis == e_up_axis::x)
                {
                    shape = new btCylinderShapeX(btVector3(btScalar(params.dimensions.x), btScalar(params.dimensions.y),
                                                           btScalar(params.dimensions.z)));
                }
                else if (params.shape_up_axis == e_up_axis::z)
                {
                    shape = new btCylinderShapeZ(btVector3(btScalar(params.dimensions.x), btScalar(params.dimensions.y),
                                                           btScalar(params.dimensions.z)));
                }
                else
                {
                    shape = new btCylinderShape(btVector3(btScalar(params.dimensions.x), btScalar(params.dimensions.y),
                                                          btScalar(params.dimensions.z)));
                }
                break;
            case physics::e_shape::capsule:
                if (params.shape_up_axis == e_up_axis::x)
                {
                    shape = new btCapsuleShapeX(btScalar(params.dimensions.y), btScalar(params.dimensions.x));
                }
                else if (params.shape_up_axis == e_up_axis::z)
                {
                    shape = new btCapsuleShapeZ(btScalar(params.dimensions.x), btScalar(params.dimensions.z));
                }
                else
                {
                    shape = new btCapsuleShape(btScalar(params.dimensions.x), btScalar(params.dimensions.y));
                }
                break;
            case physics::e_shape::cone:
                if (params.shape_up_axis == e_up_axis::x)
                {
                    shape = new btConeShapeX(btScalar(params.dimensions.y), btScalar(params.dimensions.x));
                }
                else if (params.shape_up_axis == e_up_axis::z)
                {
                    shape = new btConeShapeZ(btScalar(params.dimensions.x), btScalar(params.dimensions.z));
                }
                else
                {
                    shape = new btConeShape(btScalar(params.dimensions.x), btScalar(params.dimensions.y));
                }
                break;
            case physics::e_shape::hull:
                shape = new btConvexHullShape(params.mesh_data.vertices, params.mesh_data.num_floats / 3, 12);
                break;
            case physics::e_shape::mesh:
            {
                u32 num_tris = params.mesh_data.num_indices / 3;

                btTriangleIndexVertexArray* mesh = new btTriangleIndexVertexArray(
                    num_tris, (s32*)params.mesh_data.indices, sizeof(u32) * 3, params.mesh_data.num_floats / 3,
                    params.mesh_data.vertices, sizeof(f32) * 3);

                btBvhTriangleMeshShape* concave_mesh = new btBvhTriangleMeshShape(mesh, true);
                shape = concave_mesh;
            }
            break;
            case physics::e_shape::compound:
            {
                if (p_compound)
                {
                    u32 num_shapes = p_compound->num_shapes;

                    btCompoundShape* compound = new btCompoundShape(true);

                    btTransform basetrans = get_bttransform_from_params(p_compound->base);

                    for (u32 s = 0; s < num_shapes; ++s)
                    {
                        btCollisionShape* p_sub_shape = create_collision_shape(entity, p_compound->rb[s]);

                        btTransform trans = get_bttransform_from_params(p_compound->rb[s]);

                        trans = trans * basetrans.inverse();

                        compound->addChildShape(trans, p_sub_shape);
                    }

                    shape = compound;
                }
            }
            break;
            case physics::e_shape::sphere:
                shape = new btSphereShape(params.dimensions.x);
                break;
            default:
                PEN_ASSERT_MSG(0, "unimplemented physics shape");
                break;
        }

        if (shape)
            entity.collision_shape = shape;

        return shape;
    }

    btRigidBody* create_rb_internal(physics_entity& entity, const rigid_body_params& params, u32 ghost,
                                    btCollisionShape* p_existing_shape)
    {
        // create box shape at position and orientation specified in the command
        btCollisionShape* shape = nullptr;
        btTransform       shape_transform;

        if (p_existing_shape)
        {
            shape = p_existing_shape;
        }
        else
        {
            shape = create_collision_shape(entity, params);
        }

        shape_transform.setIdentity();
        shape_transform.setOrigin(
            btVector3(btScalar(params.position.x), btScalar(params.position.y), btScalar(params.position.z)));

        btQuaternion bt_quat;
        memcpy(&bt_quat, &params.rotation, sizeof(quat));
        shape_transform.setRotation(bt_quat);

        // create rigid body
        btScalar mass(params.mass);

        bool dynamic = (mass != 0.0f);

        btVector3 local_inertia(0, 0, 0);
        if (dynamic && shape)
        {
            shape->calculateLocalInertia(mass, local_inertia);
        }

        // using motion state is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
        btDefaultMotionState* motion_state = new btDefaultMotionState(shape_transform);
        entity.default_motion_state = motion_state;

        btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state, shape, local_inertia);

        btRigidBody* body = new btRigidBody(rb_info);

        if (params.create_flags & e_create_flags::kinematic)
        {
            body->setCollisionFlags(btCollisionObject::CF_KINEMATIC_OBJECT);
        }

        body->setContactProcessingThreshold(BT_LARGE_FLOAT);
        body->setActivationState(DISABLE_DEACTIVATION);

        if (!ghost)
        {
            s_bullet_systems.dynamics_world->addRigidBody(body, params.group, params.mask);
        }

        return body;
    }

    void physics_initialise()
    {
        s_entities.init(1024);

        g_readable_data.output_matrices._data[0] = nullptr;
        g_readable_data.output_matrices._data[1] = nullptr;
        g_readable_data.output_transforms._data[0] = nullptr;
        g_readable_data.output_transforms._data[1] = nullptr;

        s_bullet_systems.collision_config = new btDefaultCollisionConfiguration();
        s_bullet_systems.dispatcher = new btCollisionDispatcher(s_bullet_systems.collision_config);
        s_bullet_systems.olp_cache = new btAxisSweep3(btVector3(-50.0f, -50.0f, -50.0f), btVector3(50.0f, 50.0f, 50.0f));
        s_bullet_systems.solver = new btSequentialImpulseConstraintSolver;
        s_bullet_systems.dynamics_world =
            new btDiscreteDynamicsWorld(s_bullet_systems.dispatcher, s_bullet_systems.olp_cache, s_bullet_systems.solver,
                                        s_bullet_systems.collision_config);

        s_bullet_systems.dynamics_world->setGravity(btVector3(0, -10, 0));
    }

    void update_output_matrices()
    {
        mat4*&             bb_mats = g_readable_data.output_matrices.backbuffer();
        maths::transform*& bb_transforms = g_readable_data.output_transforms.backbuffer();

        u32 num = sb_count(bb_mats);

        for (u32 i = 0; i < s_entities._capacity; i++)
        {
            if (i >= num)
            {
                ++num;
                sb_push(bb_mats, mat4::create_identity());
                sb_push(bb_transforms, maths::transform());
            }

            physics_entity& entity = s_entities.get(i);

            switch (entity.type)
            {
                case ENTITY_RIGID_BODY:
                {
                    btRigidBody* p_rb = entity.rb.rigid_body;
                    if (!p_rb)
                        continue;

                    btTransform rb_transform = p_rb->getWorldTransform();

                    btScalar _mm[16];

                    rb_transform.getOpenGLMatrix(_mm);

                    for (s32 m = 0; m < 16; ++m)
                        bb_mats[i].m[m] = _mm[m];

                    bb_mats[i].transpose();
                    bb_transforms[i] = from_bttransform(rb_transform);
                }
                break;

                case ENTITY_COMPOUND_RIGID_BODY:
                {
                    btCompoundShape* p_compound = entity.compound_shape;
                    btRigidBody*     p_rb = entity.rb.rigid_body;

                    btTransform rb_transform = p_rb->getWorldTransform();

                    btScalar _mm[16];

                    rb_transform.getOpenGLMatrix(_mm);

                    for (s32 m = 0; m < 16; ++m)
                        bb_mats[i].m[m] = _mm[m];

                    bb_mats[i].transpose();

                    if (p_compound)
                    {
                        u32 num_shapes = p_compound->getNumChildShapes();
                        for (u32 j = 0; j < num_shapes; ++j)
                        {
                            if (p_rb)
                            {
                                btTransform       base = p_rb->getWorldTransform();
                                btTransform       child = p_compound->getChildTransform(j);
                                btCollisionShape* shape = p_compound->getChildShape(j);
                                u32               ph = shape->getUserIndex();

                                if (!is_valid(ph))
                                    continue;

                                btTransform child_world = base * child;

                                btScalar _mm[16];

                                child_world.getOpenGLMatrix(_mm);

                                for (s32 m = 0; m < 16; ++m)
                                    bb_mats[ph].m[m] = _mm[m];

                                bb_mats[ph].transpose();
                                bb_transforms[ph] = from_bttransform(child_world);
                            }
                        }
                    }
                }
                break;

                default:
                    break;
            }
        }

        g_readable_data.output_matrices.swap_buffers();
        g_readable_data.output_transforms.swap_buffers();
    }

    void physics_update(f32 dt)
    {
        // step
        if (!g_readable_data.b_paused)
            s_bullet_systems.dynamics_world->stepSimulation(dt);

        // update mats
        update_output_matrices();
    }

    void add_rb_internal(const rigid_body_params& params, u32 resource_slot, bool ghost)
    {
        s_entities.grow(resource_slot);
        physics_entity& entity = s_entities.get(resource_slot);

        // add the body to the dynamics world
        btRigidBody* rb = create_rb_internal(entity, params, ghost);
        rb->setUserIndex(resource_slot);

        entity.rb.rigid_body = rb;
        entity.rb.rigid_body_in_world = !ghost;

        entity.group = params.group;
        entity.mask = params.mask;

        PEN_ASSERT(rb);

        entity.type = ENTITY_RIGID_BODY;
    }

    void add_compound_rb_internal(const compound_rb_cmd& cmd, u32 resource_slot)
    {
        s_entities.grow(resource_slot);
        physics_entity& entity = s_entities.get(resource_slot);

        btCollisionShape* col = create_collision_shape(entity, cmd.params.base, &cmd.params);
        btCompoundShape*  compound = (btCompoundShape*)col;

        u32 num = cmd.params.num_shapes;
        for (u32 i = 0; i < num; ++i)
        {
            u32 ch = cmd.children_handles[i];
            s_entities.grow(ch);
            compound->getChildShape(i)->setUserIndex(ch);
        }

        entity.compound_shape = compound;
        entity.num_base_compound_shapes = cmd.params.num_shapes;

        entity.rb.rigid_body = create_rb_internal(entity, cmd.params.base, 0, compound);
        entity.rb.rigid_body->setUserIndex(resource_slot);

        entity.rb.rigid_body_in_world = 1;
        entity.group = cmd.params.base.group;
        entity.mask = cmd.params.base.mask;

        entity.type = ENTITY_COMPOUND_RIGID_BODY;
    }

    void add_compound_shape_internal(const compound_rb_params& params, u32 resource_slot)
    {
        s_entities.grow(resource_slot);
        physics_entity& entity = s_entities.get(resource_slot);

        btCollisionShape* col = create_collision_shape(entity, params.base, &params);
        btCompoundShape*  compound = (btCompoundShape*)col;

        entity.compound_shape = compound;
    }

    void add_dof6_internal(const constraint_params& params, u32 resource_slot, btRigidBody* rb, btRigidBody* fixed_body)
    {
        s_entities.grow(resource_slot);
        physics_entity& entity = s_entities.get(resource_slot);

        // reference frames are identity
        btTransform frameInA, frameInB;
        frameInA.setIdentity();
        frameInB.setIdentity();

        rb->setDamping(params.linear_damping, params.angular_damping);

        btGeneric6DofConstraint* dof6 = nullptr;
        if (fixed_body && rb)
            dof6 = new btGeneric6DofConstraint(*fixed_body, *rb, frameInA, frameInB, true);
        else if (rb)
            dof6 = new btGeneric6DofConstraint(*rb, frameInB, false);

        if (!dof6)
            return;

        dof6->setLinearLowerLimit(
            btVector3(params.lower_limit_translation.x, params.lower_limit_translation.y, params.lower_limit_translation.z));
        dof6->setLinearUpperLimit(
            btVector3(params.upper_limit_translation.x, params.upper_limit_translation.y, params.upper_limit_translation.z));

        dof6->setAngularLowerLimit(
            btVector3(params.lower_limit_rotation.x, params.lower_limit_rotation.y, params.lower_limit_rotation.z));
        dof6->setAngularUpperLimit(
            btVector3(params.upper_limit_rotation.x, params.upper_limit_rotation.y, params.upper_limit_rotation.z));

        s_bullet_systems.dynamics_world->addConstraint(dof6);
        entity.constraint.dof6 = dof6;
    }

    void add_hinge_internal(const constraint_params& params, u32 resource_slot)
    {
        PEN_ASSERT(params.rb_indices[0] > -1);

        s_entities.grow(resource_slot);
        physics_entity& entity = s_entities.get(resource_slot);

        btRigidBody* rb = s_entities.get(params.rb_indices[0]).rb.rigid_body;

        btHingeConstraint* hinge = new btHingeConstraint(*rb, btVector3(params.pivot.x, params.pivot.y, params.pivot.z),
                                                         btVector3(params.axis.x, params.axis.y, params.axis.z));
        hinge->setLimit(params.lower_limit_rotation.x, params.upper_limit_rotation.x);

        s_bullet_systems.dynamics_world->addConstraint(hinge);

        entity.constraint.type = e_constraint::hinge;
        entity.constraint.hinge = hinge;
    }

    void add_constraint_internal(const constraint_params& params, u32 resource_slot)
    {
        s_entities.grow(resource_slot);
        physics_entity& entity = s_entities.get(resource_slot);
        entity.type = ENTITY_CONSTRAINT;

        switch (params.type)
        {
            case e_constraint::p2p:
            {
                add_p2p_constraint_params p2p;
                p2p.entity_index = params.rb_indices[0];
                p2p.position = params.pivot;
                add_p2p_constraint_internal(p2p, resource_slot);
            }
            break;

            case e_constraint::hinge:
            {
                add_hinge_internal(params, resource_slot);
            }
            break;

            case e_constraint::dof6:
            {
                btRigidBody* p_rb = nullptr;
                btRigidBody* p_fixed = nullptr;

                if (params.rb_indices[0] > 0)
                    p_rb = s_entities.get(params.rb_indices[0]).rb.rigid_body;

                if (params.rb_indices[1] > 0)
                    p_fixed = s_entities.get(params.rb_indices[1]).rb.rigid_body;

                PEN_ASSERT(p_rb || p_fixed);

                add_dof6_internal(params, resource_slot, p_rb, p_fixed);
            }
            break;

            default:
                PEN_ASSERT_MSG(0, "unimplemented add constraint");
                break;
        }
    }

    void add_central_force(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_entities.get(cmd.object_index).rb.rigid_body->applyCentralForce(bt_v3);
        s_entities.get(cmd.object_index).rb.rigid_body->activate(ACTIVE_TAG);
    }

    void add_central_impulse(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_entities.get(cmd.object_index).rb.rigid_body->applyCentralImpulse(bt_v3);
        s_entities.get(cmd.object_index).rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_linear_velocity_internal(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_entities.get(cmd.object_index).rb.rigid_body->setLinearVelocity(bt_v3);
        s_entities.get(cmd.object_index).rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_angular_velocity_internal(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_entities.get(cmd.object_index).rb.rigid_body->setAngularVelocity(bt_v3);
        s_entities.get(cmd.object_index).rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_linear_factor_internal(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_entities.get(cmd.object_index).rb.rigid_body->setLinearFactor(bt_v3);
        s_entities.get(cmd.object_index).rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_angular_factor_internal(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_entities.get(cmd.object_index).rb.rigid_body->setAngularFactor(bt_v3);
        s_entities.get(cmd.object_index).rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_transform_internal(const set_transform_params& cmd)
    {
        btVector3    bt_v3;
        btQuaternion bt_quat;

        memcpy(&bt_v3, &cmd.position, sizeof(vec3f));
        memcpy(&bt_quat, &cmd.rotation, sizeof(quat));

        btTransform bt_trans;
        bt_trans.setOrigin(bt_v3);
        bt_trans.setRotation(bt_quat);

        btRigidBody* rb = s_entities.get(cmd.object_index).rb.rigid_body;

        if (rb)
        {
            if (rb->getCollisionFlags() & btCollisionObject::CF_KINEMATIC_OBJECT)
            {
                rb->getMotionState()->setWorldTransform(bt_trans);
            }
            else
            {
                rb->getMotionState()->setWorldTransform(bt_trans);
                rb->setCenterOfMassTransform(bt_trans);
            }
        }
    }

    void set_gravity_internal(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_entities.get(cmd.object_index).rb.rigid_body->setGravity(bt_v3);
        s_entities.get(cmd.object_index).rb.rigid_body->activate();
    }

    void set_friction_internal(const set_float_params& cmd)
    {
        s_entities.get(cmd.object_index).rb.rigid_body->setFriction(cmd.data);
    }

    void set_hinge_motor_internal(const set_v3_params& cmd)
    {
        btHingeConstraint* p_hinge = s_entities.get(cmd.object_index).constraint.hinge;
        p_hinge->enableAngularMotor(cmd.data.x == 0.0f ? false : true, cmd.data.y, cmd.data.z);
    }

    void set_button_motor_internal(const set_v3_params& cmd)
    {
        btGeneric6DofConstraint* dof6 = s_entities.get(cmd.object_index).constraint.dof6;

        btTranslationalLimitMotor* motor = dof6->getTranslationalLimitMotor();
        motor->m_enableMotor[1] = cmd.data.x == 0.0f ? false : true;
        motor->m_targetVelocity = btVector3(0.0f, cmd.data.y, 0.0f);
        motor->m_maxMotorForce = btVector3(0.0f, cmd.data.z, 0.0f);

        s_entities.get(cmd.object_index).rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_multi_joint_motor_internal(const set_multi_v3_params& cmd)
    {
        btMultiBodyJointMotor* p_joint_motor = s_entities.get(cmd.multi_index).mb.joint_motors.at(cmd.link_index);

        p_joint_motor->setVelocityTarget(cmd.data.x);
        p_joint_motor->setMaxAppliedImpulse(cmd.data.y);
    }

    void set_multi_joint_pos_internal(const set_multi_v3_params& cmd)
    {
        btMultiBody* p_multi = s_entities.get(cmd.multi_index).mb.multi_body;

        if (1)
        {
            f32 multi_dof_pos = cmd.data.x;
            p_multi->setJointPosMultiDof(cmd.link_index, &multi_dof_pos);
        }
        else
        {
            p_multi->setJointPos(cmd.link_index, cmd.data.x);
        }
    }

    void set_multi_joint_limit_internal(const set_multi_v3_params& cmd)
    {
    }

    void set_multi_base_velocity_internal(const set_multi_v3_params& cmd)
    {
        btMultiBody* p_multi = s_entities.get(cmd.multi_index).mb.multi_body;

        p_multi->setBaseVel(btVector3(cmd.data.x, cmd.data.y, cmd.data.z));
    }

    void set_multi_base_pos_internal(const set_multi_v3_params& cmd)
    {
        btMultiBody* p_multi = s_entities.get(cmd.multi_index).mb.multi_body;

        p_multi->setBasePos(btVector3(cmd.data.x, cmd.data.y, cmd.data.z));
    }

    void sync_rigid_bodies_internal(const sync_rb_params& cmd)
    {
        if (s_entities.get(cmd.master).type == ENTITY_RIGID_BODY)
        {
            btRigidBody* p_rb = s_entities.get(cmd.master).rb.rigid_body;
            btRigidBody* p_rb_slave = s_entities.get(cmd.slave).rb.rigid_body;

            btTransform master = p_rb->getWorldTransform();
            p_rb_slave->setWorldTransform(master);
        }

        if (s_entities.get(cmd.master).type == ENTITY_MULTI_BODY && cmd.link_index != -1)
        {
            btMultiBody* p_mb = s_entities.get(cmd.master).mb.multi_body;
            btRigidBody* p_rb_slave = s_entities.get(cmd.slave).rb.rigid_body;

            btTransform master = p_mb->getLink(cmd.link_index).m_collider->getWorldTransform();
            p_rb_slave->setWorldTransform(master);
        }
    }

    void sync_rigid_body_velocity_internal(const sync_rb_params& cmd)
    {
        btVector3 master_vel = s_entities.get(cmd.master).rb.rigid_body->getAngularVelocity();

        s_entities.get(cmd.slave).rb.rigid_body->setAngularVelocity(master_vel);
    }

    void sync_compound_multi_internal(const sync_compound_multi_params& cmd)
    {
        btMultiBody*     p_multi = s_entities.get(cmd.multi_index).mb.multi_body;
        btCompoundShape* p_compound = s_entities.get(cmd.compound_index).compound_shape;
        btRigidBody*     p_rb = s_entities.get(cmd.compound_index).rb.rigid_body;

        u32 num_base_compound_shapes = s_entities.get(cmd.compound_index).num_base_compound_shapes;

        PEN_ASSERT(p_multi);
        PEN_ASSERT(p_compound);
        PEN_ASSERT(p_rb);

        u32 num_links = p_multi->getNumLinks();

        btTransform rb_trans = p_rb->getWorldTransform();
        p_multi->setBasePos(rb_trans.getOrigin());

        btTransform multi_base_trans = p_multi->getBaseCollider()->getWorldTransform();

        btAlignedObjectArray<btTransform> current_offsets;

        // get current attached shapes offsets
        btTransform base_cur = p_compound->getChildTransform(0);

        u32 num_shapes = p_compound->getNumChildShapes();
        for (u32 attached = num_base_compound_shapes; attached < num_shapes; ++attached)
        {
            btTransform child_cur = p_compound->getChildTransform(attached);

            current_offsets.push_back(base_cur.inverse() * child_cur);
        }

        u32 compound_shape_itr = 0;
        for (u32 link = 0; link < num_links; ++link)
        {
            if (p_multi->getLink(link).m_collider)
            {
                btTransform link_world_trans = p_multi->getLink(link).m_collider->getWorldTransform();

                btTransform link_local_trans = multi_base_trans.inverse() * link_world_trans;

                p_compound->updateChildTransform(compound_shape_itr, link_local_trans);

                compound_shape_itr++;
            }
        }

        // transform current attached by the base
        btTransform base_new = p_compound->getChildTransform(0);

        u32 offset_index = 0;
        for (u32 attached = num_base_compound_shapes; attached < num_shapes; ++attached)
        {
            p_compound->updateChildTransform(attached, base_new * current_offsets.at(offset_index));

            offset_index++;
        }
    }

    void add_p2p_constraint_internal(const add_p2p_constraint_params& cmd, u32 resource_slot)
    {
        physics_entity* entity = &s_entities.get(resource_slot);

        btRigidBody* rb = s_entities.get(cmd.entity_index).rb.rigid_body;

        btVector3 constrain_pos = btVector3(cmd.position.x, cmd.position.y, cmd.position.z);

        entity->type = ENTITY_CONSTRAINT;

        if (rb)
        {
            rb->setActivationState(DISABLE_DEACTIVATION);

            btVector3 local_pivot = rb->getCenterOfMassTransform().inverse() * constrain_pos;

            btPoint2PointConstraint* p2p = new btPoint2PointConstraint(*rb, local_pivot);

            s_bullet_systems.dynamics_world->addConstraint(p2p, true);

            btScalar mousePickClamping = 0.f;
            p2p->m_setting.m_impulseClamp = mousePickClamping;

            // very weak constraint for picking
            p2p->m_setting.m_tau = 10.0f;

            entity->constraint.point = p2p;
            entity->constraint.type = e_constraint::p2p;
        }
    }

    void set_p2p_constraint_pos_internal(const set_v3_params& cmd)
    {
        physics_entity& pe = s_entities.get(cmd.object_index);

        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));

        switch (pe.constraint.type)
        {
            case e_constraint::p2p:
                pe.constraint.point->setPivotB(bt_v3);
                break;
            case e_constraint::p2p_multi:
                pe.constraint.point_multi->setPivotInB(bt_v3);
                break;
            default:
                return;
        }
    }

    void set_damping_internal(const set_v3_params& cmd)
    {
        btRigidBody* rb = s_entities.get(cmd.object_index).rb.rigid_body;

        rb->setDamping(cmd.data.x, cmd.data.y);
    }

    void set_group_internal(const set_group_params& cmd)
    {
        btRigidBody* rb = s_entities.get(cmd.object_index).rb.rigid_body;

        if (rb)
        {
            s_bullet_systems.dynamics_world->removeRigidBody(rb);
            s_bullet_systems.dynamics_world->addRigidBody(rb, cmd.group, cmd.mask);
        }
    }

    mat4 get_rb_start_matrix(u32 rb_index)
    {
        return mat4::create_identity();
    }

    void attach_rb_to_compound_internal(const attach_to_compound_params& params)
    {
        // todo test this function still works after refactor

        physics_entity& compound = s_entities.get(params.compound);
        physics_entity& pe = s_entities.get(params.rb);

        rigid_body_entity rb = pe.rb;

        if (compound.compound_shape && pe.type == ENTITY_RIGID_BODY)
        {
            btTransform base = compound.rb.rigid_body->getWorldTransform();

            if (params.detach_index != -1)
            {
                // to callback later
                rb.p_attach_user_data = params.p_user_data;
                rb.attach_function = params.function;
                rb.attach_shape_index = -1;
                rb.call_attach = 1;

                rb.rigid_body_in_world = 1;

                btTransform compound_child = compound.compound_shape->getChildTransform(params.detach_index);

                compound.compound_shape->removeChildShapeByIndex(params.detach_index);

                s_bullet_systems.dynamics_world->addRigidBody(rb.rigid_body, pe.group, pe.mask);

                pe.type = ENTITY_RIGID_BODY;

                rb.rigid_body->setWorldTransform(base * compound_child);
            }
            else
            {
                // to callback later
                rb.p_attach_user_data = params.p_user_data;
                rb.attach_function = params.function;
                rb.attach_shape_index = compound.compound_shape->getNumChildShapes();
                rb.call_attach = 1;

                btCompoundShape* compound2 = new btCompoundShape();

                rb.rigid_body_in_world = 0;

                btTransform new_child = rb.rigid_body->getWorldTransform();

                if (0)
                {
                    btScalar    masses[3] = {1, 1, 1};
                    btTransform principal;
                    btVector3   inertia;
                    compound.compound_shape->calculatePrincipalAxisTransform(masses, principal, inertia);

                    for (int i = 0; i < compound.compound_shape->getNumChildShapes(); i++)
                        compound2->addChildShape(compound.compound_shape->getChildTransform(i) * principal.inverse(),
                                                 compound.compound_shape->getChildShape(i));
                }
                else
                {
                    btTransform offset = base.inverse() * new_child;

                    u32 ci = compound.compound_shape->getNumChildShapes();
                    compound.compound_shape->addChildShape(offset, rb.rigid_body->getCollisionShape());
                    compound.compound_shape->getChildShape(ci)->setUserIndex(params.rb);
                }

                pe.type = ENTITY_COMPOUND_RIGID_BODY_CHILD;

                //s_bullet_systems.dynamics_world->removeRigidBody(compound.rb.rigid_body);
                s_bullet_systems.dynamics_world->removeRigidBody(rb.rigid_body);
            }
        }
    }

    void release_entity_internal(u32 entity_index)
    {
        if (s_entities.get(entity_index).type == ENTITY_RIGID_BODY)
        {
            remove_from_world_internal(entity_index);
            delete s_entities.get(entity_index).rb.rigid_body;
            s_entities.get(entity_index).rb.rigid_body = nullptr;
        }
        else if (s_entities.get(entity_index).type == ENTITY_CONSTRAINT)
        {
            btTypedConstraint* generic_constraint = s_entities.get(entity_index).constraint.generic;

            if (generic_constraint)
                s_bullet_systems.dynamics_world->removeConstraint(generic_constraint);

            delete generic_constraint;
            s_entities.get(entity_index).constraint.generic = nullptr;
        }

        s_entities.get(entity_index).type = ENTITY_NULL;
    }

    void remove_from_world_internal(u32 entity_index)
    {
        s_bullet_systems.dynamics_world->removeRigidBody(s_entities.get(entity_index).rb.rigid_body);
    }

    void add_to_world_internal(u32 entity_index)
    {
        physics_entity& pe = s_entities.get(entity_index);
        s_bullet_systems.dynamics_world->addRigidBody(pe.rb.rigid_body, pe.group, pe.mask);
    }

    cast_result cast_ray_internal(const ray_cast_params& rcp)
    {
        btVector3 from = from_vec3(rcp.start);
        btVector3 to = from_vec3(rcp.end);

        btCollisionWorld::ClosestRayResultCallback ray_callback(from, to);
        ray_callback.m_collisionFilterMask = rcp.mask;
        ray_callback.m_collisionFilterGroup = rcp.group;

        cast_result rcr;
        rcr.user_data = rcp.user_data;

        rcr.physics_handle = -1;
        s_bullet_systems.dynamics_world->rayTest(from, to, ray_callback);
        if (ray_callback.hasHit())
        {
            rcr.point = from_btvector(ray_callback.m_hitPointWorld);
            rcr.normal = from_btvector(ray_callback.m_hitNormalWorld);

            btRigidBody* body = (btRigidBody*)btRigidBody::upcast(ray_callback.m_collisionObject);

            if (body)
            {
                rcr.physics_handle = body->getUserIndex();
                rcr.set = true;
            }
        }

        if (rcp.callback)
            rcp.callback(rcr);

        return rcr;
    }

    cast_result cast_sphere_internal(const sphere_cast_params& scp)
    {
        btTransform from = get_bttransform(scp.from, quat());
        btTransform to = get_bttransform(scp.to, quat());

        btVector3 vfrom = from_vec3(scp.from);
        btVector3 vto = from_vec3(scp.to);

        btSphereShape shape = btSphereShape(btScalar(scp.dimension.x));

        btCollisionWorld::ClosestConvexResultCallback cast_callback =
            btCollisionWorld::ClosestConvexResultCallback(vfrom, vto);
        cast_callback.m_collisionFilterMask = scp.mask;
        cast_callback.m_collisionFilterGroup = scp.group;

        s_bullet_systems.dynamics_world->convexSweepTest((btConvexShape*)&shape, from, to, cast_callback);

        cast_result sr;
        sr.user_data = scp.user_data;

        sr.physics_handle = -1;
        if (cast_callback.hasHit())
        {
            btRigidBody* body = (btRigidBody*)btRigidBody::upcast(cast_callback.m_hitCollisionObject);

            if (body)
            {
                sr.physics_handle = body->getUserIndex();
                sr.set = true;
            }

            sr.point = from_btvector(cast_callback.m_hitPointWorld);
            sr.normal = from_btvector(cast_callback.m_hitNormalWorld);
        }

        if (scp.callback)
            scp.callback(sr);

        return sr;
    }

    class contact_processor : public btCollisionWorld::ContactResultCallback
    {
      public:
        btRigidBody*         ref_rb;
        contact_test_results ctr;
        u32                  ref_entity;

        btScalar addSingleResult(btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
                                 const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1)
        {
            contact c;

            if (ref_rb == colObj0Wrap->getCollisionObject())
            {
                c.group = colObj1Wrap->getCollisionObject()->getBroadphaseHandle()->m_collisionFilterGroup;
                c.mask = colObj1Wrap->getCollisionObject()->getBroadphaseHandle()->m_collisionFilterMask;
                c.physics_handle = colObj1Wrap->getCollisionObject()->getUserIndex();

                c.pos = from_btvector(cp.m_positionWorldOnB);
            }
            else
            {
                c.group = colObj0Wrap->getCollisionObject()->getBroadphaseHandle()->m_collisionFilterGroup;
                c.mask = colObj0Wrap->getCollisionObject()->getBroadphaseHandle()->m_collisionFilterMask;
                c.physics_handle = colObj1Wrap->getCollisionObject()->getUserIndex();

                c.pos = from_btvector(cp.m_positionWorldOnA);
            }

            c.normal = from_btvector(cp.m_normalWorldOnB);

            sb_push(ctr.contacts, c);
            return 0.0f;
        }
    };

    void contact_test_internal(const contact_test_params& ctp)
    {
        btRigidBody* rb = s_entities.get(ctp.entity).rb.rigid_body;
        if (!rb)
            return;

        contact_processor cb;
        cb.ref_rb = rb;
        cb.ref_entity = ctp.entity;

        btCollisionObject* cobj = (btCollisionObject*)rb;

        s_bullet_systems.dynamics_world->contactTest(cobj, cb);

        ctp.callback(cb.ctr);
    }
} // namespace physics

#if PICKING_REFERENCE // reference
virtual void removePickingConstraint()
{
    if (m_pickedConstraint)
    {
        m_pickedBody->forceActivationState(m_savedState);
        m_pickedBody->activate();
        m_dynamicsWorld->removeConstraint(m_pickedConstraint);
        delete m_pickedConstraint;
        m_pickedConstraint = 0;
        m_pickedBody = 0;
    }
}
#endif
