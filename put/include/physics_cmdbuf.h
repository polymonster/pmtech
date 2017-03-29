#ifndef _phyiscs_cmdbuf_h
#define _phyiscs_cmdbuf_h

#include "threads.h"
#include "polyspoon_math.h"

namespace physics
{
	PEN_THREAD_RETURN physics_thread_main( void* params );

    enum e_physics_cmd : s32
	{
		CMD_SET_LINEAR_VELOCITY = 1,
		CMD_SET_ANGULAR_VELOCITY,
		CMD_SET_LINEAR_FACTOR,
		CMD_SET_ANGULAR_FACTOR,
		CMD_SET_TRANSFORM,
		CMD_ADD_RIGID_BODY,
		CMD_ADD_GHOST_RIGID_BODY,
		CMD_ADD_CONSTRAINED_RB,
		CMD_ADD_MULTI_BODY,
		CMD_ADD_COMPOUND_RB,
		CMD_ADD_COMPOUND_SHAPE,
		CMD_SET_GRAVITY,
		CMD_SET_FRICTION,
		CMD_SET_HINGE_MOTOR,
		CMD_SET_BUTTON_MOTOR,
		CMD_SET_MULTI_JOINT_MOTOR,
		//uses v.x to represent rotation about the joints axis
		CMD_SET_MULTI_JOINT_POS,
		CMD_SET_MULTI_JOINT_LIMITS,
		CMD_SET_MULTI_BASE_VELOCITY,
		CMD_SET_MULTI_BASE_POS,
		CMD_SYNC_COMPOUND_TO_MULTI,
		CMD_SYNC_RIGID_BODY_TRANSFORM,
		CMD_SYNC_RIGID_BODY_VELOCITY,
		CMD_ADD_P2P_CONSTRAINT,
		CMD_REMOVE_P2P_CONSTRAINT,
		CMD_SET_P2P_CONSTRAINT_POS,
		CMD_SET_DAMPING,
		CMD_SET_GROUP,
		CMD_REMOVE_FROM_WORLD,
		CMD_ADD_TO_WORLD,
		CMD_ADD_COLLISION_TRIGGER,
		CMD_ATTACH_RB_TO_COMPOUND
	};

    enum e_physics_shape : s32
	{
		BOX = 1,
		CYLINDER,
		SPHERE,
		CAPSULE,
		HULL,
		MESH,
		COMPOUND
	};

    enum e_physics_constraint : s32
	{
		DOF6 = 1,
		DOF6_NOCREATE,
		DOF6_RB,
		HINGE,
	};

    enum e_multibody_link_type : s32
	{
		REVOLUTE = 1,
		FIXED,
	};

    enum e_up_axis : s32
	{
		UP_Y = 0,
		UP_X = 1,
		UP_Z = 2,
	};

	typedef struct lw_vec3f
	{
		f32 x, y, z;
	}lw_vec3f;

	typedef struct collision_response
	{
		s32 hit_tag;
		u32 collider_flags;
	}collision_response;

	inline void set_lw_vec3f( lw_vec3f *plwv, vec3f v )
	{
		pen::memory_cpy( plwv, &v, sizeof( vec3f ) );
	}

	typedef struct collision_trigger_data
	{
		u32 entity_index;
		u32 group;
		u32 mask;
	}collision_trigger_data;

	typedef struct collision_mesh_data
	{
		f32* vertices;
		u32* indices;
		u32  num_floats;
		u32  num_indices;
	}collision_mesh_data;

	typedef struct rigid_body_params
	{
		lw_vec3f position;
		lw_vec3f dimensions;
		u32		 shape_up_axis;
		quat	 rotation;
		u32		 shape;
		f32		 mass;
		u32		 group;
		u32		 mask;
		collision_mesh_data mesh_data;
	}rigid_body_params;

	typedef struct compound_rb_params
	{
		rigid_body_params  base;
		rigid_body_params* rb;
		u32				   num_shapes;
	}compound_rb_params;

	typedef struct multi_body_link
	{
		rigid_body_params	rb;
		lw_vec3f			hinge_axis;
		lw_vec3f			hinge_offset;
		lw_vec3f			hinge_limits;
		u32					link_type;
		s32					parent;
		u32					transform_world_to_local;
		u32					joint_motor;
		u32					joint_limit_constraint;
		u32					compound_index;
		compound_rb_params	compound_shape;
	} multi_body_link;

	typedef struct multi_body_params
	{
		rigid_body_params	base;
		multi_body_link*	links;
		u32					multi_dof;

		u32 num_links;
	}multi_body_params;

	typedef struct constraint_params
	{
		rigid_body_params rb;

		u32 type;

		lw_vec3f axis;
		lw_vec3f pivot;

		lw_vec3f lower_limit_translation;
		lw_vec3f upper_limit_translation;

		lw_vec3f lower_limit_rotation;
		lw_vec3f upper_limit_rotation;

		f32		linear_damping;
		f32		angular_damping;

		s32		rb_indices[ 2 ];

	}slider_constraint_params;

