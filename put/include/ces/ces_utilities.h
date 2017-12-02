#ifndef ces_utility_h__
#define ces_utility_h__

#include "ces/ces_scene.h"

namespace put
{
    namespace ces
    {
		void    clone_node( entity_scene* scene, u32 src, u32 dst, s32 parent, vec3f offset = vec3f::zero(), const c8* suffix = "_cloned");
        void    build_joint_list( entity_scene* scene, u32 start_node, std::vector<s32>& joint_list );
        
        bool    is_valid( u32 handle );
    
        //-----------------------------------------------------------------------------------------------------------------------------------
        //inlines
        //-----------------------------------------------------------------------------------------------------------------------------------
        inline bool is_valid( u32 handle )
        {
            return handle != -1;
        }
	}
}
#endif

