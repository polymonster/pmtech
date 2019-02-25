#include "pen_string.h"
#include "slot_resource.h"
#include "timer.h"

#include "console.h"
#include "physics_internal.h"

namespace physics
{
#define SWAP_BUFFERS(b) b = (b + 1) % NUM_OUTPUT_BUFFERS
    inline btVector3 from_lw_vec3(const lw_vec3f& v3)
    {
        return btVector3(v3.x, v3.y, v3.z);
    }

    inline btVector3 from_vec3(const vec3f& v3)
    {
        return btVector3(v3.x, v3.y, v3.z);
    }

    inline vec3f from_btvector(const btVector3& bt)
    {
        return vec3f(bt.getX(), bt.getY(), bt.getZ());
    }

    readable_data g_readable_data;

    static trigger              s_triggers[MAX_TRIGGERS];
    static trigger_contact_data s_trigger_contacts[MAX_TRIGGERS];
    static u32                  s_num_triggers = 0;
    static bullet_systems       s_bullet_systems;
    static bullet_objects       s_bullet_objects;

    // todo merge these 2
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
            case physics::BOX:
                shape = new btBoxShape(
                    btVector3(btScalar(params.dimensions.x), btScalar(params.dimensions.y), btScalar(params.dimensions.z)));
                break;
            case physics::CYLINDER:
                if (params.shape_up_axis == UP_X)
                {
                    shape = new btCylinderShapeX(btVector3(btScalar(params.dimensions.x), btScalar(params.dimensions.y),
                                                           btScalar(params.dimensions.z)));
                }
                else if (params.shape_up_axis == UP_Z)
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
            case physics::CAPSULE:
                if (params.shape_up_axis == UP_X)
                {
                    shape = new btCapsuleShapeX(btScalar(params.dimensions.y), btScalar(params.dimensions.x));
                }
                else if (params.shape_up_axis == UP_Z)
                {
                    shape = new btCapsuleShapeZ(btScalar(params.dimensions.x), btScalar(params.dimensions.z));
                }
                else
                {
                    shape = new btCapsuleShape(btScalar(params.dimensions.x), btScalar(params.dimensions.y));
                }
                break;
            case physics::CONE:
                if (params.shape_up_axis == UP_X)
                {
                    shape = new btConeShapeX(btScalar(params.dimensions.y), btScalar(params.dimensions.x));
                }
                else if (params.shape_up_axis == UP_Z)
                {
                    shape = new btConeShapeZ(btScalar(params.dimensions.x), btScalar(params.dimensions.z));
                }
                else
                {
                    shape = new btConeShape(btScalar(params.dimensions.x), btScalar(params.dimensions.y));
                }
                break;
            case physics::HULL:
                shape = new btConvexHullShape(params.mesh_data.vertices, params.mesh_data.num_floats / 3, 12);
                break;
            case physics::MESH:
            {
                u32                         num_tris = params.mesh_data.num_indices / 3;
                btTriangleIndexVertexArray* mesh = new btTriangleIndexVertexArray(
                    num_tris, (s32*)params.mesh_data.indices, sizeof(u32) * 3, params.mesh_data.num_floats / 3,
                    params.mesh_data.vertices, sizeof(f32) * 3);
                btBvhTriangleMeshShape* concave_mesh = new btBvhTriangleMeshShape(mesh, true);
                shape = concave_mesh;
            }
            break;
            case physics::COMPOUND:
            {
                if (p_compound)
                {
                    u32 num_shapes = p_compound->num_shapes;

                    btCompoundShape* compound = new btCompoundShape(true);

                    for (u32 s = 0; s < num_shapes; ++s)
                    {
                        btCollisionShape* p_sub_shape = create_collision_shape(entity, p_compound->rb[s]);

                        btTransform trans = get_bttransform_from_params(p_compound->rb[s]);

                        compound->addChildShape(trans, p_sub_shape);
                    }

                    shape = compound;
                }
            }
            break;
            case physics::SPHERE:
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

        if (params.create_flags & CF_KINEMATIC)
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
        pen::memory_zero(&s_bullet_objects.entities, sizeof(physics_entity) * MAX_PHYSICS_RESOURCES);
        s_bullet_objects.num_entities = 0;

        s_bullet_systems.collision_config = new btDefaultCollisionConfiguration();

