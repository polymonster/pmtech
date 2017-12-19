#ifndef ces_scene_h__
#define ces_scene_h__

#include "definitions.h"
#include "put_math.h"
#include "camera.h"
#include "loader.h"
#include "physics_cmdbuf.h"
#include "str/Str.h"
#include "pmfx.h"

#include <vector>

#define CES_DEBUG
#define INVALID_HANDLE -1

#ifdef CES_DEBUG
#define ASSIGN_DEBUG_NAME( D, S ) D = S
#else
#define ASSIGN_DEBUG_NAME( D, S )
#endif

namespace put
{
    typedef s32 anim_handle;
    
	namespace ces
	{
        enum e_scene_view_flags : u32
        {
            SV_NONE = 0,
            SV_HIDE = (1<<0),
            SV_BITS_END = 0
        };

		enum e_scene_flags : u32
		{
			INVALIDATE_NONE = 0,
			INVALIDATE_SCENE_TREE = 1<<1,
            PAUSE_UPDATE = 1<<2
		};
        
		enum e_component_flags : u32
		{
            CMP_ALLOCATED       = (1 << 0),
			CMP_GEOMETRY		= (1 << 1),
			CMP_PHYSICS			= (1 << 2),
			CMP_PHYSICS_MULTI	= (1 << 3),
			CMP_MATERIAL		= (1 << 4),
			CMP_HAND			= (1 << 5),
			CMP_SKINNED			= (1 << 6),
			CMP_BONE			= (1 << 7),
            CMP_DYNAMIC			= (1 << 8),
            CMP_ANIM_CONTROLLER = (1 << 9),
            CMP_ANIM_TRAJECTORY = (1 << 10),
            CMP_LIGHT           = (1 << 11),
			CMP_TRANSFORM		= (1 << 12)
		};

        enum e_light_types : u32
        {
            LIGHT_TYPE_DIR = 0,
            LIGHT_TYPE_POINT = 1,
            LIGHT_TYPE_SPOT = 2
        };
        
        struct per_model_cbuffer
        {
            mat4 world_matrix;
            vec4f info;
        };

		struct scene_node_skin
		{
			u32		num_joints;
			mat4	bind_shape_matirx;
			mat4	joint_bind_matrices[85];
            u32     bone_cbuffer = PEN_INVALID_HANDLE;
		};
        
        enum scene_node_textures
        {
            SN_ALBEDO_MAP = 0,
            SN_NORMAL_MAP,
            SN_SPECULAR_MAP,
            SN_EMISSIVE_MAP,
            
            SN_NUM_TEXTURES
        };
        
        struct scene_node_material
        {
            s32      texture_id[SN_NUM_TEXTURES] = { 0 };
            vec4f    diffuse_rgb_shininess = vec4f(1.0f, 1.0f, 1.0f, 0.5f);
            vec4f    specular_rgb_reflect = vec4f(1.0f, 1.0f, 1.0f, 0.5f);
        };
        
        struct scene_node_physics
        {
            vec3f min_extents;
            vec3f max_extents;
            vec3f centre;
            u32   collision_shape = 0;
            vec3f start_position;
            quat  start_rotation;
            f32   mass = 0.0f;
            physics::collision_mesh_data mesh_data;
        };
        
        struct bounding_volume
        {
            vec3f min_extents;
            vec3f max_extents;
            
            vec3f transformed_min_extents;
            vec3f transformed_max_extents;
            f32   radius;
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
            u32                 vertex_size;
			scene_node_skin*	p_skin;
		};

        struct animation_controller
        {
            std::vector<anim_handle> handles;
            s32                      joints_offset;
            anim_handle              current_animation;
            f32                      current_time;
            s32                      current_frame = 0;
            u8                       play_flags = 0;
            bool                     apply_root_motion = true;
        };
        
        struct scene_node_light
        {
            u32     type;
            vec3f   colour;
            vec4f   data;
        };

		struct transform
		{
			vec3f	translation;
			quat	rotation;
			vec3f	scale;
		};

		struct forward_light
		{
			vec4f pos_radius;
			vec4f colour;
		};
        
#define MAX_FORWARD_LIGHTS 8
        struct forward_light_buffer
        {
            forward_light lights[MAX_FORWARD_LIGHTS];
            vec4f         info;
        };

		struct entity_scene
		{
            u32                     num_nodes = 0;
            u32                     nodes_size = 0;

            a_u64*                  entities;

			hash_id*				id_name;
			hash_id*				id_geometry;
			hash_id*				id_material;
            hash_id*                id_resource;

			u32*					parents;
			transform*				transforms;
			mat4*					local_matrices;
			mat4*					world_matrices;
			mat4*					offset_matrices;
			mat4*					physics_matrices;
            bounding_volume*        bounding_volumes;
            scene_node_light*       lights;
            
            u32*                    physics_handles;
            u32*                    multibody_handles;
            s32*                    multibody_link;
            
            scene_node_geometry*    geometries;
            scene_node_material*    materials;
            scene_node_physics*     physics_data;
            
