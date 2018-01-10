#include "pen_string.h"
#include "timer.h"
#include "slot_resource.h"

#include "internal/physics_internal.h"

namespace physics
{
#define SWAP_BUFFERS( b ) b = (b + 1)%NUM_OUTPUT_BUFFERS

    inline btVector3 from_lw_vec3( const lw_vec3f& v3 )
    {
        return btVector3( v3.x, v3.y, v3.z );
    }

    inline vec3f from_btvector( const btVector3& bt )
    {
        return vec3f( bt.getX(), bt.getY(), bt.getZ() );
    }

	readable_data				g_readable_data;

	trigger						g_triggers[MAX_TRIGGERS];

	trigger_contact_data		g_trigger_contacts[MAX_TRIGGERS];

	u32							g_num_triggers = 0;

	bullet_systems				g_bullet_systems;
	bullet_objects				g_bullet_objects;
	
	generic_constraint			p2p_constraints[MAX_P2P_CONSTRAINTS];

    struct collision_responsors
	{
		collision_responsors( )
		{
			num_responsors = 0;
		}

		collision_response response_data[MAX_PHYSICS_RESOURCES];
		u32 num_responsors;
	};

	collision_responsors g_collision_responsors;

	btTransform get_bttransform_from_params( const rigid_body_params &params )
	{
		btTransform trans;
		trans.setIdentity( );
		trans.setOrigin( btVector3( btScalar( params.position.x ), btScalar( params.position.y ), btScalar( params.position.z ) ) );

		btQuaternion bt_quat;
		pen::memory_cpy( &bt_quat, &params.rotation, sizeof(quat) );
		trans.setRotation( bt_quat );

		return trans;
	}

	btCollisionShape* create_collision_shape( physics_entity& entity, const rigid_body_params &params, const compound_rb_params *p_compound = NULL )
	{
		btCollisionShape* shape = NULL;

        switch (params.shape)
        {
        case physics::BOX:
            shape = new btBoxShape( btVector3( btScalar( params.dimensions.x ), btScalar( params.dimensions.y ), btScalar( params.dimensions.z ) ) );
            break;
        case physics::CYLINDER:
            if (params.shape_up_axis == UP_X)
            {
                shape = new btCylinderShapeX( btVector3( btScalar( params.dimensions.x ), btScalar( params.dimensions.y ), btScalar( params.dimensions.z ) ) );
            }
            else if (params.shape_up_axis == UP_Z)
            {
                shape = new btCylinderShapeZ( btVector3( btScalar( params.dimensions.x ), btScalar( params.dimensions.y ), btScalar( params.dimensions.z ) ) );
            }
            else
            {
                shape = new btCylinderShape( btVector3( btScalar( params.dimensions.x ), btScalar( params.dimensions.y ), btScalar( params.dimensions.z ) ) );
            }
            break;
        case physics::CAPSULE:
            if (params.shape_up_axis == UP_X)
            {
                shape = new btCapsuleShapeX( btScalar( params.dimensions.y ), btScalar( params.dimensions.x ) );
            }
            else if (params.shape_up_axis == UP_Z)
            {
                shape = new btCapsuleShapeZ( btScalar( params.dimensions.x ), btScalar( params.dimensions.z ) );
            }
            else
            {
                shape = new btCapsuleShape( btScalar( params.dimensions.x ), btScalar( params.dimensions.y ) );
            }
            break;
        case physics::HULL:
            shape = new btConvexHullShape( params.mesh_data.vertices, params.mesh_data.num_floats / 3, 12 );
            break;
        case physics::MESH:
            {
                u32 num_tris = params.mesh_data.num_indices / 3;
                btTriangleIndexVertexArray* mesh = new btTriangleIndexVertexArray( num_tris, ( s32* )params.mesh_data.indices, sizeof( u32 ) * 3, params.mesh_data.num_floats / 3, params.mesh_data.vertices, sizeof( f32 ) * 3 );
                btBvhTriangleMeshShape* concave_mesh = new btBvhTriangleMeshShape( mesh, true );
                shape = concave_mesh;
            }
            break;
        case physics::COMPOUND:
        {
            if (p_compound)
            {
                u32 num_shapes = p_compound->num_shapes;

                btCompoundShape* compound = new btCompoundShape( true );

                for (u32 s = 0; s < num_shapes; ++s)
                {
                    btCollisionShape* p_sub_shape = create_collision_shape( entity, p_compound->rb[s] );

                    btTransform trans = get_bttransform_from_params( p_compound->rb[s] );

                    compound->addChildShape( trans, p_sub_shape );
                }

                shape = compound;
            }
        }
        break;
        case physics::SPHERE:
            shape = new btSphereShape( params.dimensions.x );
            break;
        default:
            PEN_ASSERT_MSG( 0, "unimplemented physics shape" );
            break;
        }

		if (shape)
			entity.collision_shape = shape;

		return shape;
	}

