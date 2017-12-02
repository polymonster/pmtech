#ifndef component_entity_h__
#define component_entity_h__

#include "definitions.h"
#include "put_math.h"
#include "camera.h"
#include "loader.h"
#include "physics_cmdbuf.h"
#include "str/Str.h"
#include <vector>

#define CES_DEBUG
#define INVALID_HANDLE -1

namespace put
{
    typedef s32 anim_handle;
    typedef s32 mat_handle;
    
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
            CMP_DYNAMIC			= (1 << 8),
            CMP_ANIM_CONTROLLER = (1 << 9),
            CMP_ANIM_TRAJECTORY = (1 << 10)
		};

		enum e_node_types : u32
		{
			NODE_TYPE_NONE = 0,
			NODE_TYPE_JOINT = 1,
			NODE_TYPE_GEOM = 2
		};
        
        enum e_pmm_load_flags : u32
        {
            PMM_GEOMETRY = (1<<0),
            PMM_MATERIAL = (1<<1),
            PMM_NODES = (1<<2),
            PMM_ALL = 7
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
            SN_DIFFUSE,
            SN_NORMAL_MAP,
            SN_SPECULAR_MAP,
            
            SN_NUM_TEXTURES
        };
        
        struct scene_node_material
        {
            s32      texture_id[SN_NUM_TEXTURES] = { 0 };
            vec4f    diffuse_rgb_shininess = vec4f(1.0f, 1.0f, 1.0f, 0.5f);
            vec4f    specular_rgb_reflect = vec4f(1.0f, 1.0f, 1.0f, 0.5f);
        };
        
        struct material_resource
        {
            Str         filename;
            Str         material_name;
            hash_id     hash;
            
            s32         texture_id[SN_NUM_TEXTURES] = { 0 };
            vec4f       diffuse_rgb_shininess = vec4f(1.0f, 1.0f, 1.0f, 0.5f);
            vec4f       specular_rgb_reflect = vec4f(1.0f, 1.0f, 1.0f, 0.5f);
        };
        
        struct scene_node_physics
        {
            vec3f min_extents;
            vec3f max_extents;
            vec3f centre;
            u32   collision_shape;
            u32   collision_dynamic;
            vec3f start_position;
            quat  start_rotation;
            
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
        
        struct geometry_resource
        {
            hash_id                file_hash;
            hash_id                hash;
            
            Str                    filename;
            Str                    geometry_name;
            Str                    material_name;
            u32                    submesh_index;
            
            u32                    position_buffer;
            u32                    vertex_buffer;
            u32                    index_buffer;
            u32                    num_indices;
            u32                    num_vertices;
            u32                    index_type;
            u32                    material_index;
            u32                    vertex_size;
            
            scene_node_physics     physics_info;
            scene_node_skin*       p_skin;
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
        
        enum e_animation_semantics
        {
            A_TIME = 0,
            A_TRANSFORM = 1,
        };
        
        enum e_animation_data_types
        {
            A_FLOAT = 0,
            A_FLOAT4x4 = 1,
            A_TRANSLATION = 2,
            A_ROTATION = 3
        };
    
        struct node_animation_channel
        {
            u32     num_frames;
            hash_id target;
            
            f32*    times;
            mat4*   matrices;
        };
        
        struct animation
        {
            hash_id id_name;
            u32 node;
            
            u32 num_channels;
            node_animation_channel* channels;
            
            f32 length;
            f32 step;
            
#ifdef CES_DEBUG
            Str name;
#endif
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
			mat4*					local_matrices;
			mat4*					world_matrices;
			mat4*					offset_matrices;
			mat4*					physics_matrices;
            bounding_volume*        bounding_volumes;
            
            u32*                    physics_handles;
            u32*                    multibody_handles;
            s32*                    multibody_link;
            
            scene_node_geometry*    geometries;
            scene_node_material*    materials;
            scene_node_physics*     physics_data;
            
            animation_controller*   anim_controller;
            
            u32*                    cbuffer;
            
#ifdef CES_DEBUG
            Str*                    names;
			Str*					geometry_names;
			Str*					material_names;

            s32                     selected_index = -1;
            u32                     debug_flags;
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

		struct scene_view
		{
			u32 cb_view;
			u32 scene_node_flags = 0;
			u32 debug_flags = 0;
            
            hash_id technique;
			entity_scene* scene = nullptr;
		};

		entity_scene*   create_scene( const c8* name );
        
        void            save_scene( const c8* filename, entity_scene* scene );
        void            load_scene( const c8* filename, entity_scene* scene );

        void            load_pmm( const c8* model_scene_name, entity_scene* scene = nullptr, u32 load_flags = PMM_ALL );
        anim_handle     load_pma( const c8* model_scene_name );

		void            clone_node( entity_scene* scene, u32 src, u32 dst, s32 parent, vec3f offset = vec3f::zero(), const c8* suffix = "_cloned");
        void            build_joint_list( entity_scene* scene, u32 start_node, std::vector<s32>& joint_list );
        
		void            scene_browser_ui( entity_scene* scene, bool* open );
        void            enumerate_resources( bool* open );
        
        bool            is_valid( u32 handle );
        
        //inlines
        inline bool is_valid( u32 handle )
        {
            return handle != -1;
        }
	}
}

#endif
