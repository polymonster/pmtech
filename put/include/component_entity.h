#ifndef component_entity_h__
#define component_entity_h__

#include "definitions.h"
#include "put_math.h"
#include "camera.h"
#include "loader.h"
#include "physics_cmdbuf.h"

#define MAX_SUBMESHES			32
#define MAX_SCENE_NODE_CHARS	1024
#define MAX_SUBS_COMPOUND		32

#define BUTTON_LOCK_DOWN		(1)
#define BUTTON_LOCK_UP			(1<<1)

#define BUTTON_DOWN				1
#define BUTTON_UP				2
#define BUTTON_TRANS			3


namespace put
{
	namespace ces
	{
		enum e_component_flags : u32
		{
			CMP_GEOMETRY		= (1 << 1),
			CMP_PHYSICS			= (1 << 2),
			CMP_PHYSICS_MULTI	= (1 << 3),
			CMP_MATERIAL		= (1 << 4),
			CMP_HAND			= (1 << 5),
			CMP_SKINNED			= (1 << 6),
			CMP_BONE			= (1 << 7),
            CMP_DYNAMIC			= (1 << 8)
		};

		enum e_node_types : u32
		{
			NODE_TYPE_NONE = 0,
			NODE_TYPE_JOINT = 1,
			NODE_TYPE_GEOM = 2
		};

		struct scene_node_skin
		{
			u32		num_joints;
			mat4	bind_shape_matirx;
			mat4	joint_bind_matrices[25];
			mat4	joint_matrices[25];
		};

		struct scene_node_geometry
		{
			u32					position_buffer;
			u32					vertex_buffer;
			u32					pre_skin_vertex_buffer;
			u32					skinned_position;
			u32					index_buffer;
			u32					num_indices;
			u32					num_vertices;
			u32					index_type;
			u32					submesh_material_index;
			scene_node_skin*	p_skin;
		};

		enum scene_node_textures
		{
			SN_DIFFUSE,
			SN_NORMAL_MAP,
			SN_SPECULAR_MAP,

			SN_NUM_TEXTURES
		};
        
        enum scene_render_type
        {
            SN_RENDER_LIT,
            SN_RENDER_DEPTH,
            SN_RENDER_DEBUG
        };

		struct scene_node_material
		{
			s32		texture_id[SN_NUM_TEXTURES] = { 0 };
			vec4f	diffuse_rgb_shininess = vec4f(1.0f, 1.0f, 1.0f, 0.5f);
			vec4f	specular_rgb_reflect = vec4f(1.0f, 1.0f, 1.0f, 0.5f);
		};

		struct scene_node_physics
		{
			vec3f min_extents;
			vec3f max_extents;
			vec3f centre;
			u32	  collision_shape;
			u32   collision_dynamic;
			vec3f start_position;
			quat  start_rotation;

			physics::collision_mesh_data mesh_data;
		};

		struct slider
		{
			vec3f limit_min;
			vec3f limit_max;
			f32	  direction;
			u32   entity_index;
		};

		struct button
		{
			vec3f limit_up;
			vec3f limit_down;
			vec3f offset;
			f32	  spring;
			u32	  entity_index;
			u32	  lock_state;
			u32	  debounce = 0;
		};

		struct component_entity_scene
		{
			u32*					id_name;
			u32*					id_geometry;
			u32*					id_material;

			a_u64*					entities;
			u32*					parents;
			scene_node_geometry*	geometries;
			scene_node_material*	materials;
			mat4*					local_matrices;
			mat4*					world_matrices;
			mat4*					offset_matrices;
			mat4*					physics_matrices;
			u32*					physics_handles;
			u32*					multibody_handles;
			s32*					multibody_link;
			scene_node_physics*		physics_data;
            u32*                    cbuffer;

			u32						num_nodes = 0;
			u32						nodes_size = 0;

			//debug / display info
			c8**					names;
			c8**					geometry_names;
			c8**					material_names;
            s32                     selected_index;
		};