	btMultiBody* create_multirb_internal( physics_entity& entity, const multi_body_params &params )
	{
		//init the base	
		btVector3 baseInertiaDiag( 0.f, 0.f, 0.f );

		btCollisionShape *p_base_col = create_collision_shape( entity, params.base );

		if (p_base_col)
		{
			p_base_col->calculateLocalInertia( params.base.mass, baseInertiaDiag );
		}

		bool canSleep = false;

		btMultiBody *p_multibody = new btMultiBody( params.num_links, params.base.mass, baseInertiaDiag, params.base.mass == 0.0f ? true : false, canSleep, params.multi_dof == 1 );

		btQuaternion bt_quat;
		pen::memory_cpy( &bt_quat, &params.base.rotation, sizeof(quat) );

		btVector3 base_pos = btVector3( params.base.position.x, params.base.position.y, params.base.position.z );
		p_multibody->setBasePos( base_pos );
		p_multibody->setWorldToBaseRot( bt_quat );

		//init the links	
		btVector3 linkInertiaDiag( 0.f, 0.f, 0.f );

		btMultiBodyLinkCollider* col = new btMultiBodyLinkCollider( p_multibody, -1 );
		entity.link_colliders.push_back(col);
		col->setCollisionShape( p_base_col );

		btQuaternion world_to_local = p_multibody->getWorldToBaseRot( );
		float quat[4] =
		{
			-world_to_local.x( ),
			-world_to_local.y( ),
			-world_to_local.z( ),
			world_to_local.w( )
		};

		btTransform tr;
		tr.setIdentity( );
		tr.setOrigin( p_multibody->getBasePos( ) );
		tr.setRotation( btQuaternion( quat[0], quat[1], quat[2], quat[3] ) );
		col->setWorldTransform( tr );

		//, params.base.group, params.base.mask 
		if (p_base_col)
		{
			g_bullet_systems.dynamics_world->addCollisionObject( col, params.base.group, params.base.mask );
			p_multibody->setBaseCollider( col );
		}
		else
		{
			p_multibody->setBaseCollider( NULL );
		}

		btVector3 parent_pos = base_pos;

		btVector3 link_positions[32];

		entity.joint_motors.clear( );

		for (u32 i = 0; i < params.num_links; ++i)
		{
			link_positions[i] = btVector3( params.links[i].rb.position.x, params.links[i].rb.position.y, params.links[i].rb.position.z );
		}

		for (u32 i = 0; i < params.num_links; ++i)
		{
			//y-axis assumed up
			btVector3 link_pos = btVector3( params.links[i].rb.position.x, params.links[i].rb.position.y, params.links[i].rb.position.z );

			if (params.links[i].parent == -1)
			{
				parent_pos = base_pos;
			}
			else
			{
				parent_pos = link_positions[params.links[i].parent];
			}

			btVector3 parentComToCurrentCom = link_pos;
			if (params.links[i].transform_world_to_local)
			{
				parentComToCurrentCom = link_pos - parent_pos;
			}

			btVector3 hinge_offset = btVector3( params.links[i].hinge_offset.x, params.links[i].hinge_offset.y, params.links[i].hinge_offset.z );

			btVector3 currentPivotToCurrentCom = hinge_offset;		 									//cur body's COM to cur body's PIV offset
			btVector3 parentComToCurrentPivot = parentComToCurrentCom - currentPivotToCurrentCom;	    //par body's COM to cur body's PIV offset

			btQuaternion link_quat;
			pen::memory_cpy( &link_quat, &params.links[i].rb.rotation, sizeof(quat) );

			btQuaternion parent_to_this_quat = link_quat.inverse( );

			btVector3 hinge_axis = btVector3( params.links[i].hinge_axis.x, params.links[i].hinge_axis.y, params.links[i].hinge_axis.z );

			if (params.links[i].link_type == REVOLUTE)
			{
				p_multibody->setupRevolute( i, params.links[i].rb.mass, linkInertiaDiag, params.links[i].parent, parent_to_this_quat, hinge_axis, parentComToCurrentPivot, currentPivotToCurrentCom, false );
				parent_pos = link_pos;
			}
			else if (params.links[i].link_type == FIXED)
			{
				parent_to_this_quat = link_quat.inverse( );

				p_multibody->setupFixed( i, params.links[i].rb.mass, linkInertiaDiag, params.links[i].parent, parent_to_this_quat, parentComToCurrentPivot, currentPivotToCurrentCom, false );
			}

#ifdef MULTIBODY_WORLD
			if (params.links[i].joint_motor == 1 && !params.multi_dof)
			{
				btMultiBodyJointMotor* con = new btMultiBodyJointMotor( p_multibody, i, 0.0f, 0.0f );
				g_bullet_systems.dynamics_world->addMultiBodyConstraint( con );
				
				entity.joint_motors.push_back( con );
			}
			else
			{
				entity.joint_motors.push_back( NULL );
			}

			if (params.links[i].joint_limit_constraint == 1)
			{
				btMultiBodyJointLimitConstraint* con = new btMultiBodyJointLimitConstraint( p_multibody, i, params.links[i].hinge_limits.x, params.links[i].hinge_limits.y );
				g_bullet_systems.dynamics_world->addMultiBodyConstraint( con );

				entity.joint_limits.push_back( con );
			}
			else
			{
				entity.joint_limits.push_back( NULL );
			}
#endif
		}

		if (params.multi_dof == 1)
		{
			p_multibody->finalizeMultiDof( );
		}

#ifdef MULTIBODY_WORLD
		g_bullet_systems.dynamics_world->addMultiBody( p_multibody );
#endif

		btVector3 local_origin;

		for (int i = 0; i < p_multibody->getNumLinks( ); ++i)
		{
			btCollisionShape* link_col_shape = NULL;

			if (params.links[i].rb.shape == physics::COMPOUND)
			{
				link_col_shape = g_bullet_objects.entities[ params.links[i].compound_index ].compound_shape;
			}
			else
			{
				link_col_shape = create_collision_shape( entity, params.links[i].rb );
			}

			if (!link_col_shape)
			{
				continue;
			}

			//const int parent = p_multibody->getParent( i );
			world_to_local = p_multibody->getParentToLocalRot( i ) * world_to_local;

			btVector3 multi_base_pos = p_multibody->getBasePos( );

			btVector3 r_vector = p_multibody->getRVector( i );

			btQuaternion world_to_local_inverse = world_to_local.inverse( );

			local_origin = p_multibody->getBasePos( ) + (quatRotate( world_to_local_inverse, r_vector ));

			btVector3 posr = local_origin;

			float quat[4] = { -world_to_local.x( ), -world_to_local.y( ), -world_to_local.z( ), world_to_local.w( ) };

			btMultiBodyLinkCollider* col = new btMultiBodyLinkCollider( p_multibody, i );
			entity.link_colliders.push_back( col );

			col->setCollisionShape( link_col_shape );
			btTransform tr;
			tr.setIdentity( );
			tr.setOrigin( posr );
			tr.setRotation( btQuaternion( quat[0], quat[1], quat[2], quat[3] ) );
			col->setWorldTransform( tr );

			g_bullet_systems.dynamics_world->addCollisionObject( col, params.links[i].rb.group, params.links[i].rb.mask );

			p_multibody->getLink( i ).m_collider = col;
		}

		entity.multi_body = p_multibody;

		return p_multibody;
	}

