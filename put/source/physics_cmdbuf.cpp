#include "pen_string.h"
#include "timer.h"

#include "physics_cmdbuf.h"
#include "internal/physics_internal.h"

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

#define MAX_COMMANDS 2048
#define INC_WRAP( V ) V = (V+1)%MAX_COMMANDS

	physics_cmd cmd_buffer[MAX_COMMANDS];
	u32 put_pos = 0;
	u32 get_pos = 0;
		
	void exec_cmd( const physics_cmd &cmd )
	{
		switch (cmd.command_index)
		{
		case CMD_SET_LINEAR_VELOCITY:
			set_linear_velocity_internal( cmd.set_v3 );
			break;

		case CMD_SET_ANGULAR_VELOCITY:
			set_linear_velocity_internal( cmd.set_v3 );
			break;

		case CMD_SET_LINEAR_FACTOR:
			set_linear_factor_internal( cmd.set_v3 );
			break;

		case CMD_SET_ANGULAR_FACTOR:
			set_angular_factor_internal( cmd.set_v3 );
			break;

		case CMD_SET_TRANSFORM:
			set_transform_internal( cmd.set_transform );
			break;

		case CMD_ADD_RIGID_BODY:
			add_rb_internal( cmd.add_rb );
			break;

		case CMD_ADD_GHOST_RIGID_BODY:
			add_rb_internal( cmd.add_rb, true );
			break;

		case CMD_ADD_CONSTRAINED_RB:
			add_constrained_rb_internal( cmd.add_constained_rb );
			break;

		case CMD_SET_GRAVITY:
			set_gravity_internal( cmd.set_v3 );
			break;

		case CMD_ADD_MULTI_BODY:
			add_multibody_internal( cmd.add_multi );
			break;

		case CMD_SET_FRICTION:
			set_friction_internal( cmd.set_float );
			break;

		case CMD_SET_HINGE_MOTOR:
			set_hinge_motor_internal( cmd.set_v3 );
			break;

		case CMD_SET_BUTTON_MOTOR:
			set_button_motor_internal( cmd.set_v3 );
			break;

		case CMD_SET_MULTI_JOINT_MOTOR:
			set_multi_joint_motor_internal( cmd.set_multi_v3 );
			break;

		case CMD_SET_MULTI_JOINT_POS:
			set_multi_joint_pos_internal( cmd.set_multi_v3 );
			break;

		case CMD_SET_MULTI_JOINT_LIMITS:
			set_multi_joint_limit_internal( cmd.set_multi_v3 );
			break;

		case CMD_SET_MULTI_BASE_VELOCITY:
			set_multi_base_velocity_internal( cmd.set_multi_v3 );
			break;

		case CMD_SET_MULTI_BASE_POS:
			set_multi_base_pos_internal( cmd.set_multi_v3 );
			break;

		case CMD_ADD_COMPOUND_RB:
			add_compound_rb_internal( cmd.add_compound_rb );
			break;

		case CMD_SYNC_COMPOUND_TO_MULTI:
			sync_compound_multi_internal( cmd.sync_compound );
			break;

		case CMD_SYNC_RIGID_BODY_TRANSFORM:
			sync_rigid_bodies_internal( cmd.sync_rb );
			break;

		case CMD_SYNC_RIGID_BODY_VELOCITY:
			sync_rigid_body_velocity_internal( cmd.sync_rb );
			break;

		case CMD_ADD_P2P_CONSTRAINT:
			add_p2p_constraint_internal( cmd.add_p2p );
			break;

		case CMD_SET_P2P_CONSTRAINT_POS:
			set_p2p_constraint_pos_internal( cmd.set_v3 );
			break;

		case CMD_REMOVE_P2P_CONSTRAINT:
			remove_p2p_constraint_internal( cmd.entity_index );
			break;

		case CMD_SET_DAMPING:
			set_damping_internal( cmd.set_v3 );
			break;

		case CMD_SET_GROUP:
			set_group_internal( cmd.set_group );
			break;

		case CMD_ADD_COMPOUND_SHAPE:
			add_compound_shape_internal( cmd.add_compound_rb );
			break;

		case CMD_ADD_COLLISION_TRIGGER:
			add_collision_watcher_internal( cmd.trigger_data );
			break;

		case CMD_ATTACH_RB_TO_COMPOUND:
			attach_rb_to_compound_internal( cmd.attach_compound );
			break;

		case CMD_REMOVE_FROM_WORLD:
			remove_from_world_internal( cmd.entity_index );
			break;

		case CMD_ADD_TO_WORLD:
			add_to_world_internal( cmd.entity_index );
			break;

		default:
			break;
		}
	}

	void consume_cmd_buf( )
	{
		u32 end_pos = put_pos;
		while (get_pos != end_pos)
		{
			exec_cmd( cmd_buffer[get_pos] );

			INC_WRAP( get_pos );
		}
	}

	void set_v3( const u32 &entity_index, const vec3f &velocity, u32 cmd )
	{
		cmd_buffer[put_pos].command_index = cmd;
		pen::memory_cpy( &cmd_buffer[put_pos].set_v3.data, &velocity, sizeof(vec3f) );
		cmd_buffer[put_pos].set_v3.object_index = entity_index;

		INC_WRAP( put_pos );
	}

	void set_float( const u32 &entity_index, const f32 &fval, u32 cmd )
	{
		cmd_buffer[put_pos].command_index = cmd;
		pen::memory_cpy( &cmd_buffer[put_pos].set_float.data, &fval, sizeof(f32) );
		cmd_buffer[put_pos].set_float.object_index = entity_index;

		INC_WRAP( put_pos );
	}

	void set_transform( const u32 &entity_index, const vec3f &position, const quat &quaternion )
	{
		cmd_buffer[put_pos].command_index = CMD_SET_TRANSFORM;
		pen::memory_cpy( &cmd_buffer[put_pos].set_transform.position, &position, sizeof(vec3f) );
		pen::memory_cpy( &cmd_buffer[put_pos].set_transform.rotation, &quaternion, sizeof(quat) );
		cmd_buffer[put_pos].set_transform.object_index = entity_index;

		INC_WRAP( put_pos );
	}

	mat4 get_rb_matrix( const u32 &entity_index )
	{
		return g_readable_data.output_matrices[ g_readable_data.current_ouput_frontbuffer ][ entity_index ];
	}

	mat4 get_multirb_matrix( const u32 &multi_index, const s32 &link_index )
	{
		s32 internal_link_index = link_index + 1;
		return g_readable_data.multi_output_matrices[ g_readable_data.current_ouput_frontbuffer ][ multi_index ][ internal_link_index ];
	}

	f32 get_multi_joint_pos( const u32 &multi_index, const s32 &link_index )
	{
		s32 internal_link_index = link_index + 1;
		return g_readable_data.multi_joint_positions[g_readable_data.current_ouput_frontbuffer][multi_index][internal_link_index];
	}

	trigger_contact_data* get_trigger_contacts( u32 entity_index )
	{
		return &g_readable_data.output_contact_data[g_readable_data.current_ouput_frontbuffer][entity_index];
	}

	u32  add_rb( const rigid_body_params &rbp )
	{
		cmd_buffer[put_pos].command_index = CMD_ADD_RIGID_BODY;
		cmd_buffer[put_pos].add_rb = rbp;

		INC_WRAP( put_pos );

		return get_next_free_entity_index( DOMAIN_DEFER );
	}

	u32 add_ghost_rb( const rigid_body_params &rbp )
	{
		cmd_buffer[put_pos].command_index = CMD_ADD_GHOST_RIGID_BODY;
		cmd_buffer[put_pos].add_rb = rbp;

		INC_WRAP( put_pos );

		return get_next_free_entity_index( DOMAIN_DEFER );
	}

	u32  add_constrained_rb( const constraint_params &crbp )
	{
		cmd_buffer[put_pos].command_index = CMD_ADD_CONSTRAINED_RB;
		cmd_buffer[put_pos].add_constained_rb = crbp;

		INC_WRAP( put_pos );

		return get_next_free_entity_index( DOMAIN_DEFER );
	}

	u32 add_multibody( const multi_body_params &mbp )
	{
		cmd_buffer[put_pos].command_index = CMD_ADD_MULTI_BODY;
		cmd_buffer[put_pos].add_multi = mbp;

		INC_WRAP( put_pos );

		return get_next_free_entity_index( DOMAIN_DEFER );
	}

	void set_consume( u32 val )
	{
		g_readable_data.b_consume = val;
	}

	void set_paused( u32 val )
	{
		g_readable_data.b_paused = val;
	}

	void set_multi_v3( const u32 &object_index, const u32 &link_index, const vec3f &v3_data, const u32 &cmd )
	{
		cmd_buffer[put_pos].command_index = cmd;

		pen::memory_cpy( &cmd_buffer[put_pos].set_multi_v3.data, &v3_data, sizeof(vec3f) );
		cmd_buffer[put_pos].set_multi_v3.multi_index = object_index;
		cmd_buffer[put_pos].set_multi_v3.link_index = link_index;

		INC_WRAP( put_pos );
	}

	u32 add_compound_rb( const compound_rb_params &crbp )
	{
		cmd_buffer[put_pos].command_index = CMD_ADD_COMPOUND_RB;

		cmd_buffer[put_pos].add_compound_rb = crbp;

		INC_WRAP( put_pos );

		return get_next_free_entity_index( DOMAIN_DEFER );
	}

	void sync_compound_multi( const u32 &compound_index, const u32 &multi_index )
	{
		cmd_buffer[put_pos].command_index = CMD_SYNC_COMPOUND_TO_MULTI;

		cmd_buffer[put_pos].sync_compound.compound_index = compound_index;
		cmd_buffer[put_pos].sync_compound.multi_index = multi_index;

		INC_WRAP( put_pos );
	}


	void sync_rigid_bodies( const u32 &master, const u32 &slave, const s32 &link_index, u32 cmd )
	{
		cmd_buffer[put_pos].command_index = cmd;

		cmd_buffer[put_pos].sync_rb.master = master;
		cmd_buffer[put_pos].sync_rb.slave = slave;
		cmd_buffer[put_pos].sync_rb.link_index = link_index;

		INC_WRAP( put_pos );
	}

	u32 add_p2p_constraint( const add_p2p_constraint_params &params )
	{
		cmd_buffer[put_pos].command_index = CMD_ADD_P2P_CONSTRAINT;

		cmd_buffer[put_pos].add_p2p = params;

		u32 p2p_index = assign_next_free_p2p( );
		cmd_buffer[put_pos].add_p2p.p2p_index = p2p_index;

		INC_WRAP( put_pos );

		return p2p_index;
	}

	void remove_p2p_constraint( u32 p2p_constraint )
	{
		cmd_buffer[put_pos].command_index = CMD_REMOVE_P2P_CONSTRAINT;

		cmd_buffer[put_pos].entity_index = p2p_constraint;

		INC_WRAP( put_pos );
	}

	void set_collision_group( const u32 &object_index, const u32 &group, const u32 &mask )
	{
		cmd_buffer[put_pos].command_index = CMD_SET_GROUP;

		cmd_buffer[put_pos].set_group.group = group;
		cmd_buffer[put_pos].set_group.mask = mask;

		INC_WRAP( put_pos );
	}

	u32 add_compound_shape( const compound_rb_params &crbp )
	{
		cmd_buffer[put_pos].command_index = CMD_ADD_COMPOUND_SHAPE;

		cmd_buffer[put_pos].add_compound_rb = crbp;

		INC_WRAP( put_pos );

		return get_next_free_entity_index( DOMAIN_DEFER );
	}

	void add_collision_trigger( const collision_trigger_data &trigger_data )
	{
		cmd_buffer[put_pos].command_index = CMD_ADD_COLLISION_TRIGGER;

		cmd_buffer[put_pos].trigger_data = trigger_data;

		INC_WRAP( put_pos );
	}

	u32 get_hit_flags( u32 entity_index )
	{
		u32 r = g_readable_data.output_hit_flags[ g_readable_data.current_ouput_frontbuffer ][ entity_index ];
		return r;
	}

	u32 attach_rb_to_compound( const attach_to_compound_params &params )
	{
		cmd_buffer[put_pos].command_index = CMD_ATTACH_RB_TO_COMPOUND;

		cmd_buffer[put_pos].attach_compound = params;

		INC_WRAP( put_pos );

		return 0;
	}

	void remove_from_world( const u32 &entity_index )
	{
		cmd_buffer[put_pos].command_index = CMD_REMOVE_FROM_WORLD;

		cmd_buffer[put_pos].entity_index = entity_index;

		INC_WRAP( put_pos );

	}

	void add_to_world( const u32 &entity_index )
	{
		cmd_buffer[put_pos].command_index = CMD_ADD_TO_WORLD;

		cmd_buffer[put_pos].entity_index = entity_index;

		INC_WRAP( put_pos );
	}

	void reset_wait_flag( )
	{
		g_readable_data.b_wait_flag = 0;
	}

	void wait_complete( a_u32& result )
	{
		if ( g_readable_data.b_wait_flag )
		{
            result = 1;
		}

        result = 0;
	}
}