        /// use the default collision dispatcher. For parallel processing you can use a different dispatcher (see
        /// Extras/BulletMultiThreaded)
        s_bullet_systems.dispatcher = new btCollisionDispatcher(s_bullet_systems.collision_config);

        /// btDbvtBroadphase is a good general purpose broadphase. You can also try out btAxis3Sweep.
        s_bullet_systems.olp_cache = new btAxisSweep3(btVector3(-50.0f, -50.0f, -50.0f), btVector3(50.0f, 50.0f, 50.0f));

        /// the default constraint solver. For parallel processing you can use a different solver (see
        /// Extras/BulletMultiThreaded)
        s_bullet_systems.solver = new btSequentialImpulseConstraintSolver;
        s_bullet_systems.dynamics_world =
            new btDiscreteDynamicsWorld(s_bullet_systems.dispatcher, s_bullet_systems.olp_cache, s_bullet_systems.solver,
                s_bullet_systems.collision_config);

        s_bullet_systems.dynamics_world->setGravity(btVector3(0, -10, 0));
    }

    void update_output_matrices()
    {
        u32 bb = g_readable_data.current_ouput_backbuffer;

        for (u32 i = 0; i < s_bullet_objects.num_entities; i++)
        {
            physics_entity& entity = s_bullet_objects.entities[i];

            switch (entity.type)
            {
                case ENTITY_RIGID_BODY:
                {
                    btRigidBody* p_rb = entity.rb.rigid_body;

                    btTransform rb_transform = p_rb->getWorldTransform();

                    btScalar _mm[16];

                    rb_transform.getOpenGLMatrix(_mm);

                    for (s32 m = 0; m < 16; ++m)
                        g_readable_data.output_matrices[bb][i].m[m] = _mm[m];

                    g_readable_data.output_matrices[bb][i] = g_readable_data.output_matrices[bb][i].transposed();
                }
                break;

                default:
                    break;
            }
        }
    }

#if 0 // todo ressurect compounds
	void update_output_matrices( )
	{
		for (u32 i = 0; i < g_bullet_objects.num_entities; i++)
		{
			btCompoundShape* p_compound = g_bullet_objects.entities[i].compound_shape;

			if (p_compound)
			{
				g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][0] = g_readable_data.output_matrices[current_ouput_backbuffer][i];

				u32 num_shapes = p_compound->getNumChildShapes( );
				for (u32 j = 0; j < num_shapes; ++j)
				{
					if (p_rb)
					{
						btTransform base = p_rb->getWorldTransform( );
						btTransform child = p_compound->getChildTransform( j );

						btTransform child_world = base * child;

						mat4 out_mat;
						child_world.getOpenGLMatrix( out_mat.m );

						g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][j + 1] = out_mat;
					}
					else
					{
						p_compound->getChildTransform( j ).getOpenGLMatrix( g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][j + 1].m );
					}

					g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][j + 1] = g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][j + 1].transpose( );
				}
			}

			if (g_bullet_objects.entities[i].call_attach)
			{
				g_bullet_objects.entities[i].call_attach = 0;

				g_bullet_objects.entities[i].attach_function( g_bullet_objects.entities[i].p_attach_user_data, g_bullet_objects.entities[i].attach_shape_index );
			}
		}
	}