            animation_controller*   anim_controller;
            
            u32*                    cbuffer;

			u32						forward_light_buffer = PEN_INVALID_HANDLE;
			u32						flags;

#ifdef CES_DEBUG
            Str*                    names;
			Str*					geometry_names;
			Str*					material_names;

            s32                     selected_index = -1;
            u32                     view_flags;
#endif
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
        
        enum e_scene_render_flags
        {
            RENDER_FORWARD_LIT = 1
        };

		struct scene_view
		{
			u32 cb_view;
            u32 cb_2d_view;
			u32 render_flags = 0;
            
            u32 depth_stencil_state = 0;
            u32 blend_state_state = 0;
            u32 raster_state = 0;
            
            camera* camera;
            pen::viewport* viewport;
            
			pmfx::pmfx_handle pmfx_shader;
            hash_id technique;

			entity_scene* scene = nullptr;
		};

		entity_scene*   create_scene( const c8* name );
        
		void            render_scene_view( const scene_view& view );
		void            update_scene( entity_scene* scene, f32 dt );

		void			resize_scene_buffers( entity_scene* scene, s32 size = 1024 );
		void			zero_entity_components( entity_scene* scene, u32 node_index );

        void            update_view_flags( entity_scene* scene, bool error );
	}

    struct camera_controller
    {
        hash_id id_name;
        put::camera* camera;
        
        void(*update_function)(put::camera_controller*) = nullptr;
        
        Str name;
    };
    
    struct scene_controller
    {
        hash_id id_name;
        put::ces::entity_scene* scene;
		put::camera* camera;
        
        void(*update_function)(put::scene_controller*) = nullptr;
        
        Str name;
    };
    
    struct scene_view_renderer
    {
        hash_id id_name;
        
        void(*render_function)(const put::ces::scene_view&)  = nullptr;
        
        Str name;
    };
}

#endif

//todo port functions from old code base
/*
 struct slider
 {
 vec3f limit_min;
 vec3f limit_max;
 f32      direction;
 u32   entity_index;
 };
 
 struct button
 {
 vec3f limit_up;
 vec3f limit_down;
 vec3f offset;
 f32      spring;
 u32      entity_index;
 u32      lock_state;
 u32      debounce = 0;
 };
 
 scene_nodes*            get_scene_nodes();
 u32                        get_num_nodes();
 void                    get_rb_params_from_node(u32 node_index, f32 mass, physics::rigid_body_params &rbp);
 
 void                    create_compound_from_nodes(physics::compound_rb_params &crbp, const c8* compound_names, u32 clone = 0, c8* suffix = "", vec3f offset = vec3f::zero());
 void                    create_compound_rb(const c8* compound_name, const c8* base_name, f32 mass, u32 group, u32 mask, u32 &rb_index);
 
 void                    create_physics_object(u32 node_index, physics::constraint_params* p_constraint, f32 mass, u32 group, u32 mask, vec3f scale_dimensions = vec3f(1.0f, 1.0f, 1.0f));
 
 u32                        create_slider_widget(u32 node_index, u32 start_index, u32 end_index, f32 mass, f32 invert_max, vec3f &slider_start, vec3f &slider_end, u32 &rb_handle);
 f32                        get_slider_ratio(slider slider_node);
 
 u32                        create_button_widget(u32 node_index, u32 up_index, u32 down_index, f32 mass, f32 spring, button &button_data);
 u32                        check_button_state(button &button_data, u32 button_lock, f32 down_epsilon, f32 up_epsilon);
 void                    release_button(button &button_data);
 
 u32                        create_rotary_widget(u32 node_index, f32 mass, vec3f axis, vec3f pivot, u32 &rb_handle, vec2f limits = vec2f(-FLT_MAX, FLT_MAX));
 
 a_u64*                    get_sn_entityflags(u32 index);
 
 scene_node_physics*        get_sn_physics(u32 index);
 scene_node_geometry*    get_sn_geometry(u32 index);
 scene_node_material*    get_sn_material(u32 index);
 
 s32*                    get_sn_multibodylink(u32 index);
 
 u32*                    get_sn_physicshandle(u32 index);
 u32*                    get_sn_parents(u32 index);
 u32*                    get_sn_multibodyhandle(u32 index);
 
 mat4*                    get_sn_offsetmat(u32 index);
 mat4*                    get_sn_worldmat(u32 index);
 mat4*                    get_sn_localmat(u32 index);
 
 c8*                        get_sn_name(u32 index);
 
 s32                        get_scene_node_by_name(const c8* name);
 s32                        get_cloned_scene_node_by_name(const c8* name, const c8* suffix, vec3f offset, u32 disble_src);
 
 void                    clone_node(u32 src, u32 dst, s32 parent, vec3f offset = vec3f::zero(), const c8* suffix = "_cloned");
 
 void                    hide_node(u32 node_index, u32 remove_physics);
 void                    show_node(u32 node_index, u32 add_physics);
 */