	btRigidBody* create_rb_internal( physics_entity& entity, const rigid_body_params &params, u32 ghost, btCollisionShape* p_existing_shape )
	{
		//create box shape at position and orientation specified in the command
		btCollisionShape* shape = NULL;
		btTransform shape_transform;

		if (p_existing_shape)
		{
			shape = p_existing_shape;
		}
		else
		{
			shape = create_collision_shape( entity, params );
		}

		shape_transform.setIdentity( );
		shape_transform.setOrigin( btVector3( btScalar( params.position.x ), btScalar( params.position.y ), btScalar( params.position.z ) ) );

		btQuaternion bt_quat;
		pen::memory_cpy( &bt_quat, &params.rotation, sizeof(quat) );
		shape_transform.setRotation( bt_quat );

		//create rigid body
		btScalar mass( params.mass );

		bool dynamic = (mass != 0.0f);

		btVector3 local_inertia( 0, 0, 0 );
		if (dynamic && shape)
		{
			shape->calculateLocalInertia( mass, local_inertia );
		}

		//using motion state is recommended, it provides interpolation capabilities, and only synchronizes 'active' objects
		btDefaultMotionState* motion_state = new btDefaultMotionState( shape_transform );
		entity.default_motion_state = motion_state;

		btRigidBody::btRigidBodyConstructionInfo rb_info( mass, motion_state, shape, local_inertia );

		btRigidBody* body = new btRigidBody( rb_info );
		body->setContactProcessingThreshold( BT_LARGE_FLOAT );
		body->setActivationState( DISABLE_DEACTIVATION );

		if (!ghost)
		{
			g_bullet_systems.dynamics_world->addRigidBody( body, params.group, params.mask );
		}

		return body;
	}

	void physics_initialise( )
	{
        pen::memory_zero( &g_bullet_objects.entities, sizeof( physics_entity ) * MAX_PHYSICS_RESOURCES );
		g_bullet_objects.num_entities = 0;

		g_bullet_systems.collision_config = new btDefaultCollisionConfiguration( );

		///use the default collision dispatcher. For parallel processing you can use a different dispatcher (see Extras/BulletMultiThreaded)
		g_bullet_systems.dispatcher = new btCollisionDispatcher( g_bullet_systems.collision_config );

		///btDbvtBroadphase is a good general purpose broadphase. You can also try out btAxis3Sweep.
		g_bullet_systems.olp_cache = new btAxisSweep3( btVector3( -50.0f, -50.0f, -50.0f ), btVector3( 50.0f, 50.0f, 50.0f ) );

		///the default constraint solver. For parallel processing you can use a different solver (see Extras/BulletMultiThreaded)
#ifdef MULTIBODY_WORLD
		g_bullet_systems.solver = new btMultiBodyConstraintSolver;
		g_bullet_systems.dynamics_world = new btMultiBodyDynamicsWorld( g_bullet_systems.dispatcher, g_bullet_systems.olp_cache, g_bullet_systems.solver, g_bullet_systems.collision_config );
#else
		g_bullet_systems.solver = new btSequentialImpulseConstraintSolver;
		g_bullet_systems.dynamics_world = new btDiscreteDynamicsWorld( g_bullet_systems.dispatcher, g_bullet_systems.olp_cache, g_bullet_systems.solver, g_bullet_systems.collision_config );
#endif

		g_bullet_systems.dynamics_world->setGravity( btVector3( 0, -10, 0 ) );
	}

	void consume_cmd_buf( );

	void update_output_matrices( )
	{
		u32 current_ouput_backbuffer = g_readable_data.current_ouput_backbuffer;

		for (u32 i = 0; i < g_bullet_objects.num_entities; i++)
		{
			btRigidBody* p_rb = g_bullet_objects.entities[i].rigid_body;

			if (p_rb && g_bullet_objects.entities[i].rigid_body_in_world)
			{
				btTransform rb_transform = p_rb->getWorldTransform( );

                btScalar _mm[16];
                
				rb_transform.getOpenGLMatrix( _mm );
                
                for( s32 m = 0; m < 16; ++m )
                    g_readable_data.output_matrices[current_ouput_backbuffer][i].m[m] = _mm[m];

				g_readable_data.output_matrices[current_ouput_backbuffer][i] = g_readable_data.output_matrices[current_ouput_backbuffer][i].transpose( );
			}

			btMultiBody* p_multi = g_bullet_objects.entities[i].multi_body;

			if (p_multi)
			{
				btMultiBodyLinkCollider* p_base_col = p_multi->getBaseCollider( );

				btTransform rb_transform;

				if (p_base_col)
				{
					rb_transform = p_base_col->getWorldTransform( );
					rb_transform.getOpenGLMatrix( g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][0].m );
				}

				g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][0] = g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][0].transpose( );

