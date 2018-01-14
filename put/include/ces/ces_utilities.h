#ifndef ces_utility_h__
#define ces_utility_h__

#include "ces/ces_scene.h"
#include <vector>

namespace put
{
    namespace ces
    {
        struct scene_tree
        {
            s32 node_index;
            const c8* node_name;
            
            std::vector<scene_tree> children;
        };
        
        enum e_clone_mode
        {
            CLONE_INSTANTIATE = 0, //clones node and instantiates new entites (physics, cbuffer, etc),
            CLONE_COPY = 1, //clones node with a shallow copy and will not instantiate new physics or rendering entities
            CLONE_MOVE = 2  //clones the node to the new location keeping physics and rendering entities and zeros the src node
        };
    
        u32     get_new_node( entity_scene* scene );
		void	get_new_nodes_contiguous(entity_scene* scene, s32 num, s32& start, s32& end);
        void	get_new_nodes_append( entity_scene* scene, s32 num, s32& start, s32& end );
        
        u32		clone_node( entity_scene* scene, u32 src, s32 dst = -1, s32 parent = -1, u32 flags = CLONE_INSTANTIATE, vec3f offset = vec3f::zero(), const c8* suffix = "_cloned");
		void	clone_selection_hierarchical(entity_scene* scene, std::vector<u32>& selection_list, const c8* suffix);

		void	set_node_parent(entity_scene* scene, u32 parent, u32 child);

        void    tree_to_node_index_list( const scene_tree& tree, s32 start_node, std::vector<s32>& list_out );
        void    build_scene_tree( entity_scene* scene, s32 start_node, scene_tree& tree_out );
        void    build_heirarchy_node_list( entity_scene* scene, s32 start_node, std::vector<s32>& node_list );
        
        void    scene_tree_enumerate( entity_scene* scene, const scene_tree& tree, std::vector<u32>& selection_list );
        void    scene_tree_add_node( scene_tree& tree, scene_tree& node, std::vector<s32>& heirarchy );

        
        bool    is_valid( u32 handle );
        
        Str     read_parsable_string(const u32** data);
        Str     read_parsable_string( std::ifstream& ifs );
        void    write_parsable_string( const Str& str, std::ofstream& ofs );
        
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

