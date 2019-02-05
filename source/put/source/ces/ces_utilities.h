// ces_utilities.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#ifndef ces_utility_h__
#define ces_utility_h__

#include "ces/ces_scene.h"
#include "hash.h"
#include <vector>

namespace
{
    hash_id ID_VERTEX_CLASS_INSTANCED = PEN_HASH("_instanced");
    hash_id ID_VERTEX_CLASS_SKINNED = PEN_HASH("_skinned");
    hash_id ID_VERTEX_CLASS_BASIC = PEN_HASH("");

    enum e_shader_permutation
    {
        PERMUTATION_SKINNED = 1 << 31,
        PERMUTATION_INSTANCED = 1 << 30
    };
} // namespace

namespace put
{
    namespace ces
    {
        struct scene_tree
        {
            s32                     node_index;
            std::vector<scene_tree> children;
        };

        enum e_clone_mode
        {
            CLONE_INSTANTIATE = 0, // clones node and instantiates new entites (physics, cbuffer, etc),
            CLONE_COPY = 1, // clones node with a shallow copy and will not instantiate new physics or rendering entities
            CLONE_MOVE = 2  // clones node to the new location keeping physics and rendering entities and zeros the src node
        };

        u32  get_next_node(entity_scene* scene); // gets next node index
        u32  get_new_node(entity_scene* scene);  // allocates a new node at the next index o(1)
        void get_new_nodes_contiguous(entity_scene* scene, s32 num, s32& start,
                                      s32& end); // gets new nodes finding contiguous space in the scene o(n)
        void get_new_nodes_append(entity_scene* scene, s32 num, s32& start,
                                  s32& end); // gets new nodes appending them on the end o(1)
        u32  clone_node(entity_scene* scene, u32 src, s32 dst = -1, s32 parent = -1, u32 flags = CLONE_INSTANTIATE,
                        vec3f offset = vec3f::zero(), const c8* suffix = "_cloned");
        void clone_selection_hierarchical(entity_scene* scene, u32** selection_list, const c8* suffix);
        void instance_node_range(entity_scene* scene, u32 master_node, u32 num_nodes);
        void set_node_parent(entity_scene* scene, u32 parent, u32 child);
        bool bind_animation_to_rig(entity_scene* scene, anim_handle anim_handle, u32 node_index);
        bool bind_animation_to_rig_v2(entity_scene* scene, anim_handle anim_handle, u32 node_index);
        void tree_to_node_index_list(const scene_tree& tree, s32 start_node, std::vector<s32>& list_out);
        void build_scene_tree(entity_scene* scene, s32 start_node, scene_tree& tree_out);
        void build_heirarchy_node_list(entity_scene* scene, s32 start_node, std::vector<s32>& node_list);
        void scene_tree_enumerate(entity_scene* scene, const scene_tree& tree);
        void scene_tree_add_node(scene_tree& tree, scene_tree& node, std::vector<s32>& heirarchy);
        Str  read_parsable_string(const u32** data);
        Str  read_parsable_string(std::ifstream& ifs);
        void write_parsable_string(const Str& str, std::ofstream& ofs);
    } // namespace ces
} // namespace put
#endif