#endif

    struct trigger_callback : public btCollisionWorld::ContactResultCallback
    {
        btScalar addSingleResult(btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0,
                                 const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1)
        {
            if (s_trigger_contacts[trigger_a].num >= MAX_TRIGGER_CONTACTS)
            {
                return 0.0f;
            }

            u32 contact_index = s_trigger_contacts[trigger_a].num;

            // set flags
            s_triggers[trigger_a].hit_flags |= s_triggers[trigger_b].group;
            s_triggers[trigger_b].hit_flags |= s_triggers[trigger_a].group;

            // set normals
            s_trigger_contacts[trigger_a].normals[contact_index] =
                vec3f(cp.m_normalWorldOnB.x(), cp.m_normalWorldOnB.y(), cp.m_normalWorldOnB.z());

            // set position
            s_trigger_contacts[trigger_a].pos[contact_index] =
                vec3f(cp.m_positionWorldOnB.x(), cp.m_positionWorldOnB.y(), cp.m_positionWorldOnB.z());

            // set indices
            s_trigger_contacts[trigger_a].entity[contact_index] = s_triggers[trigger_b].entity_index;
            s_trigger_contacts[trigger_a].flag[contact_index] = s_triggers[trigger_b].group;

            // increment contact index for the next one
            s_trigger_contacts[trigger_a].num++;

            return 0.0f;
        }

        u32 trigger_a;
        u32 trigger_b;
    };

    void physics_update(f32 dt)
    {
        // step
        if (!g_readable_data.b_paused)
            s_bullet_systems.dynamics_world->stepSimulation(dt);

        // update mats
        update_output_matrices();

        // process triggers
        for (u32 i = 0; i < s_num_triggers; ++i)
        {
            s_trigger_contacts[i].num = 0;

            for (u32 j = 0; j < s_num_triggers; ++j)
            {
                if (i == j)
                    continue;

                if (s_triggers[i].mask & s_triggers[j].group)
                {
                    trigger_callback tc;
                    tc.trigger_a = i;
                    tc.trigger_b = j;

                    s_bullet_systems.dynamics_world->contactPairTest(s_triggers[i].collision_object,
                        s_triggers[j].collision_object, tc);
                }
            }
        }

        // update outputs and clear hit flags
        u32 current_ouput_backbuffer = g_readable_data.current_ouput_backbuffer;

        for (u32 i = 0; i < s_num_triggers; ++i)
        {
            g_readable_data.output_hit_flags[current_ouput_backbuffer][s_triggers[i].entity_index] = s_triggers[i].hit_flags;
            s_triggers[i].hit_flags = 0;
        }

        for (u32 i = 0; i < s_num_triggers; ++i)
        {
            g_readable_data.output_contact_data[current_ouput_backbuffer][s_triggers[i].entity_index] = s_trigger_contacts[i];
        }

        // swap the output buffers
        SWAP_BUFFERS(g_readable_data.current_ouput_backbuffer);
        SWAP_BUFFERS(g_readable_data.current_ouput_frontbuffer);
    }

    void add_rb_internal(const rigid_body_params& params, u32 resource_slot, bool ghost)
    {
        s_bullet_objects.num_entities = std::max<u32>(resource_slot + 1, s_bullet_objects.num_entities);
        physics_entity& entity = s_bullet_objects.entities[resource_slot];

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

    void add_compound_rb_internal(const compound_rb_params& params, u32 resource_slot)
    {
        s_bullet_objects.num_entities = std::max<u32>(resource_slot + 1, s_bullet_objects.num_entities);
        physics_entity& next_entity = s_bullet_objects.entities[resource_slot];

        btCollisionShape* col = create_collision_shape(next_entity, params.base, &params);
        btCompoundShape*  compound = (btCompoundShape*)col;

        next_entity.compound_shape = compound;
        next_entity.num_base_compound_shapes = params.num_shapes;

        next_entity.rb.rigid_body = create_rb_internal(next_entity, params.base, 0, compound);

        next_entity.rb.rigid_body_in_world = 1;
        next_entity.group = params.base.group;
        next_entity.mask = params.base.mask;
    }

    void add_compound_shape_internal(const compound_rb_params& params, u32 resource_slot)
    {
        s_bullet_objects.num_entities = std::max<u32>(resource_slot + 1, s_bullet_objects.num_entities);
        physics_entity& next_entity = s_bullet_objects.entities[resource_slot];

        btCollisionShape* col = create_collision_shape(next_entity, params.base, &params);
        btCompoundShape*  compound = (btCompoundShape*)col;

        next_entity.compound_shape = compound;
    }

    void add_dof6_internal(const constraint_params& params, u32 resource_slot, btRigidBody* rb, btRigidBody* fixed_body)
    {
        s_bullet_objects.num_entities = std::max<u32>(resource_slot + 1, s_bullet_objects.num_entities);
        physics_entity& next_entity = s_bullet_objects.entities[resource_slot];

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
        next_entity.constraint.dof6 = dof6;
    }

    void add_hinge_internal(const constraint_params& params, u32 resource_slot)
    {
        PEN_ASSERT(params.rb_indices[0] > -1);

        s_bullet_objects.num_entities = std::max<u32>(resource_slot + 1, s_bullet_objects.num_entities);
        physics_entity& next_entity = s_bullet_objects.entities[resource_slot];

        btRigidBody* rb = s_bullet_objects.entities[params.rb_indices[0]].rb.rigid_body;

        btHingeConstraint* hinge = new btHingeConstraint(*rb, btVector3(params.pivot.x, params.pivot.y, params.pivot.z),
                                                         btVector3(params.axis.x, params.axis.y, params.axis.z));
        hinge->setLimit(params.lower_limit_rotation.x, params.upper_limit_rotation.x);

        s_bullet_systems.dynamics_world->addConstraint(hinge);

        next_entity.constraint.type = CONSTRAINT_HINGE;
        next_entity.constraint.hinge = hinge;
    }

    void add_constraint_internal(const constraint_params& params, u32 resource_slot)
    {
        s_bullet_objects.entities[resource_slot].type = ENTITY_CONSTRAINT;

        switch (params.type)
        {
            case CONSTRAINT_P2P:
            {
                add_p2p_constraint_params p2p;
                p2p.entity_index = params.rb_indices[0];
                p2p.position = params.pivot;
                add_p2p_constraint_internal(p2p, resource_slot);
            }
            break;

            case CONSTRAINT_HINGE:
            {
                add_hinge_internal(params, resource_slot);
            }
            break;

            case CONSTRAINT_DOF6:
            {
                btRigidBody* p_rb = nullptr;
                btRigidBody* p_fixed = nullptr;

                if (params.rb_indices[0] != -1)
                    p_rb = s_bullet_objects.entities[params.rb_indices[0]].rb.rigid_body;

                if (params.rb_indices[1] != -1)
                    p_fixed = s_bullet_objects.entities[params.rb_indices[1]].rb.rigid_body;

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
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->applyCentralForce(bt_v3);
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->activate(ACTIVE_TAG);
    }

    void add_central_impulse(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->applyCentralImpulse(bt_v3);
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_linear_velocity_internal(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->setLinearVelocity(bt_v3);
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_angular_velocity_internal(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->setAngularVelocity(bt_v3);
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_linear_factor_internal(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->setLinearFactor(bt_v3);
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_angular_factor_internal(const set_v3_params& cmd)
    {
        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->setAngularFactor(bt_v3);
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->activate(ACTIVE_TAG);
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

        btRigidBody* rb = s_bullet_objects.entities[cmd.object_index].rb.rigid_body;

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
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->setGravity(bt_v3);
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->activate();
    }

    void set_friction_internal(const set_float_params& cmd)
    {
        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->setFriction(cmd.data);
    }

    void set_hinge_motor_internal(const set_v3_params& cmd)
    {
        btHingeConstraint* p_hinge = s_bullet_objects.entities[cmd.object_index].constraint.hinge;
        p_hinge->enableAngularMotor(cmd.data.x == 0.0f ? false : true, cmd.data.y, cmd.data.z);
    }

    void set_button_motor_internal(const set_v3_params& cmd)
    {
        btGeneric6DofConstraint* dof6 = s_bullet_objects.entities[cmd.object_index].constraint.dof6;

        btTranslationalLimitMotor* motor = dof6->getTranslationalLimitMotor();
        motor->m_enableMotor[1] = cmd.data.x == 0.0f ? false : true;
        motor->m_targetVelocity = btVector3(0.0f, cmd.data.y, 0.0f);
        motor->m_maxMotorForce = btVector3(0.0f, cmd.data.z, 0.0f);

        s_bullet_objects.entities[cmd.object_index].rb.rigid_body->activate(ACTIVE_TAG);
    }

    void set_multi_joint_motor_internal(const set_multi_v3_params& cmd)
    {
        btMultiBodyJointMotor* p_joint_motor = s_bullet_objects.entities[cmd.multi_index].mb.joint_motors.at(cmd.link_index);

        p_joint_motor->setVelocityTarget(cmd.data.x);
        p_joint_motor->setMaxAppliedImpulse(cmd.data.y);
    }

    void set_multi_joint_pos_internal(const set_multi_v3_params& cmd)
    {
        btMultiBody* p_multi = s_bullet_objects.entities[cmd.multi_index].mb.multi_body;

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
        btMultiBody* p_multi = s_bullet_objects.entities[cmd.multi_index].mb.multi_body;

        p_multi->setBaseVel(btVector3(cmd.data.x, cmd.data.y, cmd.data.z));
    }

    void set_multi_base_pos_internal(const set_multi_v3_params& cmd)
    {
        btMultiBody* p_multi = s_bullet_objects.entities[cmd.multi_index].mb.multi_body;

        p_multi->setBasePos(btVector3(cmd.data.x, cmd.data.y, cmd.data.z));
    }

    void sync_rigid_bodies_internal(const sync_rb_params& cmd)
    {
        if (s_bullet_objects.entities[cmd.master].type == ENTITY_RIGID_BODY)
        {
            btRigidBody* p_rb = s_bullet_objects.entities[cmd.master].rb.rigid_body;
            btRigidBody* p_rb_slave = s_bullet_objects.entities[cmd.slave].rb.rigid_body;

            btTransform master = p_rb->getWorldTransform();
            p_rb_slave->setWorldTransform(master);
        }

        if (s_bullet_objects.entities[cmd.master].type == ENTITY_MULTI_BODY && cmd.link_index != -1)
        {
            btMultiBody* p_mb = s_bullet_objects.entities[cmd.master].mb.multi_body;
            btRigidBody* p_rb_slave = s_bullet_objects.entities[cmd.slave].rb.rigid_body;

            btTransform master = p_mb->getLink(cmd.link_index).m_collider->getWorldTransform();
            p_rb_slave->setWorldTransform(master);
        }
    }

    void sync_rigid_body_velocity_internal(const sync_rb_params& cmd)
    {
        btVector3 master_vel = s_bullet_objects.entities[cmd.master].rb.rigid_body->getAngularVelocity();

        s_bullet_objects.entities[cmd.slave].rb.rigid_body->setAngularVelocity(master_vel);
    }

    void sync_compound_multi_internal(const sync_compound_multi_params& cmd)
    {
        btMultiBody*     p_multi = s_bullet_objects.entities[cmd.multi_index].mb.multi_body;
        btCompoundShape* p_compound = s_bullet_objects.entities[cmd.compound_index].compound_shape;
        btRigidBody*     p_rb = s_bullet_objects.entities[cmd.compound_index].rb.rigid_body;

        u32 num_base_compound_shapes = s_bullet_objects.entities[cmd.compound_index].num_base_compound_shapes;

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
        physics_entity* entity = &s_bullet_objects.entities[resource_slot];

        btRigidBody* rb = s_bullet_objects.entities[cmd.entity_index].rb.rigid_body;

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
            entity->constraint.type = CONSTRAINT_P2P;
        }
    }

    void set_p2p_constraint_pos_internal(const set_v3_params& cmd)
    {
        physics_entity& pe = s_bullet_objects.entities[cmd.object_index];

        btVector3 bt_v3;
        memcpy(&bt_v3, &cmd.data, sizeof(vec3f));

        switch (pe.constraint.type)
        {
            case CONSTRAINT_P2P:
                pe.constraint.point->setPivotB(bt_v3);
                break;
            case CONSTRAINT_P2P_MULTI:
                pe.constraint.point_multi->setPivotInB(bt_v3);
                break;
            default:
                return;
        }
    }

    void set_damping_internal(const set_v3_params& cmd)
    {
        btRigidBody* rb = s_bullet_objects.entities[cmd.object_index].rb.rigid_body;

        rb->setDamping(cmd.data.x, cmd.data.y);
    }

    void set_group_internal(const set_group_params& cmd)
    {
        btRigidBody* rb = s_bullet_objects.entities[cmd.object_index].rb.rigid_body;

        if (rb)
        {
            s_bullet_systems.dynamics_world->removeRigidBody(rb);
            s_bullet_systems.dynamics_world->addRigidBody(rb, cmd.group, cmd.mask);
        }
    }

    void add_collision_watcher_internal(const collision_trigger_data& trigger_data)
    {
        physics_entity& pe = s_bullet_objects.entities[trigger_data.entity_index];

        if (pe.type == ENTITY_RIGID_BODY)
        {
            s_triggers[s_num_triggers].collision_object = (btCollisionObject*)pe.rb.rigid_body;
            s_triggers[s_num_triggers].group = trigger_data.group;
            s_triggers[s_num_triggers].mask = trigger_data.mask;
            s_triggers[s_num_triggers].hit_flags = 0;
            s_triggers[s_num_triggers].entity_index = trigger_data.entity_index;
            s_num_triggers++;
        }
    }

    void attach_rb_to_compound_internal(const attach_to_compound_params& params)
    {
        // todo test this function still works after refactor

        physics_entity& compound = s_bullet_objects.entities[params.compound];
        physics_entity& pe = s_bullet_objects.entities[params.rb];

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

                rb.rigid_body->setWorldTransform(base * compound_child);
            }
            else
            {
                // to callback later
                rb.p_attach_user_data = params.p_user_data;
                rb.attach_function = params.function;
                rb.attach_shape_index = compound.compound_shape->getNumChildShapes();
                rb.call_attach = 1;

                rb.rigid_body_in_world = 0;

                btTransform new_child = rb.rigid_body->getWorldTransform();

                btTransform offset = base.inverse() * new_child;

                compound.compound_shape->addChildShape(offset, rb.rigid_body->getCollisionShape());

                s_bullet_systems.dynamics_world->removeRigidBody(rb.rigid_body);
            }
        }
    }

    void release_entity_internal(u32 entity_index)
    {
        if (s_bullet_objects.entities[entity_index].type == ENTITY_RIGID_BODY)
        {
            remove_from_world_internal(entity_index);
            delete s_bullet_objects.entities[entity_index].rb.rigid_body;
        }
        else if (s_bullet_objects.entities[entity_index].type == ENTITY_CONSTRAINT)
        {
            btTypedConstraint* generic_constraint = s_bullet_objects.entities[entity_index].constraint.generic;

            if (generic_constraint)
                s_bullet_systems.dynamics_world->removeConstraint(generic_constraint);

            delete generic_constraint;
        }
    }

    void remove_from_world_internal(u32 entity_index)
    {
        s_bullet_systems.dynamics_world->removeRigidBody(s_bullet_objects.entities[entity_index].rb.rigid_body);
    }

    void add_to_world_internal(u32 entity_index)
    {
        physics_entity& pe = s_bullet_objects.entities[entity_index];
        s_bullet_systems.dynamics_world->addRigidBody(pe.rb.rigid_body, pe.group, pe.mask);
    }

    void cast_ray_internal(const ray_cast_params& rcp)
    {
        btVector3 from = from_vec3(rcp.start);
        btVector3 to = from_vec3(rcp.end);

        btCollisionWorld::ClosestRayResultCallback ray_callback(from, to);
        ray_callback.m_collisionFilterMask = rcp.mask;
        ray_callback.m_collisionFilterGroup = rcp.group;

        ray_cast_result rcr;
        rcr.user_data = rcp.user_data;

        rcr.physics_handle = -1;
        s_bullet_systems.dynamics_world->rayTest(from, to, ray_callback);
        if (ray_callback.hasHit())
        {
            rcr.point = from_btvector(ray_callback.m_hitPointWorld);
            rcr.normal = from_btvector(ray_callback.m_hitNormalWorld);

            btRigidBody* body = (btRigidBody*)btRigidBody::upcast(ray_callback.m_collisionObject);

            if (body)
                rcr.physics_handle = body->getUserIndex();
        }

        rcp.callback(rcr);
    }

    void cast_sphere_internal(const sphere_cast_params& scp)
    {
        btTransform from = get_bttransform(scp.from, quat());
        btTransform to = get_bttransform(scp.to, quat());

        btVector3 vfrom = from_vec3(scp.from);
        btVector3 vto = from_vec3(scp.to);

        btSphereShape shape = btSphereShape(btScalar(scp.dimension.x));

        btCollisionWorld::ClosestConvexResultCallback cast_callback = btCollisionWorld::ClosestConvexResultCallback(vfrom, vto);
        cast_callback.m_collisionFilterMask = scp.mask;
        cast_callback.m_collisionFilterGroup = scp.group;

        s_bullet_systems.dynamics_world->convexSweepTest((btConvexShape*)&shape, from, to, cast_callback);

        sphere_cast_result sr;
        sr.user_data = scp.user_data;

        sr.physics_handle = -1;
        if (cast_callback.hasHit())
        {
            btRigidBody* body = (btRigidBody*)btRigidBody::upcast(cast_callback.m_hitCollisionObject);

            if (body)
                sr.physics_handle = body->getUserIndex();

            sr.point = from_btvector(cast_callback.m_hitPointWorld);
            sr.normal = from_btvector(cast_callback.m_hitNormalWorld);
        }

        scp.callback(sr);
    }
} // namespace physics

#if 0 // reference
virtual void removePickingConstraint()
{
    if (m_pickedConstraint)
    {
        m_pickedBody->forceActivationState( m_savedState );
        m_pickedBody->activate();
        m_dynamicsWorld->removeConstraint( m_pickedConstraint );
        delete m_pickedConstraint;
        m_pickedConstraint = 0;
        m_pickedBody = 0;
    }
}

#endif
