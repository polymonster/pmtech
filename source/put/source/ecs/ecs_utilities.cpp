// ecs_utilities.cpp
// Copyright 2014 - 2019 Alex Dixon. 
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "dev_ui.h"
#include <fstream>

#include "ecs/ecs_editor.h"
#include "ecs/ecs_resources.h"
#include "ecs/ecs_utilities.h"

#include "data_struct.h"
#include "str_utilities.h"

namespace put
{
    namespace ecs
    {
        Str read_parsable_string(const u32** data)
        {
            Str name;

            const u32* p_len = *data;
            u32        name_len = *p_len++;
            c8*        char_reader = (c8*)p_len;
            for (s32 j = 0; j < name_len; ++j)
            {
                name.append((c8)*char_reader);
                char_reader += 4;
            }

            *data += name_len + 1;

            return name;
        }

        Str read_parsable_string(std::ifstream& ifs)
        {
            Str name;
            u32 len = 0;

            ifs.read((c8*)&len, sizeof(u32));

            for (s32 i = 0; i < len; ++i)
            {
                c8 c;
                ifs.read((c8*)&c, 1);
                name.append(c);
            }

            return name;
        }

        void write_parsable_string(const Str& str, std::ofstream& ofs)
        {
            if (str.c_str())
            {
                u32 len = str.length();
                ofs.write((const c8*)&len, sizeof(u32));
                ofs.write((const c8*)str.c_str(), len);
            }
            else
            {
                u32 zero = 0;
                ofs.write((const c8*)&zero, sizeof(u32));
            }
        }

        void get_new_nodes_append(ecs_scene* scene, s32 num, s32& start, s32& end)
        {
            // o(1) - appends a bunch of nodes on the end

            if (scene->num_nodes + num >= scene->nodes_size || !scene->free_list_head)
                resize_scene_buffers(scene);

            start = scene->num_nodes;
            end = start + num;

            // iterate over nodes flagging allocated
            for (s32 i = start; i < end; ++i)
                scene->entities[i] |= CMP_ALLOCATED;

            scene->free_list_head = scene->free_list[end].next;

            if (scene->free_list[start].prev)
            {
                // remove chunk from the free list
                scene->free_list[start].prev = scene->free_list[end].next;
            }
            else
            {
                // if we are at the head
                scene->free_list_head = scene->free_list[end].next;
            }

            scene->num_nodes = end;
        }

        void get_new_nodes_contiguous(ecs_scene* scene, s32 num, s32& start, s32& end)
        {
            // o(n) - has to find contiguous nodes within the free list, and worst case will allocate more mem and append the
            // new nodes

            if (scene->num_nodes + num >= scene->nodes_size || !scene->free_list_head)
                resize_scene_buffers(scene);

            free_node_list* fnl_iter = scene->free_list_head;
            free_node_list* fnl_start = fnl_iter;

            s32 count = num;

            // find contiguous nodes
            for (;;)
            {
                if (!fnl_iter->next)
                    break;

                s32 diff = fnl_iter->next->node - fnl_iter->node;

                fnl_iter = fnl_iter->next;

                if (diff == 1)
                {
                    count--;

                    if (count == 0)
                        break;
                }
                else
                {
                    fnl_start = fnl_iter;
                    count = num;
                }
            }

            if (count == 0)
            {
                start = fnl_start->node;
                end = start + num;

                // iterate over nodes allocating
                fnl_iter = fnl_start;
                for (s32 i = 0; i < num + 1; ++i)
                {
                    scene->entities[fnl_iter->node] |= CMP_ALLOCATED;
                    scene->free_list_head = fnl_iter;
                    fnl_iter = fnl_iter->next;
                }

                scene->num_nodes = std::max<u32>(end, scene->num_nodes);
            }
        }

        u32 get_next_node(ecs_scene* scene)
        {
            if (!scene->free_list_head)
                resize_scene_buffers(scene);

            return scene->free_list_head->node;
        }

        u32 get_new_node(ecs_scene* scene)
        {
            // o(1) using free list

            if (!scene->free_list_head)
                resize_scene_buffers(scene);

            u32 ii = 0;
            ii = scene->free_list_head->node;
            scene->free_list_head = scene->free_list_head->next;

            u32 i = ii;

            scene->flags |= INVALIDATE_SCENE_TREE;

            scene->num_nodes = std::max<u32>(i + 1, scene->num_nodes);

            scene->entities[i] = CMP_ALLOCATED;

            // default parent is self (no parent)
            scene->parents[i] = i;

            return i;
        }