				for (s32 j = 0; j < p_multi->getNumLinks( ); ++j)
				{
					if (p_multi->getLink( j ).m_collider)
					{
						rb_transform = p_multi->getLink( j ).m_collider->getWorldTransform( );
						rb_transform.getOpenGLMatrix( g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][j + 1].m );

						g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][j + 1] = g_readable_data.multi_output_matrices[current_ouput_backbuffer][i][j + 1].transpose( );
					}

					g_readable_data.multi_joint_positions[current_ouput_backbuffer][i][j + 1] = p_multi->getJointPos( j );
				}
			}

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

	struct trigger_callback : public btCollisionWorld::ContactResultCallback
	{
		btScalar addSingleResult( btManifoldPoint& cp, const btCollisionObjectWrapper* colObj0Wrap, int partId0, int index0, const btCollisionObjectWrapper* colObj1Wrap, int partId1, int index1 )
		{
			if (g_trigger_contacts[trigger_a].num >= MAX_TRIGGER_CONTACTS)
			{
				return 0.0f;
			}

			u32 contact_index = g_trigger_contacts[trigger_a].num;

			//set flags
			g_triggers[trigger_a].hit_flags |= g_triggers[trigger_b].group;
			g_triggers[trigger_b].hit_flags |= g_triggers[trigger_a].group;

			//set normals
			g_trigger_contacts[trigger_a].normals[contact_index] = vec3f( cp.m_normalWorldOnB.x( ), cp.m_normalWorldOnB.y( ), cp.m_normalWorldOnB.z( ) );

			//set position
			g_trigger_contacts[trigger_a].pos[contact_index] = vec3f( cp.m_positionWorldOnB.x( ), cp.m_positionWorldOnB.y( ), cp.m_positionWorldOnB.z( ) );

			//set indices
			g_trigger_contacts[trigger_a].entity[contact_index] = g_triggers[trigger_b].entity_index;
			g_trigger_contacts[trigger_a].flag[contact_index] = g_triggers[trigger_b].group;

			//increment contact index for the next one
			g_trigger_contacts[trigger_a].num++;

			return 0.0f;
		}

		u32		 trigger_a;
		u32		 trigger_b;
	};
    
    void physics_update( f32 dt )
    {
        //step
        if (!g_readable_data.b_paused)
        {
            g_bullet_systems.dynamics_world->stepSimulation( dt );
        }
        
        //update mats
        update_output_matrices( );
        
        //process triggers
        for (u32 i = 0; i < g_num_triggers; ++i)
        {
            g_trigger_contacts[i].num = 0;
            
            for (u32 j = 0; j < g_num_triggers; ++j)
            {
                if (i == j)
                {
                    continue;
                }
                
                if (g_triggers[i].mask & g_triggers[j].group)
                {
                    trigger_callback tc;
                    tc.trigger_a = i;
                    tc.trigger_b = j;
                    
                    g_bullet_systems.dynamics_world->getCollisionWorld( )->contactPairTest( g_triggers[i].collision_object, g_triggers[j].collision_object, tc );
                    
                }
            }
        }
        
        //update outputs and clear hit flags
        u32 current_ouput_backbuffer = g_readable_data.current_ouput_backbuffer;
        //u32 current_ouput_frontbuffer = g_readable_data.current_ouput_frontbuffer;
        
        for (u32 i = 0; i < g_num_triggers; ++i)
        {
            g_readable_data.output_hit_flags[current_ouput_backbuffer][g_triggers[i].entity_index] = g_triggers[i].hit_flags;
            g_triggers[i].hit_flags = 0;
        }
        
        for (u32 i = 0; i < g_num_triggers; ++i)
        {
            g_readable_data.output_contact_data[current_ouput_backbuffer][g_triggers[i].entity_index] = g_trigger_contacts[i];
        }
        
        //swap the output buffers
        SWAP_BUFFERS( g_readable_data.current_ouput_backbuffer );
        SWAP_BUFFERS( g_readable_data.current_ouput_frontbuffer );

    }

	void add_rb_internal( const rigid_body_params &params, u32 resource_slot, bool ghost )
	{
        g_bullet_objects.num_entities = std::max<u32>(resource_slot+1, g_bullet_objects.num_entities);
		physics_entity& entity = g_bullet_objects.entities[ resource_slot ];

		//add the body to the dynamics world
		btRigidBody* rb = create_rb_internal( entity, params, ghost );
        rb->setUserIndex( resource_slot );

        entity.rigid_body = rb;
        entity.rigid_body_in_world = !ghost;

        entity.group = params.group;
        entity.mask = params.mask;
	}

	void add_compound_rb_internal( const compound_rb_params &params, u32 resource_slot )
	{
        g_bullet_objects.num_entities = std::max<u32>(resource_slot+1, g_bullet_objects.num_entities);
		physics_entity& next_entity = g_bullet_objects.entities[ resource_slot ];

		btCollisionShape* col = create_collision_shape( next_entity, params.base, &params );
		btCompoundShape* compound = ( btCompoundShape* ) col;

		next_entity.compound_shape = compound;
		next_entity.num_base_compound_shapes = params.num_shapes;

		next_entity.rigid_body = create_rb_internal( next_entity, params.base, 0, compound );

		next_entity.rigid_body_in_world = 1;
		next_entity.group = params.base.group;
		next_entity.mask = params.base.mask;
	}

	void add_compound_shape_internal( const compound_rb_params &params, u32 resource_slot )
	{
        g_bullet_objects.num_entities = std::max<u32>(resource_slot+1, g_bullet_objects.num_entities);
		physics_entity& next_entity = g_bullet_objects.entities[ resource_slot ];

		btCollisionShape* col = create_collision_shape( next_entity, params.base, &params );
		btCompoundShape* compound = ( btCompoundShape* ) col;

		next_entity.compound_shape = compound;
	}

	void add_dof6_internal( const constraint_params &params, u32 resource_slot, btRigidBody* rb, btRigidBody* fixed_body )
	{
        g_bullet_objects.num_entities = std::max<u32>(resource_slot+1, g_bullet_objects.num_entities);
		physics_entity& next_entity = g_bullet_objects.entities[ resource_slot ];

		u32 fixed_con = 1;

		if (rb == NULL)
		{
			fixed_con = 0;

			//create the actual rigid body
			rb = create_rb_internal( next_entity, params.rb, 0 );

			next_entity.rigid_body = rb;
			next_entity.rigid_body_in_world = 1;

			next_entity.group = params.rb.group;
			next_entity.mask = params.rb.mask;

		}

		if (fixed_body == NULL)
		{
			//create a fixed rigid body as the anchor 
			rigid_body_params fixed_rbp;
			fixed_rbp = params.rb;
			fixed_rbp.mass = 0.0f;
			fixed_rbp.shape = 0;
			fixed_body = create_rb_internal( next_entity, fixed_rbp, 0 );
			fixed_body->setActivationState( DISABLE_DEACTIVATION );
		}

		//reference frames are identity 
		btTransform frameInA, frameInB;
		frameInA.setIdentity( );
		frameInB.setIdentity( );

		rb->setDamping( params.linear_damping, params.angular_damping );

		//create the constraint and lock all axes
		btGeneric6DofConstraint* dof6 = new btGeneric6DofConstraint( *fixed_body, *rb, frameInA, frameInB, true );

		dof6->setLinearLowerLimit( btVector3( params.lower_limit_translation.x, params.lower_limit_translation.y, params.lower_limit_translation.z ) );
		dof6->setLinearUpperLimit( btVector3( params.upper_limit_translation.x, params.upper_limit_translation.y, params.upper_limit_translation.z ) );

		dof6->setAngularLowerLimit( btVector3( params.lower_limit_rotation.x, params.lower_limit_rotation.y, params.lower_limit_rotation.z ) );
		dof6->setAngularUpperLimit( btVector3( params.upper_limit_rotation.x, params.upper_limit_rotation.y, params.upper_limit_rotation.z ) );

		g_bullet_systems.dynamics_world->addConstraint( dof6 );
		next_entity.dof6_constraint = dof6;
	}

	void add_multibody_internal( const multi_body_params &params, u32 resource_slot )
	{
		physics_entity& next_entity = g_bullet_objects.entities[ resource_slot ];

		next_entity.multi_body = create_multirb_internal( next_entity, params );
	}

	void add_hinge_internal( const constraint_params &params, u32 resource_slot )
	{
        g_bullet_objects.num_entities = std::max<u32>(resource_slot+1, g_bullet_objects.num_entities);
        physics_entity& next_entity = g_bullet_objects.entities[ resource_slot ];

		//create the actual rigid body
		btRigidBody* rb = create_rb_internal( next_entity, params.rb, 0 );

		next_entity.rigid_body = rb;
		next_entity.rigid_body_in_world = 1;
		next_entity.group = params.rb.group;
		next_entity.mask = params.rb.mask;

		btHingeConstraint* hinge = new btHingeConstraint( *rb, btVector3( params.pivot.x, params.pivot.y, params.pivot.z ), btVector3( params.axis.x, params.axis.y, params.axis.z ) );
		hinge->setLimit( params.lower_limit_rotation.x, params.upper_limit_rotation.x );

		g_bullet_systems.dynamics_world->addConstraint( hinge );

		next_entity.hinge_constraint = hinge;
	}

	void add_constrained_rb_internal( const constraint_params &params, u32 resource_slot )
	{
		switch (params.type)
		{
			case physics::CONSTRAINT_DOF6_RB:
			{
				btRigidBody* p_rb = NULL;
				btRigidBody* p_fixed = NULL;

				if (params.rb_indices[0] != -1)
				{
					p_rb = g_bullet_objects.entities[params.rb_indices[0]].rigid_body;
				}

				if (params.rb_indices[1] != -1)
				{
					p_fixed = g_bullet_objects.entities[params.rb_indices[1]].rigid_body;
				}

				add_dof6_internal( params, resource_slot, p_rb, p_fixed );
			}
			break;

		case physics::CONSTRAINT_DOF6:
			add_dof6_internal( params, resource_slot, NULL, NULL );
			break;

		case physics::CONSTRAINT_HINGE:
			add_hinge_internal( params, resource_slot );
			break;

		default:
			break;
		}
	}

    void add_constraint_internal( const constraint_params &params, u32 resource_slot )
    {
        switch (params.type)
        {
            case CONSTRAINT_P2P:
            {
                add_p2p_constraint_params p2p;
                p2p.entity_index = params.rb_indices[0];
                p2p.position = params.pivot;
                add_p2p_constraint_internal( p2p, resource_slot );
            }
            break;

            default:
                PEN_ASSERT_MSG( 0, "unimplemented add constraint" );
                break;
        }
    }

	void set_linear_velocity_internal( const set_v3_params &cmd )
	{
		btVector3 bt_v3;
		pen::memory_cpy( &bt_v3, &cmd.data, sizeof(vec3f) );
		g_bullet_objects.entities[cmd.object_index].rigid_body->setLinearVelocity( bt_v3 );
		g_bullet_objects.entities[cmd.object_index].rigid_body->activate( ACTIVE_TAG );
	}

	void set_angular_velocity_internal( const set_v3_params &cmd )
	{
		btVector3 bt_v3;
		pen::memory_cpy( &bt_v3, &cmd.data, sizeof(vec3f) );
		g_bullet_objects.entities[cmd.object_index].rigid_body->setAngularVelocity( bt_v3 );
		g_bullet_objects.entities[cmd.object_index].rigid_body->activate( ACTIVE_TAG );
	}

	void set_linear_factor_internal( const set_v3_params &cmd )
	{
		btVector3 bt_v3;
		pen::memory_cpy( &bt_v3, &cmd.data, sizeof(vec3f) );
		g_bullet_objects.entities[cmd.object_index].rigid_body->setLinearFactor( bt_v3 );
		g_bullet_objects.entities[cmd.object_index].rigid_body->activate( ACTIVE_TAG );
	}

	void set_angular_factor_internal( const set_v3_params &cmd )
	{
		btVector3 bt_v3;
		pen::memory_cpy( &bt_v3, &cmd.data, sizeof(vec3f) );
		g_bullet_objects.entities[cmd.object_index].rigid_body->setAngularFactor( bt_v3 );
		g_bullet_objects.entities[cmd.object_index].rigid_body->activate( ACTIVE_TAG );
	}

	void set_transform_internal( const set_transform_params &cmd )
	{
		btVector3 bt_v3;
		btQuaternion bt_quat;

		pen::memory_cpy( &bt_v3, &cmd.position, sizeof(vec3f) );
		pen::memory_cpy( &bt_quat, &cmd.rotation, sizeof(quat) );

		btTransform bt_trans;
		bt_trans.setOrigin( bt_v3 );
		bt_trans.setRotation( bt_quat );

        btRigidBody* rb = g_bullet_objects.entities[cmd.object_index].rigid_body;

        if (rb)
        {
            g_bullet_objects.entities[cmd.object_index].rigid_body->getMotionState()->setWorldTransform( bt_trans );
            g_bullet_objects.entities[cmd.object_index].rigid_body->setCenterOfMassTransform( bt_trans );
        }
	}

	void set_gravity_internal( const set_v3_params &cmd )
	{
		btVector3 bt_v3;
		pen::memory_cpy( &bt_v3, &cmd.data, sizeof(vec3f) );
		g_bullet_objects.entities[cmd.object_index].rigid_body->setGravity( bt_v3 );
		g_bullet_objects.entities[cmd.object_index].rigid_body->activate( );
	}

	void set_friction_internal( const set_float_params &cmd )
	{
		g_bullet_objects.entities[cmd.object_index].rigid_body->setFriction( cmd.data );
	}

	void set_hinge_motor_internal( const set_v3_params &cmd )
	{
		g_bullet_objects.entities[cmd.object_index].hinge_constraint->enableAngularMotor( cmd.data.x == 0.0f ? false : true, cmd.data.y, cmd.data.z );
	}

	void set_button_motor_internal( const set_v3_params &cmd )
	{
		btTranslationalLimitMotor* motor = g_bullet_objects.entities[cmd.object_index].dof6_constraint->getTranslationalLimitMotor( );
		motor->m_enableMotor[1] = cmd.data.x == 0.0f ? false : true;
		motor->m_targetVelocity = btVector3( 0.0f, cmd.data.y, 0.0f );
		motor->m_maxMotorForce = btVector3( 0.0f, cmd.data.z, 0.0f );

		g_bullet_objects.entities[cmd.object_index].rigid_body->activate( ACTIVE_TAG );
	}

	void set_multi_joint_motor_internal( const set_multi_v3_params &cmd )
	{
		btMultiBodyJointMotor* p_joint_motor = g_bullet_objects.entities[cmd.multi_index].joint_motors.at( cmd.link_index );

		p_joint_motor->setVelocityTarget( cmd.data.x );
		p_joint_motor->setMaxAppliedImpulse( cmd.data.y );
	}

	void set_multi_joint_pos_internal( const set_multi_v3_params &cmd )
	{
		btMultiBody* p_multi = g_bullet_objects.entities[cmd.multi_index].multi_body;

		if( 1 )
        {
			f32 multi_dof_pos = cmd.data.x;
			p_multi->setJointPosMultiDof( cmd.link_index, &multi_dof_pos );
		}
		else
		{
			p_multi->setJointPos( cmd.link_index, cmd.data.x );
		}
	}

	void set_multi_joint_limit_internal( const set_multi_v3_params &cmd )
	{

	}

	void set_multi_base_velocity_internal( const set_multi_v3_params &cmd )
	{
		g_bullet_objects.entities[cmd.multi_index].multi_body->setBaseVel( btVector3( cmd.data.x, cmd.data.y, cmd.data.z ) );
	}

	void set_multi_base_pos_internal( const set_multi_v3_params &cmd )
	{
		g_bullet_objects.entities[cmd.multi_index].multi_body->setBasePos( btVector3( cmd.data.x, cmd.data.y, cmd.data.z ) );
	}

	void sync_rigid_bodies_internal( const sync_rb_params &cmd )
	{
		if (g_bullet_objects.entities[cmd.master].rigid_body)
		{
			btTransform master = g_bullet_objects.entities[cmd.master].rigid_body->getWorldTransform( );
			g_bullet_objects.entities[cmd.slave].rigid_body->setWorldTransform( master );
		}

		if (g_bullet_objects.entities[cmd.master].multi_body && cmd.link_index != -1)
		{
			btTransform master = g_bullet_objects.entities[cmd.master].multi_body->getLink( cmd.link_index ).m_collider->getWorldTransform( );
			g_bullet_objects.entities[cmd.slave].rigid_body->setWorldTransform( master );
		}
	}

	void sync_rigid_body_velocity_internal( const sync_rb_params &cmd )
	{
		btVector3 master_vel = g_bullet_objects.entities[cmd.master].rigid_body->getAngularVelocity( );

		g_bullet_objects.entities[cmd.slave].rigid_body->setAngularVelocity( master_vel );
	}

	void sync_compound_multi_internal( const sync_compound_multi_params &cmd )
	{
		btMultiBody* p_multi = g_bullet_objects.entities[cmd.multi_index].multi_body;
		btCompoundShape* p_compound = g_bullet_objects.entities[cmd.compound_index].compound_shape;
		btRigidBody* p_rb = g_bullet_objects.entities[cmd.compound_index].rigid_body;

		u32 num_base_compound_shapes = g_bullet_objects.entities[cmd.compound_index].num_base_compound_shapes;

		PEN_ASSERT( p_multi );
		PEN_ASSERT( p_compound );
		PEN_ASSERT( p_rb );

		u32 num_links = p_multi->getNumLinks( );

		btTransform rb_trans = p_rb->getWorldTransform( );
		p_multi->setBasePos( rb_trans.getOrigin( ) );

		btTransform multi_base_trans = p_multi->getBaseCollider( )->getWorldTransform( );

		btAlignedObjectArray< btTransform > current_offsets;

		//get current attached shapes offsets
		btTransform base_cur = p_compound->getChildTransform( 0 );

		u32 num_shapes = p_compound->getNumChildShapes( );
		for (u32 attached = num_base_compound_shapes; attached < num_shapes; ++attached)
		{
			btTransform child_cur = p_compound->getChildTransform( attached );

			current_offsets.push_back( base_cur.inverse( ) * child_cur );
		}

		u32 compound_shape_itr = 0;
		for (u32 link = 0; link < num_links; ++link)
		{
			if (p_multi->getLink( link ).m_collider)
			{
				btTransform link_world_trans = p_multi->getLink( link ).m_collider->getWorldTransform( );

				btTransform link_local_trans = multi_base_trans.inverse( ) * link_world_trans;

				p_compound->updateChildTransform( compound_shape_itr, link_local_trans );

				compound_shape_itr++;
			}
		}

		//transform current attached by the base
		btTransform base_new = p_compound->getChildTransform( 0 );

		u32 offset_index = 0;
		for (u32 attached = num_base_compound_shapes; attached < num_shapes; ++attached)
		{
			p_compound->updateChildTransform( attached, base_new * current_offsets.at( offset_index ) );

			offset_index++;
		}
	}

	void add_p2p_constraint_internal( const add_p2p_constraint_params &cmd, u32 resource_slot )
	{
        physics_entity* entity = &g_bullet_objects.entities[resource_slot];

		btRigidBody* rb = g_bullet_objects.entities[cmd.entity_index].rigid_body;
		btMultiBody* mb = g_bullet_objects.entities[cmd.entity_index].multi_body;

		btVector3 constrain_pos = btVector3( cmd.position.x, cmd.position.y, cmd.position.z );

		if (rb)
		{
			rb->setActivationState( DISABLE_DEACTIVATION );

			btVector3 local_pivot = rb->getCenterOfMassTransform( ).inverse( ) * constrain_pos;

			btPoint2PointConstraint* p2p = new btPoint2PointConstraint( *rb, local_pivot );

			g_bullet_systems.dynamics_world->addConstraint( p2p, true );

			btScalar mousePickClamping = 0.f;
			p2p->m_setting.m_impulseClamp = mousePickClamping;

			//very weak constraint for picking
			p2p->m_setting.m_tau = 10.0f;

            entity->point_constraint = p2p;
		}
		else if (mb)
		{
			btVector3 pivotInA = mb->worldPosToLocal( 1, constrain_pos );

			void* mem = pen::memory_alloc_align( sizeof(btMultiBodyPoint2Point), 16 );
			btMultiBodyPoint2Point* p2p = new( mem ) btMultiBodyPoint2Point( mb, 1, 0, pivotInA, constrain_pos );

			p2p->setMaxAppliedImpulse( 2 );

			g_bullet_systems.dynamics_world->addMultiBodyConstraint( p2p );

            entity->point_constraint_multi = p2p;
		}
	}

	void set_p2p_constraint_pos_internal( const set_v3_params &cmd )
	{
        btPoint2PointConstraint* p2p = g_bullet_objects.entities[cmd.object_index].point_constraint;

        btVector3 bt_v3;
        pen::memory_cpy( &bt_v3, &cmd.data, sizeof( vec3f ) );

        if (p2p)
            p2p->setPivotB( bt_v3 );

        /*
		if (p_p2p->p_point_constraint)
		{
			btPoint2PointConstraint* p_con = p_p2p->p_point_constraint;

			btVector3 constrain_pos = btVector3( cmd.data.x, cmd.data.y, cmd.data.z );
			p_con->setPivotB( constrain_pos );
		}
		else if (p_p2p->p_point_constraint_multi)
		{
			btMultiBodyPoint2Point* p_con = p_p2p->p_point_constraint_multi;

			btVector3 constrain_pos = btVector3( cmd.data.x, cmd.data.y, cmd.data.z );
			p_con->setPivotInB( constrain_pos );
		}
        */
	}

	void remove_p2p_constraint_internal( u32 index )
	{
		generic_constraint* p_p2p = &p2p_constraints[index];

		if (p_p2p->in_use)
		{
			if (p_p2p->p_point_constraint)
			{
				btPoint2PointConstraint* p_con = p_p2p->p_point_constraint;

				g_bullet_systems.dynamics_world->removeConstraint( p_con );

				delete p_con;
				p_con = NULL;
			}
			else if (p_p2p->p_point_constraint_multi)
			{
				btMultiBodyPoint2Point* p_con = p_p2p->p_point_constraint_multi;

				g_bullet_systems.dynamics_world->removeMultiBodyConstraint( p_con );

				pen::memory_free_align( p_con );
				p_con = NULL;
			}

			p_p2p->in_use = 0;
		}
	}

	void set_damping_internal( const set_v3_params &cmd )
	{
		btRigidBody* rb = g_bullet_objects.entities[cmd.object_index].rigid_body;

		rb->setDamping( cmd.data.x, cmd.data.y );
	}

	void set_group_internal( const set_group_params &cmd )
	{
		btRigidBody* rb = g_bullet_objects.entities[cmd.object_index].rigid_body;

		if (rb)
		{
			g_bullet_systems.dynamics_world->removeRigidBody( rb );
			g_bullet_systems.dynamics_world->addRigidBody( rb, cmd.group, cmd.mask );
		}
	}

	void add_collision_watcher_internal( const collision_trigger_data &trigger_data )
	{
		physics_entity pe = g_bullet_objects.entities[trigger_data.entity_index];

		if (pe.rigid_body)
		{
			g_triggers[g_num_triggers].collision_object = ( btCollisionObject* ) pe.rigid_body;
			g_triggers[g_num_triggers].group = trigger_data.group;
			g_triggers[g_num_triggers].mask = trigger_data.mask;
			g_triggers[g_num_triggers].hit_flags = 0;
			g_triggers[g_num_triggers].entity_index = trigger_data.entity_index;
			g_num_triggers++;
		}
	}

	void attach_rb_to_compound_internal( const attach_to_compound_params &params )
	{
		physics_entity& compound = g_bullet_objects.entities[params.compound];
		physics_entity& rb = g_bullet_objects.entities[params.rb];

		if (compound.compound_shape && rb.rigid_body)
		{
			btTransform base = compound.rigid_body->getWorldTransform( );

			if (params.detach_index != -1)
			{
				//to callback later
				rb.p_attach_user_data = params.p_user_data;
				rb.attach_function = params.function;
				rb.attach_shape_index = -1;
				rb.call_attach = 1;

				rb.rigid_body_in_world = 1;

				btTransform compound_child = compound.compound_shape->getChildTransform( params.detach_index );

				compound.compound_shape->removeChildShapeByIndex( params.detach_index );

				g_bullet_systems.dynamics_world->addRigidBody( rb.rigid_body, rb.group, rb.mask );

				rb.rigid_body->setWorldTransform( base * compound_child );
			}
			else
			{
				//to callback later
				rb.p_attach_user_data = params.p_user_data;
				rb.attach_function = params.function;
				rb.attach_shape_index = compound.compound_shape->getNumChildShapes( );
				rb.call_attach = 1;

				rb.rigid_body_in_world = 0;

				btTransform new_child = rb.rigid_body->getWorldTransform( );

				btTransform offset = base.inverse( ) * new_child;

				compound.compound_shape->addChildShape( offset, rb.rigid_body->getCollisionShape( ) );

				g_bullet_systems.dynamics_world->removeRigidBody( rb.rigid_body );
			}
		}
	}

    void release_constraint_internal( u32 entity_index )
    {
        btPoint2PointConstraint* p2p = g_bullet_objects.entities[entity_index].point_constraint;

        if (p2p)
            g_bullet_systems.dynamics_world->removeConstraint( p2p );

        delete p2p;
    }
    
    void release_entity_internal( u32 entity_index )
    {
        //rb
        if(g_bullet_objects.entities[entity_index].rigid_body)
            remove_from_world_internal( entity_index );

        delete g_bullet_objects.entities[entity_index].rigid_body;

        //constraint
        release_constraint_internal( entity_index );
    }

	void remove_from_world_internal( u32 entity_index )
	{
		g_bullet_systems.dynamics_world->removeRigidBody( g_bullet_objects.entities[entity_index].rigid_body );
	}

	void add_to_world_internal( u32 entity_index )
	{
		physics_entity& rb = g_bullet_objects.entities[entity_index];
		g_bullet_systems.dynamics_world->addRigidBody( rb.rigid_body, rb.group, rb.mask );
	}

    void cast_ray_internal( const ray_cast_params& rcp )
    {
        btVector3 from = from_lw_vec3( rcp.start );
        btVector3 to = from_lw_vec3( rcp.end );

        btCollisionWorld::ClosestRayResultCallback ray_callback( from, to );
        
        btVector3 pick_pos = btVector3( 0.0, 0.0, 0.0 );
        s32 user_index = -1;
        g_bullet_systems.dynamics_world->rayTest( from, to, ray_callback );
        if (ray_callback.hasHit())
        {
            pick_pos = ray_callback.m_hitPointWorld;
            btRigidBody* body = ( btRigidBody* )btRigidBody::upcast( ray_callback.m_collisionObject );

            if(body)
                user_index = body->getUserIndex();
        }

        ray_cast_result rcr;
        rcr.point = from_btvector( pick_pos );
        rcr.physics_handle = user_index;

        rcp.callback( rcr );
    }
}

