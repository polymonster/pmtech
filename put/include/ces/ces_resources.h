#ifndef ces_resources_h__
#define ces_resources_h__

#include "ces/ces_scene.h"

namespace put
{
    namespace ces
    {
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
        
        struct animation_resource
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
        
        void            save_scene( const c8* filename, entity_scene* scene );
        void            load_scene( const c8* filename, entity_scene* scene );
        
        void            load_pmm( const c8* model_scene_name, entity_scene* scene = nullptr, u32 load_flags = PMM_ALL );
        anim_handle     load_pma( const c8* model_scene_name );
        
        void            instantiate_geometry( geometry_resource* gr, entity_scene* scene, s32 node_index );
        void            instantiate_model_cbuffer( entity_scene* scene, s32 node_index );
        void            instantiate_anim_controller( entity_scene* scene, s32 node_index );
        void            instantiate_material( material_resource* mr, scene_node_material* instance );
        
        material_resource*  get_material_resource( hash_id hash );
        animation_resource* get_animation_resource( anim_handle h );
        geometry_resource*  get_geometry_resource( hash_id h );
	}
}
#endif