		struct vertex_model
		{
			f32 x, y, z, w;
			f32 nx, ny, nz, nw;
			f32 u, v, _u, _v;
			f32 tx, ty, tz, tw;
			f32 bx, by, bz, bw;
		};

		struct vertex_model_skinned
		{
			f32 x, y, z, w;
			f32 nx, ny, nz, nw;
			f32 u, v, _u, _v;
			f32 tx, ty, tz, tw;
			f32 bx, by, bz, bw;
			u32 i1, i2, i3, i4;
			f32 w1, w2, w3, w4;
		};

		struct vertex_position
		{
			f32 x, y, z, w;
		};

		struct scene_view
		{
			u32 cb_view;
			u32 scene_node_flags = 0;
			u32 debug_flags = 0;

			component_entity_scene* scene = nullptr;
		};

		component_entity_scene*	create_scene( const c8* name );

		void					import_model_scene( const c8* model_scene_name, component_entity_scene* scene );

		void					clone_node( component_entity_scene* scene, u32 src, u32 dst, s32 parent, vec3f offset = vec3f::zero(), const c8* suffix = "_cloned");

		void					enumerate_scene_ui(component_entity_scene* scene, bool* open );

		void					render_scene_view(const scene_view& view, scene_render_type render_type_flags );

		void					render_scene_debug(component_entity_scene* scene, const scene_view& view);

		void					update_scene_matrices(component_entity_scene* scene);

		/*
		scene_nodes*			get_scene_nodes();
		u32						get_num_nodes();
		void					get_rb_params_from_node(u32 node_index, f32 mass, physics::rigid_body_params &rbp);

		void					create_compound_from_nodes(physics::compound_rb_params &crbp, const c8* compound_names, u32 clone = 0, c8* suffix = "", vec3f offset = vec3f::zero());
		void					create_compound_rb(const c8* compound_name, const c8* base_name, f32 mass, u32 group, u32 mask, u32 &rb_index);

		void					create_physics_object(u32 node_index, physics::constraint_params* p_constraint, f32 mass, u32 group, u32 mask, vec3f scale_dimensions = vec3f(1.0f, 1.0f, 1.0f));

		u32						create_slider_widget(u32 node_index, u32 start_index, u32 end_index, f32 mass, f32 invert_max, vec3f &slider_start, vec3f &slider_end, u32 &rb_handle);
		f32						get_slider_ratio(slider slider_node);

		u32						create_button_widget(u32 node_index, u32 up_index, u32 down_index, f32 mass, f32 spring, button &button_data);
		u32						check_button_state(button &button_data, u32 button_lock, f32 down_epsilon, f32 up_epsilon);
		void					release_button(button &button_data);

		u32						create_rotary_widget(u32 node_index, f32 mass, vec3f axis, vec3f pivot, u32 &rb_handle, vec2f limits = vec2f(-FLT_MAX, FLT_MAX));

		a_u64*					get_sn_entityflags(u32 index);

		scene_node_physics*		get_sn_physics(u32 index);
		scene_node_geometry*	get_sn_geometry(u32 index);
		scene_node_material*	get_sn_material(u32 index);

		s32*					get_sn_multibodylink(u32 index);

		u32*					get_sn_physicshandle(u32 index);
		u32*					get_sn_parents(u32 index);
		u32*					get_sn_multibodyhandle(u32 index);

		mat4*					get_sn_offsetmat(u32 index);
		mat4*					get_sn_worldmat(u32 index);
		mat4*					get_sn_localmat(u32 index);

		c8*						get_sn_name(u32 index);

		s32						get_scene_node_by_name(const c8* name);
		s32						get_cloned_scene_node_by_name(const c8* name, const c8* suffix, vec3f offset, u32 disble_src);

		void					clone_node(u32 src, u32 dst, s32 parent, vec3f offset = vec3f::zero(), const c8* suffix = "_cloned");

		void					hide_node(u32 node_index, u32 remove_physics);
		void					show_node(u32 node_index, u32 add_physics);
		*/
	}
}

#endif
