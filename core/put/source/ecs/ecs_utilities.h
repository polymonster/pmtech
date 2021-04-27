// ecs_utilities.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "ecs/ecs_scene.h"
#include "hash.h"
#include <vector>

namespace put
{
    namespace ecs
    {
        struct scene_tree
        {
            s32                     entity_index;
            std::vector<scene_tree> children;
        };

        namespace e_clone_mode
        {
            enum clone_mode_t
            {
                instantiate,
                copy,
                move
            };
        }
        typedef e_clone_mode::clone_mode_t clone_mode;

        ecs_ref allocate_ref(ecs_scene* scene, u32 entity);
        void    free_ref(ecs_scene* scene, ecs_ref ref);
        ecs_ref get_ref_from_id(ecs_scene* scene, hash_id idname);
        ecs_ref get_child_ref_from_id(ecs_scene* scene, hash_id idname, s32 parent);
        u32     get_index_from_id(ecs_scene* scene, hash_id idname);
        u32     get_index_from_ref(ecs_scene* scene, ecs_ref ref);
        u32     get_ref_from_index(ecs_scene* scene, u32 index);
        
        u32     get_next_entity(ecs_scene* scene); // gets next entity index
        u32     get_new_entity(ecs_scene* scene);  // allocates a new entity at the next index o(1)
        void    get_new_entities_contiguous(ecs_scene* scene, s32 num, s32& start, s32& end); // finds contiguous space o(n)
        void    get_new_entities_append(ecs_scene* scene, s32 num, s32& start, s32& end);     // appends them on the end o(1)
        void    insert_new_entities(ecs_scene* scene, s32 pos, s32 num);
        u32     clone_entity(ecs_scene* scene, u32 src, s32 dst = -1, s32 parent = -1,
                             clone_mode mode = e_clone_mode::instantiate, vec3f offset = vec3f::zero(),
                             const c8* suffix = "_cloned");
        void    swap_entities(ecs_scene* scene, u32 a, s32 b);
        void    clone_selection_hierarchical(ecs_scene* scene, u32** selection_list, const c8* suffix);
        void    instance_entity_range(ecs_scene* scene, u32 master_node, u32 num_nodes);
        void    bake_entities_to_vb(ecs_scene* scene, u32 parent, u32* node_list);
        void    set_entity_parent(ecs_scene* scene, u32 parent, u32 child);
        void    set_entity_parent_validate(ecs_scene* scene, u32& parent, u32& child);
        void    trim_entities(ecs_scene* scene); // trim entites setting num_entities to the last allocated
        u32     bind_animation_to_rig(ecs_scene* scene, anim_handle anim_handle, u32 node_index);
        void    tree_to_entity_index_list(const scene_tree& tree, s32 start_node, std::vector<s32>& list_out);
        void    build_scene_tree(ecs_scene* scene, s32 start_node, scene_tree& tree_out);
        void    build_heirarchy_node_list(ecs_scene* scene, s32 start_node, std::vector<s32>& node_list);
        void    scene_tree_enumerate(ecs_scene* scene, const scene_tree& tree);
        void    scene_tree_add_entity(scene_tree& tree, scene_tree& node, std::vector<s32>& heirarchy);
        Str     read_parsable_string(const u32** data);
        Str     read_parsable_string(std::ifstream& ifs);
        void    write_parsable_string(const Str& str, std::ofstream& ofs);
        void    write_parsable_string_u32(const Str& str, std::ofstream& ofs);
        
        // inlines
        pen_inline ecs_ref allocate_ref(ecs_scene* scene, u32 entity)
        {
            // allocate a ref slot
            /*
            ecs_ref ref_slot = 0;
            if(scene->ref_free_list_head)
            {
                ref_slot = scene->ref_free_list_head->node;
                scene->ref_free_list_head = scene->ref_free_list_head->next;
            }
            */
            
            ecs_ref ref_slot = sb_count(scene->ecs_refs);
            sb_push(scene->ecs_refs, entity);
            return ref_slot;
        }
        
        pen_inline void free_ref(ecs_scene* scene, ecs_ref ref)
        {
            //scene->ecs_ref[ref] = 0;
        }
        
        pen_inline ecs_ref get_ref_from_id(ecs_scene* scene, hash_id idname)
        {
            for(u32 i = 0; i < scene->num_entities; ++i)
                if(idname == scene->id_name[i])
                    return scene->ref_slot[i];
            return -1;
        }
        
        pen_inline ecs_ref get_child_ref_from_id(ecs_scene* scene, hash_id idname, s32 parent)
        {
            for(u32 i = parent; i < scene->num_entities; ++i)
            {
                if(idname == scene->id_name[i])
                    return scene->ref_slot[i];
                    
                u32 p = scene->parents[i];
                if(p == i && i != parent)
                {
                    break;
                }
            }
            return -1;
        }
        
        pen_inline u32 get_index_from_id(ecs_scene* scene, hash_id idname)
        {
            for(u32 i = 0; i < scene->num_entities; ++i)
                if(idname == scene->id_name[i])
                    return i;
            return -1;
        }
        
        pen_inline u32 get_index_from_ref(ecs_scene* scene, ecs_ref ref)
        {
            return scene->ecs_refs[ref];
        }
        
        pen_inline ecs_ref get_ref_from_index(ecs_scene* scene, u32 index)
        {
            return scene->ref_slot[index];
        }
        
    } // namespace ecs
} // namespace put
