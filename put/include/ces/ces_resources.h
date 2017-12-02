#ifndef ces_resources_h__
#define ces_resources_h__

#include "ces/ces_scene.h"

namespace put
{
    namespace ces
    {
        void            save_scene( const c8* filename, entity_scene* scene );
        void            load_scene( const c8* filename, entity_scene* scene );
        
        void            load_pmm( const c8* model_scene_name, entity_scene* scene = nullptr, u32 load_flags = PMM_ALL );
        anim_handle     load_pma( const c8* model_scene_name );
        
        void            instantiate_geometry( geometry_resource* gr, entity_scene* scene, s32 node_index );
        void            instantiate_anim_controller( entity_scene* scene, s32 node_index );
        void            instantiate_material( material_resource* mr, scene_node_material* instance );
        
        material_resource*  get_material_resource( hash_id hash );
        animation_resource* get_animation_resource( anim_handle h );
        geometry_resource*  get_geometry_resource( hash_id h );
	}
}
#endif