#if 0
virtual bool pickBody( const btVector3& rayFromWorld, const btVector3& rayToWorld )
{
    if (m_dynamicsWorld == 0)
        return false;

    btCollisionWorld::ClosestRayResultCallback rayCallback( rayFromWorld, rayToWorld );

    m_dynamicsWorld->rayTest( rayFromWorld, rayToWorld, rayCallback );
    if (rayCallback.hasHit())
    {

        btVector3 pickPos = rayCallback.m_hitPointWorld;
        btRigidBody* body = ( btRigidBody* )btRigidBody::upcast( rayCallback.m_collisionObject );
        if (body)
        {
            //other exclusions?
            if (!(body->isStaticObject() || body->isKinematicObject()))
            {
                m_pickedBody = body;
                m_savedState = m_pickedBody->getActivationState();
                m_pickedBody->setActivationState( DISABLE_DEACTIVATION );
                btVector3 localPivot = body->getCenterOfMassTransform().inverse() * pickPos;
                btPoint2PointConstraint* p2p = new btPoint2PointConstraint( *body, localPivot );
                m_dynamicsWorld->addConstraint( p2p, true );
                m_pickedConstraint = p2p;
                btScalar mousePickClamping = 30.f;
                p2p->m_setting.m_impulseClamp = mousePickClamping;
                //very weak constraint for picking
                p2p->m_setting.m_tau = 0.001f;
            }
        }


        //??????????pickObject(pickPos, rayCallback.m_collisionObject);
        m_oldPickingPos = rayToWorld;
        m_hitPos = pickPos;
        m_oldPickingDist = (pickPos - rayFromWorld).length();
        //??????????printf("hit !\n");
        //add p2p
    }
    return false;
}

virtual bool movePickedBody( const btVector3& rayFromWorld, const btVector3& rayToWorld )
{
    if (m_pickedBody??&& m_pickedConstraint)
    {
        btPoint2PointConstraint* pickCon = static_cast< btPoint2PointConstraint* >(m_pickedConstraint);
        if (pickCon)
        {
            //keep it at the same picking distance

            btVector3 newPivotB;

            btVector3 dir = rayToWorld - rayFromWorld;
            dir.normalize();
            dir *= m_oldPickingDist;

            newPivotB = rayFromWorld + dir;
            pickCon->setPivotB( newPivotB );
            return true;
        }
    }
    return false;
}

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