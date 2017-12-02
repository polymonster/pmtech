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
	}
}
#endif