        void scene_tree_add_node(scene_tree& tree, scene_tree& node, std::vector<s32>& heirarchy)
        {
            if (heirarchy.empty())
                return;

            if (heirarchy[0] == tree.node_index)
            {
                heirarchy.erase(heirarchy.begin());

                if (heirarchy.empty())
                {
                    tree.children.push_back(node);
                    return;
                }

                for (auto& child : tree.children)
                {
                    scene_tree_add_node(child, node, heirarchy);
                }
            }
        }

        void scene_tree_enumerate(ecs_scene* scene, const scene_tree& tree)
        {
            for (auto& child : tree.children)
            {
                bool leaf = child.children.size() == 0;

                bool               selected = scene->state_flags[child.node_index] & SF_SELECTED;
                ImGuiTreeNodeFlags node_flags = selected ? ImGuiTreeNodeFlags_Selected : 0;

                if (leaf)
                    node_flags |= ImGuiTreeNodeFlags_Leaf;

                bool      node_open = false;
                const c8* node_name = scene->names[child.node_index].c_str();
                if (node_name)
                {
                    node_open = ImGui::TreeNodeEx((void*)(intptr_t)child.node_index, node_flags, "%s", node_name);
                }

                if (ImGui::IsItemClicked())
                {
                    add_selection(scene, child.node_index);
                }

                if (node_open)
                {
                    scene_tree_enumerate(scene, child);
                    ImGui::TreePop();
                }
            }
        }

        void build_scene_tree(ecs_scene* scene, s32 start_node, scene_tree& tree_out)
        {
            // tree view
            tree_out.node_index = -1;

            bool enum_all = start_node == -1;
            if (enum_all)
                start_node = 0;

            for (s32 n = start_node; n < scene->num_nodes; ++n)
            {
                if (!(scene->entities[n] & CMP_ALLOCATED))
                    continue;

                const c8* node_name = scene->names[n].c_str();
                if (!node_name || scene->names[n].empty())
                {
                    scene->names[n] = "node_";
                    scene->names[n].appendf("%i", n);
                }

                scene_tree node;
                node.node_index = n;

                if (scene->parents[n] == n || n == start_node)
                {
                    if (!enum_all && n != start_node)
                        break;

                    tree_out.children.push_back(node);
                }
                else
                {
                    std::vector<s32> heirarchy;

                    u32 p = n;
                    while (scene->parents[p] != p)
                    {
                        p = scene->parents[p];
                        heirarchy.insert(heirarchy.begin(), p);
                    }

                    heirarchy.insert(heirarchy.begin(), -1);

                    scene_tree_add_node(tree_out, node, heirarchy);
                }
            }
        }

        void tree_to_node_index_list(const scene_tree& tree, s32 start_node, std::vector<s32>& list_out)
        {
            list_out.push_back(tree.node_index);

            for (auto& child : tree.children)
            {
                tree_to_node_index_list(child, start_node, list_out);
            }
        }

        void build_heirarchy_node_list(ecs_scene* scene, s32 start_node, std::vector<s32>& list_out)
        {
            scene_tree tree;
            build_scene_tree(scene, start_node, tree);

            tree_to_node_index_list(tree, start_node, list_out);
        }

        void set_node_parent(ecs_scene* scene, u32 parent, u32 child)
        {
            if (child == parent)
                return;

            scene->parents[child] = parent;

            mat4 parent_mat = scene->world_matrices[parent];

            scene->local_matrices[child] = mat::inverse4x4(parent_mat) * scene->local_matrices[child];
        }

        void clone_selection_hierarchical(ecs_scene* scene, u32** selection_list, const c8* suffix)
        {
            std::vector<u32> parent_list;

            u32 sel_num = sb_count(*selection_list);
            for (u32 s = 0; s < sel_num; ++s)
            {
                u32 i = (*selection_list)[s];

                scene->state_flags[i] &= ~SF_SELECTED;

                if (scene->parents[i] == i || i == 0)
                    parent_list.push_back(i);
            }

            sb_free(*selection_list);
            *selection_list = nullptr;

            for (u32 i : parent_list)
            {
                scene_tree tree;
                build_scene_tree(scene, i, tree);

                std::vector<s32> node_index_list;
                tree_to_node_index_list(tree, i, node_index_list);

                s32 nodes_start, nodes_end;
                get_new_nodes_contiguous(scene, node_index_list.size() - 1, nodes_start, nodes_end);

                u32 src_parent = i;
                u32 dst_parent = nodes_start;

                s32 node_counter = 0;
                for (s32 j : node_index_list)
                {
                    if (j < 0)
                        continue;

                    u32 j_parent = scene->parents[j];
                    u32 parent_offset = j_parent - src_parent;
                    u32 parent = dst_parent + parent_offset;

                    u32 new_child =
                        clone_node(scene, j, nodes_start + node_counter, parent, CLONE_INSTANTIATE, vec3f::zero(), "");
                    node_counter++;

                    if (new_child == parent)
                        sb_push(*selection_list, new_child);

                    dev_console_log("[clone] node [%i]%s to [%i]%s, parent [%i]%s", j, scene->names[j].c_str(), new_child,
                                    scene->names[new_child].c_str(), parent, scene->names[parent].c_str());
                }

                // flush cmd buff
                pen::renderer_consume_cmd_buffer();
            }

            scene->flags |= INVALIDATE_SCENE_TREE;
        }

