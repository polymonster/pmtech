// ecs_utilities.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "ecs/ecs_scene.h"
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
    namespace ecs
    {
        struct scene_tree
        {
            s32                     entity_index;
            std::vector<scene_tree> children;
        };

        enum e_clone_mode
        {
            CLONE_INSTANTIATE = 0, // clones entites and instantiates new entites (physics, cbuffer, etc),
            CLONE_COPY = 1, // clones entites with a shallow copy and will not instantiate new physics or rendering entities
            CLONE_MOVE = 2  // clones entites to the new location keeping physics and rendering entities and zeros the src
        };

        u32  get_next_entity(ecs_scene* scene); // gets next entity index
        u32  get_new_entity(ecs_scene* scene);  // allocates a new entity at the next index o(1)
        void get_new_entities_contiguous(ecs_scene* scene, s32 num, s32& start, s32& end); // finds contiguous space o(n)
        void get_new_entities_append(ecs_scene* scene, s32 num, s32& start, s32& end);     // appends them on the end o(1)
        u32  clone_entity(ecs_scene* scene, u32 src, s32 dst = -1, s32 parent = -1, u32 flags = CLONE_INSTANTIATE,
                          vec3f offset = vec3f::zero(), const c8* suffix = "_cloned");
        void swap_entities(ecs_scene* scene, u32 a, s32 b);
        void clone_selection_hierarchical(ecs_scene* scene, u32** selection_list, const c8* suffix);
        void instance_entity_range(ecs_scene* scene, u32 master_node, u32 num_nodes);
        void bake_entities_to_vb(ecs_scene* scene, u32 parent, u32* node_list);
        void set_entity_parent(ecs_scene* scene, u32 parent, u32 child);
        void set_entity_parent_validate(ecs_scene* scene, u32& parent, u32& child);
        void trim_entities(ecs_scene* scene); // trim entites setting num_entities to the last allocated
        u32  bind_animation_to_rig(ecs_scene* scene, anim_handle anim_handle, u32 node_index);
        void tree_to_entity_index_list(const scene_tree& tree, s32 start_node, std::vector<s32>& list_out);
        void build_scene_tree(ecs_scene* scene, s32 start_node, scene_tree& tree_out);
        void build_heirarchy_node_list(ecs_scene* scene, s32 start_node, std::vector<s32>& node_list);
        void scene_tree_enumerate(ecs_scene* scene, const scene_tree& tree);
        void scene_tree_add_entity(scene_tree& tree, scene_tree& node, std::vector<s32>& heirarchy);
        Str  read_parsable_string(const u32** data);
        Str  read_parsable_string(std::ifstream& ifs);
        void write_parsable_string(const Str& str, std::ofstream& ofs);
    } // namespace ecs
} // namespace put
