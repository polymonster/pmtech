#include "put_math.h"
#include "physics.h"

//for multi body bullet
#include "btBulletDynamicsCommon.h"
#include "BulletDynamics/Featherstone/btMultiBody.h"
#include "BulletDynamics/Featherstone/btMultiBodyConstraintSolver.h"
#include "BulletDynamics/Featherstone/btMultiBodyDynamicsWorld.h"
#include "BulletDynamics/Featherstone/btMultiBodyLinkCollider.h"
#include "BulletDynamics/Featherstone/btMultiBodyLink.h"
#include "BulletDynamics/Featherstone/btMultiBodyJointLimitConstraint.h"
#include "BulletDynamics/Featherstone/btMultiBodyJointMotor.h"
#include "BulletDynamics/Featherstone/btMultiBodyPoint2Point.h"

namespace physics
{
#define MULTIBODY_WORLD
	typedef struct physics_entity
	{
		physics_entity( )
		{
			rigid_body = NULL;
			multi_body = NULL;
			compound_shape = NULL;
			call_attach = 0;
		}

		btRigidBody*											rigid_body;
		u32														rigid_body_in_world;

		btMultiBody*											multi_body;

		btAlignedObjectArray<btMultiBodyJointMotor*>			joint_motors;
		btAlignedObjectArray<btMultiBodyJointLimitConstraint*>	joint_limits;

		btHingeConstraint*										hinge_constraint;
		btGeneric6DofConstraint*								dof6_constraint;
		btFixedConstraint*										fixed_constraint;

		btDefaultMotionState*									default_motion_state;
		btAlignedObjectArray<btMultiBodyLinkCollider*>			link_colliders;
		btCollisionShape*										collision_shape;
		btCompoundShape*										compound_shape;
		u32														num_base_compound_shapes;

		//for attaching and detaching rbs into compounds
		void*													p_attach_user_data;
		s32														attach_shape_index;
		u32														call_attach;
		void( *attach_function )(void* user_data, s32 attach_index);

		u32														group;
		u32														mask;

	}physics_entity;

	typedef struct bullet_systems
	{
		btDefaultCollisionConfiguration*		collision_config;
		btCollisionDispatcher*					dispatcher;
		btBroadphaseInterface*					olp_cache;

#ifdef MULTIBODY_WORLD
		btMultiBodyConstraintSolver*			solver;
		btMultiBodyDynamicsWorld*				dynamics_world;
#else
		btConstraintSolver*						solver;
		btDynamicsWorld*						dynamics_world;
#endif

	}bullet_systems;

	typedef struct generic_constraint
	{
		generic_constraint( )
		{
			in_use = 0;
		}

		a_u32					 in_use;
		btPoint2PointConstraint* p_point_constraint;
		btMultiBodyPoint2Point*	 p_point_constraint_multi;
	}generic_constraint;

#define MAX_PHYSICS_RESOURCES	2048

	typedef struct bullet_objects
	{
		bullet_objects( )
		{
			pen::memory_zero( &entity_allocated[ 0 ],	sizeof(entity_allocated) );
			pen::memory_zero( &entities[ 0 ],			sizeof(entities) );
		};

		a_u8											entity_allocated[MAX_PHYSICS_RESOURCES];
		physics_entity									entities		[MAX_PHYSICS_RESOURCES];

		u32												num_entities;
	}bullet_objects;

#define MAX_TRIGGERS			1024
	typedef struct trigger
	{
		btCollisionObject*		collision_object;
		u32						group;
		u32						mask;
		u32						hit_flags;
		u32						entity_index;
	}trigger;