        void instance_node_range(ecs_scene* scene, u32 master_node, u32 num_nodes)
        {
            s32 master = master_node;

            if (scene->entities[master] & CMP_MASTER_INSTANCE)
                return;

            scene->entities[master] |= CMP_MASTER_INSTANCE;

            scene->master_instances[master].num_instances = num_nodes;
            scene->master_instances[master].instance_stride = sizeof(cmp_draw_call);

            pen::buffer_creation_params bcp;
            bcp.usage_flags = PEN_USAGE_DYNAMIC;
            bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            bcp.buffer_size = sizeof(cmp_draw_call) * scene->master_instances[master].num_instances;
            bcp.data = nullptr;
            bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;

            scene->master_instances[master].instance_buffer = pen::renderer_create_buffer(bcp);

            scene->geometries[master].vertex_shader_class = ID_VERTEX_CLASS_INSTANCED;

            // vertex class has changed which changes shader technique
            for (u32 i = master_node; i < master_node + num_nodes; ++i)
                bake_material_handles(scene, i);

            // todo - must ensure list is contiguous.
            dev_console_log("[instance] master instance: %i with %i sub instances", master, num_nodes);
        }
        
        bool bind_animation_to_rig(ecs_scene* scene, anim_handle anim_handle, u32 node_index)
        {
            animation_resource* anim = get_animation_resource(anim_handle);
            anim_instance anim_instance;
            anim_instance.soa = anim->soa;
            anim_instance.length = anim->length;
            
            cmp_anim_controller_v2& controller = scene->anim_controller_v2[node_index];
            
            // initialise anim with starting transform
            u32 num_joints = sb_count(controller.joint_indices);
            for (u32 j = 0; j < num_joints; ++j)
            {
                u32 jnode = controller.joint_indices[j];
                
                // create space for trans quat scale
                const cmp_transform& t = scene->initial_transform[jnode];
                sb_push(anim_instance.joints, t);
                
                // create anim target txyz, rxyz, sxyz
                anim_target at;
                for(u32 e = 0; e < 3; ++e)
                {
                    at.t[e] = t.translation[e];
                    at.t[e+6] = t.scale[e];
                }
                
                vec3f eu = t.rotation.to_euler();
                at.t[3] = eu.x;
                at.t[4] = eu.y;
                at.t[5] = eu.z;
                
                sb_push(anim_instance.targets, at);
            }
            
            // bind channels
            for (u32 c = 0; c < anim->num_channels; ++c)
            {
                anim_sampler sampler;
                sampler.joint = PEN_INVALID_HANDLE;
                sampler.pos = 0;
                
                // find bone for channel
                for (u32 j = 0; j < num_joints; ++j)
                {
                    u32 jnode = controller.joint_indices[j];
                    
                    u32 bone_len = scene->names[jnode].length();
                    u32 target_len = anim->channels[c].target_name.length();
                    
                    if (bone_len > target_len)
                        continue;
                    
                    Str ss = pen::str_substr(anim->channels[c].target_name, target_len - bone_len, target_len);
                    if (ss == scene->names[jnode])
                    {
                        // bind sampler to joint
                        sampler.joint = j;
                        break;
                    }
                }
                
                sb_push(anim_instance.samplers, sampler);
            }
            
            u32 num_samplers = sb_count(anim_instance.samplers);
            PEN_ASSERT(num_samplers == anim->num_channels);
            
            sb_push(controller.anim_instances, anim_instance);
            
            // todo validate
            scene->entities[node_index] |= CMP_ANIM_CONTROLLER;
            return true;
        }
    } // namespace ces
} // namespace put