	typedef struct attach_to_compound_params
	{
		u32		rb;
		u32		compound;
		u32		parent;
		s32		detach_index;
		void	(*function)(  void* user_data, s32 attach_index );
		void*	p_user_data;

	}attach_to_compound_params;

	typedef struct  add_p2p_constraint_params
	{
		lw_vec3f position;
		u32		 entity_index;
		s32		 link_index;
		u32		 p2p_index;
	}add_p2p_constraint_params;

	typedef struct add_box_params
	{
		lw_vec3f dimensions;
		lw_vec3f position;
		quat	 rotation;
		f32		 mass;
	}add_box_params;

	typedef struct set_v3_params
	{
		u32 object_index;
		lw_vec3f data;
	}set_v3_params;

	typedef struct set_multi_v3_params
	{
		u32 multi_index;
		u32 link_index;
		lw_vec3f data;
	}set_multi_v3_params;

	typedef struct set_float_params
	{
		u32 object_index;
		f32 data;
	}set_float_params;

	typedef struct set_transform_params
	{
		u32		 object_index;
		lw_vec3f position;
		quat	 rotation;
	}set_transform_params;

	typedef struct sync_compound_multi_params
	{
		u32 compound_index;
		u32 multi_index;
	}sync_compound_multi_params;

	typedef struct set_damping_params
	{
		u32 object_index;
		lw_vec3f linear;
		lw_vec3f angular;
	}set_damping_params;

	typedef struct set_group_params
	{
		u32 object_index;
		u32 group;
		u32 mask;
	}set_group_params;

	typedef struct  sync_rb_params
	{
		u32 master;
		u32 slave;
		s32 link_index;
	}sync_rb_params;

	typedef struct physics_cmd
	{
		u32		command_index;

		union
		{
			add_box_params					add_box;
			set_v3_params					set_v3;
			set_transform_params			set_transform;
			set_float_params 				set_float;
			rigid_body_params				add_rb;
			constraint_params				add_constained_rb;
			multi_body_params				add_multi;
			set_multi_v3_params				set_multi_v3;
			compound_rb_params				add_compound_rb;
			sync_compound_multi_params		sync_compound;
			sync_rb_params					sync_rb;
			u32								entity_index;
			set_damping_params				set_damping;
			set_group_params				set_group;
			collision_trigger_data			trigger_data;
			attach_to_compound_params		attach_compound;
			add_p2p_constraint_params		add_p2p;
		};
	}physics_cmd;

#define MAX_TRIGGER_CONTACTS	8	
	typedef struct trigger_contact_data
	{
		u32		num;
		u32		flag	[MAX_TRIGGER_CONTACTS];
		u32		entity	[MAX_TRIGGER_CONTACTS];
		vec3f	normals	[MAX_TRIGGER_CONTACTS];
		vec3f	pos		[MAX_TRIGGER_CONTACTS];
	}trigger_contact_data;

	//
	void	set_consume( u32 val );	
	void	set_paused( u32 val );	

	//add
	u32		add_rb( const rigid_body_params &rbp );
	u32		add_ghost_rb( const rigid_body_params &rbp );
	u32		add_constrained_rb( const constraint_params &crbp );
	u32		add_multibody( const multi_body_params &mbp );
	u32		add_compound_rb( const compound_rb_params &crbp );
	u32		add_compound_shape( const compound_rb_params &crbp );
	u32		add_p2p_constraint( const add_p2p_constraint_params &params );
	u32		attach_rb_to_compound( const attach_to_compound_params &params  );
	u32		detach_all_from_compound( u32 compound_index );

	void	add_collision_trigger( const collision_trigger_data &trigger_data );
	
	//remove
	void	remove_p2p_constraint( u32 p2p_constraint );
	
	//set
	void	set_v3( const u32 &entity_index, const vec3f &velocity, u32 cmd );
	void	set_float( const u32 &entity_index, const f32 &fval, u32 cmd );
	void	set_transform( const u32 &entity_index, const vec3f &position, const quat &quaternion );
	void	set_multi_v3( const u32 &entity_index, const u32 &link_index, const vec3f &v3_data, const u32 &cmd );
	void	set_collision_group( const u32 &entity_index, const u32 &group, const u32 &mask );
	void	remove_from_world( const u32 &entity_index );
	void	add_to_world( const u32 &entity_index );

	void	sync_compound_multi( const u32 &compound_index, const u32 &multi_index );
	void	sync_rigid_bodies( const u32 &master, const u32 &slave, const s32 &link_index, u32 cmd );

	//get
	mat4	get_rb_matrix( const u32 &entity_index );
	mat4	get_multirb_matrix( const u32 &multi_index, const s32 &link_index );
	f32		get_multi_joint_pos( const u32 &multi_index, const s32 &link_index );
	u32		get_hit_flags( u32 entity_index );

	trigger_contact_data* get_trigger_contacts( u32 entity_index );

	void	reset_wait_flag();
	void	wait_complete( a_u32& result );
}

#endif