	typedef struct detach_callback
	{
		void( *attach_function )(void* user_data, s32 attach_index);
		void*   p_attach_user_data;
		s32		attach_shape_index;
		u32		call_attach;

	}detach_callback;

#define MAX_LINKS			32
#define NUM_OUTPUT_BUFFERS	2
#define MAX_OUTPUTS			2048
#define MAX_MULTIS			2048
	typedef struct readable_data
	{
		readable_data()
		{
			current_ouput_backbuffer = 0;
			current_ouput_frontbuffer = 1;

			b_consume = 0;
			b_paused = 0;
		}

		a_u32					current_ouput_backbuffer;
		a_u32					current_ouput_frontbuffer;
		a_u32					b_consume;
		a_u32					b_paused;
        
		mat4					output_matrices[NUM_OUTPUT_BUFFERS][MAX_OUTPUTS];
		mat4					multi_output_matrices[NUM_OUTPUT_BUFFERS][MAX_MULTIS][MAX_LINKS];
		f32						multi_joint_positions[NUM_OUTPUT_BUFFERS][MAX_MULTIS][MAX_LINKS];
		u32						output_hit_flags[NUM_OUTPUT_BUFFERS][MAX_OUTPUTS];
		trigger_contact_data	output_contact_data[NUM_OUTPUT_BUFFERS][MAX_OUTPUTS];
	}readable_data;

	extern readable_data g_readable_data;

	//--------------------------------------------------------------
	//--------------------------------------------------------------
	//--------------------------------------------------------------
	//FUNCTIONS-----------------------------------------------------
    void                physics_update( f32 dt );
    void                physics_initialise( );
    
	btMultiBody*		create_multirb_internal( physics_entity& entity, const multi_body_params &params );
	btRigidBody*		create_rb_internal( physics_entity& entity, const rigid_body_params &params, u32 ghost, btCollisionShape* p_existing_shape = NULL );
    
	void				add_rb_internal( const rigid_body_params &params, u32 resource_slot, bool ghost = false );
	void				add_compound_rb_internal( const compound_rb_params &params, u32 resource_slot );
	void				add_compound_shape_internal( const compound_rb_params &params, u32 resource_slot );
	void				add_dof6_internal( const constraint_params &params, u32 resource_slot, btRigidBody* rb, btRigidBody* fixed_body );
	void				add_multibody_internal( const multi_body_params &params, u32 resource_slot );
	void				add_hinge_internal( const constraint_params &params, u32 resource_slot );
	void				add_constrained_rb_internal( const constraint_params &params, u32 resource_slot );
    
	void				set_linear_velocity_internal( const set_v3_params &cmd );
	void				set_angular_velocity_internal( const set_v3_params &cmd );
	void				set_linear_factor_internal( const set_v3_params &cmd );
	void				set_angular_factor_internal( const set_v3_params &cmd );
	void				set_transform_internal( const set_transform_params &cmd );
	void				set_gravity_internal( const set_v3_params &cmd );
	void				set_friction_internal( const set_float_params &cmd );
	void				set_hinge_motor_internal( const set_v3_params &cmd );
	void				set_button_motor_internal( const set_v3_params &cmd );
	void				set_multi_joint_motor_internal( const set_multi_v3_params &cmd );
	void				set_multi_joint_pos_internal( const set_multi_v3_params &cmd );
	void				set_multi_joint_limit_internal( const set_multi_v3_params &cmd );
	void				set_multi_base_velocity_internal( const set_multi_v3_params &cmd );
	void				set_multi_base_pos_internal( const set_multi_v3_params &cmd );
	void				set_group_internal( const set_group_params &cmd );
	void				set_damping_internal( const set_v3_params &cmd );

	void				sync_rigid_bodies_internal( const sync_rb_params &cmd );
	void				sync_rigid_body_velocity_internal( const sync_rb_params &cmd );
	void				sync_compound_multi_internal( const sync_compound_multi_params &cmd );
	void				attach_rb_to_compound_internal( const attach_to_compound_params &params );

	void				add_to_world_internal( u32 entity_index );
	void				remove_from_world_internal( u32 entity_index );
    
    void                release_entity_internal( u32 entity_index );

	void				add_p2p_constraint_internal( const add_p2p_constraint_params &cmd );
	void				remove_p2p_constraint_internal( u32 index );
	void				set_p2p_constraint_pos_internal( const set_v3_params &cmd );
	u32					assign_next_free_p2p( );

	void				add_collision_watcher_internal( const collision_trigger_data &trigger_data );

}
