#ifndef ces_scene_h__
#define ces_scene_h__

#include "pen.h"
#include "put_math.h"
#include "camera.h"
#include "loader.h"
#include "physics.h"
#include "str/Str.h"
#include "pmfx.h"

#include <vector>

#define CES_DEBUG
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
		struct material_resource;

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
			CMP_TRANSFORM		= (1 << 12),
            CMP_CONSTRAINT      = (1 << 13),
            CMP_SUB_INSTANCE    = (1 << 14),
            CMP_MASTER_INSTANCE = (1 << 15),
            CMP_PRE_SKINNED     = (1 << 16),
			CMP_SUB_GEOMETRY	= (1 << 17),
			CMP_SDF_SHADOW		= (1 << 18)
		};

        enum e_state_flags : u32
        {
            SF_SELECTED         = (1<<0),
            SF_CHILD_SELECTED   = (1<<1)
        };

        enum e_light_types : u32
        {
            LIGHT_TYPE_DIR = 0,
            LIGHT_TYPE_POINT = 1,
            LIGHT_TYPE_SPOT = 2
        };
        
        struct per_draw_call
        {
            mat4    world_matrix;
            vec4f   v1;             //generic data ie colour
            vec4f   v2;             //generic data ie roughness, shininess,

			mat4    world_matrix_inv_transpose;
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
			SN_ENV_MAP,
			SN_VOLUME_TEXTURE,
            SN_EMISSIVE_MAP,
            
            SN_NUM_TEXTURES
        };

        struct scene_node_material
        {
            s32      texture_handles[SN_NUM_TEXTURES] = { 0 };
			u32		 sampler_states[SN_NUM_TEXTURES] = { 0 };
            vec4f    diffuse_rgb_shininess = vec4f(1.0f, 1.0f, 1.0f, 0.5f);
            vec4f    specular_rgb_reflect = vec4f(1.0f, 1.0f, 1.0f, 0.5f);

			pmfx::shader_handle pmfx_shader;
			u32					technique;

			material_resource*	resource = nullptr;
        };

        enum e_physics_type
        {
            PHYSICS_TYPE_RIGID_BODY = 0,
            PHYSICS_TYPE_CONSTRAINT
        };
        
        struct scene_node_physics
        {
            s32 type;

            union
            {
                physics::rigid_body_params rigid_body;
                physics::constraint_params constraint;
            };

            scene_node_physics() {};
            ~scene_node_physics() {};

            scene_node_physics& operator = ( const scene_node_physics& other )
            {
                pen::memory_cpy( this, &other, sizeof( scene_node_physics ) );
                return *this;
            }
        };
        
        struct bounding_volume
        {
            vec3f min_extents;
            vec3f max_extents;
            vec3f transformed_min_extents;
            vec3f transformed_max_extents;
            f32   radius;
        };
        
        struct extents
        {
            vec3f min;
            vec3f max;
        };
        
		struct scene_node_geometry
		{
			u32					position_buffer;
			u32					vertex_buffer;
			u32					index_buffer;
			u32					num_indices;
			u32					num_vertices;
			u32					index_type;
            u32                 vertex_size;
			scene_node_skin*	p_skin;
			hash_id				vertex_shader_class;
		};
        
        struct scene_node_pre_skin
        {
            u32                 vertex_buffer;
            u32                 position_buffer;
            u32                 vertex_size;
            u32                 num_verts;
        };
        
        struct master_instance
        {
            u32                 num_instances;
            u32                 instance_buffer;
            u32                 instance_stride;
        };

        struct animation_controller
        {
            enum e_play_flags : u8
            {
                STOPPED = 0,
                PLAY = 1
            };
            
            std::vector<anim_handle> handles;
            s32                      joints_offset;
            anim_handle              current_animation;
            f32                      current_time;
            s32                      current_frame = 0;
            u8                       play_flags = STOPPED;
            bool                     apply_root_motion = true;
        };
    
        struct scene_node_light
        {
            u32     type;
            vec3f   colour;
            
            float   radius;
            float   azimuth;
            float   altitude;
            
            vec3f   direction;
            
            bool    shadow = false;
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

		struct distance_field_shadow
		{
			vec4f	half_size;
			mat4	world_matrix;
			mat4	world_matrix_inverse;
		};

		struct distance_field_shadow_buffer
		{
			distance_field_shadow shadows;
		};

        struct free_node_list
        {
            u32 node;
            free_node_list* next;
            free_node_list* prev;
        };

		struct entity_scene
		{
            u32                     num_nodes = 0;
            u32                     nodes_size = 0;

            a_u64*                  entities;
            a_u64*                  state_flags;

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
            
            master_instance*        master_instances;
            scene_node_geometry*    geometries;
            scene_node_material*    materials;
            scene_node_pre_skin*    pre_skin;
            
            scene_node_physics*     physics_data;
            
            animation_controller*   anim_controller;
            
            u32*                    cbuffer;
            per_draw_call*          draw_call_data;
            
            free_node_list*         free_list;
            free_node_list*         free_list_head;
            
			u32						forward_light_buffer = PEN_INVALID_HANDLE;
			u32						sdf_shadow_buffer = PEN_INVALID_HANDLE;
			u32						flags;
            
            extents                 renderable_extents;

#ifdef CES_DEBUG
            Str*                    names;
			Str*					geometry_names;
			Str*					material_names;

            s32                     selected_index = -1;
            u32                     view_flags;
#endif
		};

		struct entity_scene_instance
		{
			u32 id_name;
			const c8* name;
			entity_scene* scene;
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
            
			pmfx::shader_handle pmfx_shader;
            hash_id technique;

			entity_scene* scene = nullptr;
		};

		entity_scene*   create_scene( const c8* name );
		void			destroy_scene(entity_scene* scene);

		void            render_scene_view( const scene_view& view );
		void            update_scene( entity_scene* scene, f32 dt );

		void			clear_scene(entity_scene* scene);
		void			default_scene(entity_scene* scene);

		void			resize_scene_buffers( entity_scene* scene, s32 size = 1024 );
		void			zero_entity_components( entity_scene* scene, u32 node_index );

        void            delete_entity( entity_scene* scene, u32 node_index );
        void            delete_entity_first_pass( entity_scene* scene, u32 node_index );
        void            delete_entity_second_pass( entity_scene* scene, u32 node_index );

        void            update_view_flags( entity_scene* scene, bool error );
        
        void            initialise_free_list( entity_scene* scene );
	}

    struct camera_controller
    {
        hash_id				id_name;
        camera*				camera = nullptr;
		ces::entity_scene*	scene = nullptr;
        
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
