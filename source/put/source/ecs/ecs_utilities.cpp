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
        
        void write_parsable_string_u32(const Str& str, std::ofstream& ofs)
        {
            // writes chars as u32? because some stupid code in the python export
            if (str.c_str())
            {
                u32 l = str.length();
                ofs.write((const c8*)&l, sizeof(u32));
                for(u32 c = 0; c < l; ++c)
                {
                    ofs.write(&str.c_str()[c], 1);
                    ofs.write(&str.c_str()[c], 1);
                    ofs.write(&str.c_str()[c], 1);
                    ofs.write(&str.c_str()[c], 1);
                }
            }
            else
            {
                u32 zero = 0;
                ofs.write((const c8*)&zero, sizeof(u32));
            }
        }

        void get_new_entities_append(ecs_scene* scene, s32 num, s32& start, s32& end)
        {
            // o(1) - appends a bunch of nodes on the end
            u32 max_num = scene->num_entities + num;
            if (max_num >= scene->soa_size || !scene->free_list_head)
                resize_scene_buffers(scene, max_num * 2);

            start = scene->num_entities;
            end = start + num;

            // iterate over nodes flagging allocated
            for (s32 i = start; i < end; ++i)
                scene->entities[i] |= e_cmp::allocated;

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

            scene->num_entities = end;
        }

        void get_new_entities_contiguous(ecs_scene* scene, s32 num, s32& start, s32& end)
        {
            // o(n) - has to find contiguous nodes within the free list, and worst case will allocate more mem and append the
            // new nodes
            u32 max_num = scene->num_entities + num;
            if (max_num >= scene->soa_size || !scene->free_list_head)
                resize_scene_buffers(scene, max_num * 2);

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
                    scene->entities[fnl_iter->node] |= e_cmp::allocated;
                    scene->free_list_head = fnl_iter;
                    fnl_iter = fnl_iter->next;
                }

                scene->num_entities = std::max<u32>(end, scene->num_entities);
            }
        }

        u32 get_next_entity(ecs_scene* scene)
        {
            if (!scene->free_list_head)
                resize_scene_buffers(scene);

            return scene->free_list_head->node;
        }

        u32 get_new_entity(ecs_scene* scene)
        {
            // o(1) using free list

            if (!scene->free_list_head)
                resize_scene_buffers(scene, scene->soa_size * 2);

            u32 ii = 0;
            ii = scene->free_list_head->node;
            scene->free_list_head = scene->free_list_head->next;

            u32 i = ii;

            scene->flags |= e_scene_flags::invalidate_scene_tree;

            scene->num_entities = std::max<u32>(i + 1, scene->num_entities);

            scene->entities[i] = e_cmp::allocated;

            scene->names[i] = "";
            scene->names[i].appendf("entity_%i", i);

            // default parent is self (no parent)
            scene->parents[i] = i;

            return i;
        }

        void scene_tree_add_entity(scene_tree& tree, scene_tree& node, std::vector<s32>& heirarchy)
        {
            if (heirarchy.empty())
                return;

            if (heirarchy[0] == tree.entity_index)
            {
                heirarchy.erase(heirarchy.begin());

                if (heirarchy.empty())
                {
                    tree.children.push_back(node);
                    return;
                }

                for (auto& child : tree.children)
                {
                    scene_tree_add_entity(child, node, heirarchy);
                }
            }
        }

        void scene_tree_enumerate(ecs_scene* scene, const scene_tree& tree)
        {
            for (auto& child : tree.children)
            {
                bool leaf = child.children.size() == 0;

                bool               selected = scene->state_flags[child.entity_index] & e_state::selected;
                ImGuiTreeNodeFlags node_flags = selected ? ImGuiTreeNodeFlags_Selected : 0;

                if (leaf)
                    node_flags |= ImGuiTreeNodeFlags_Leaf;

                bool      node_open = false;
                const c8* node_name = scene->names[child.entity_index].c_str();
                if (node_name)
                {
                    node_open = ImGui::TreeNodeEx((void*)(intptr_t)child.entity_index, node_flags, "%s", node_name);
                }

                if (ImGui::IsItemClicked())
                {
                    add_selection(scene, child.entity_index);
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
            tree_out.entity_index = -1;

            bool enum_all = start_node == -1;
            if (enum_all)
                start_node = 0;

            for (s32 n = start_node; n < scene->num_entities; ++n)
            {
                if (!(scene->entities[n] & e_cmp::allocated))
                    continue;

                const c8* node_name = scene->names[n].c_str();
                if (!node_name || scene->names[n].empty())
                {
                    scene->names[n] = "node_";
                    scene->names[n].appendf("%i", n);
                }

                scene_tree node;
                node.entity_index = n;

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

                    scene_tree_add_entity(tree_out, node, heirarchy);
                }
            }
        }

        void tree_to_entity_index_list(const scene_tree& tree, s32 start_node, std::vector<s32>& list_out)
        {
            list_out.push_back(tree.entity_index);

            for (auto& child : tree.children)
            {
                tree_to_entity_index_list(child, start_node, list_out);
            }
        }

        void build_heirarchy_node_list(ecs_scene* scene, s32 start_node, std::vector<s32>& list_out)
        {
            scene_tree tree;
            build_scene_tree(scene, start_node, tree);

            tree_to_entity_index_list(tree, start_node, list_out);
        }

        // just set parent and fixup matrix
        void set_entity_parent(ecs_scene* scene, u32 parent, u32 child)
        {
            if (child == parent)
                return;

            scene->parents[child] = parent;

            mat4 parent_mat = scene->world_matrices[parent];

            scene->local_matrices[child] = mat::inverse4x4(parent_mat) * scene->local_matrices[child];
        }

        // set parent and also swap nodes to maintain valid heirarchy
        void set_entity_parent_validate(ecs_scene* scene, u32& parent, u32& child)
        {
            if (child == parent)
                return;

            set_entity_parent(scene, parent, child);

            // children must be below parents
            if (child < parent)
            {
                swap_entities(scene, parent, child);
                u32 temp = parent;
                parent = child;
                child = temp;
            }
        }

        void trim_entities(ecs_scene* scene)
        {
            u32 new_num = scene->num_entities;
            for (u32 n = scene->num_entities - 1; n >= 0; --n)
            {
                if (scene->entities[n] & e_cmp::allocated)
                    break;

                new_num--;
            }

            scene->num_entities = new_num;
        }

        void clone_selection_hierarchical(ecs_scene* scene, u32** selection_list, const c8* suffix)
        {
            std::vector<u32> parent_list;

            u32 sel_num = sb_count(*selection_list);
            for (u32 s = 0; s < sel_num; ++s)
            {
                u32 i = (*selection_list)[s];

                scene->state_flags[i] &= ~e_state::selected;

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
                tree_to_entity_index_list(tree, i, node_index_list);

                s32 nodes_start, nodes_end;
                get_new_entities_contiguous(scene, node_index_list.size() - 1, nodes_start, nodes_end);

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
                        clone_entity(scene, j, nodes_start + node_counter, parent, CLONE_INSTANTIATE, vec3f::zero(), "");
                    node_counter++;

                    if (new_child == parent)
                        sb_push(*selection_list, new_child);

                    dev_console_log("[clone] node [%i]%s to [%i]%s, parent [%i]%s", j, scene->names[j].c_str(), new_child,
                                    scene->names[new_child].c_str(), parent, scene->names[parent].c_str());
                }

                // flush cmd buff
                pen::renderer_consume_cmd_buffer();
            }

            scene->flags |= e_scene_flags::invalidate_scene_tree;
        }

        void instance_entity_range(ecs_scene* scene, u32 master_node, u32 num_nodes)
        {
            s32 master = master_node;

            if (scene->entities[master] & e_cmp::master_instance)
                return;

            scene->entities[master] |= e_cmp::master_instance;

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
            u32 flush = 0;
            for (u32 i = master_node; i < master_node + num_nodes; ++i)
            {
                bake_material_handles(scene, i);

                if (flush > 100)
                {
                    flush = 0;
                    pen::renderer_consume_cmd_buffer();
                }
            }

            // todo - must ensure list is contiguous.
            dev_console_log("[instance] master instance: %i with %i sub instances", master, num_nodes);
        }

        void bake_entities_to_vb(ecs_scene* scene, u32 parent, u32* node_list)
        {
            u32 num_nodes = sb_count(node_list);

            // vb / ib
            pen::buffer_creation_params ibcp;
            ibcp.usage_flags = PEN_USAGE_DEFAULT;
            ibcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
            ibcp.cpu_access_flags = 0;

            pen::buffer_creation_params vbcp;
            vbcp.usage_flags = PEN_USAGE_DEFAULT;
            vbcp.bind_flags = PEN_BIND_INDEX_BUFFER;
            vbcp.cpu_access_flags = 0;
            vbcp.buffer_size = 0;

            u32 vertex_size = 0;
            u32 num_vertices = 0;
            u32 num_indices = 0;

            // calc size
            for (u32 i = 0; i < num_nodes; ++i)
            {
                u32 n = node_list[i];

                geometry_resource* gr = get_geometry_resource_by_index(PEN_HASH(scene->geometry_names[n]), 0);

                if (vertex_size && gr->vertex_size != vertex_size)
                {
                    dev_console_log("[error] can't bake vertex buffer with different vertex types.");
                    return;
                }

                vertex_size = gr->vertex_size;
                num_vertices += gr->num_vertices;
                num_indices += gr->num_indices;

                vbcp.buffer_size += gr->num_vertices * gr->vertex_size;
            }

            // determine output index type
            u32 output_index_size = 2;
            u32 index_type = PEN_FORMAT_R16_UINT;
            if (num_vertices >= 65535)
            {
                index_type = PEN_FORMAT_R32_UINT;
                output_index_size = 4;
            }

            // ib buffer size
            ibcp.buffer_size = output_index_size * num_indices;

            // alloc
            vbcp.data = pen::memory_alloc(vbcp.buffer_size);
            ibcp.data = pen::memory_alloc(ibcp.buffer_size);

            // transform verts and bake into buffer
            u8* vb_data_pos = (u8*)vbcp.data;
            u8* ib_data_pos = (u8*)ibcp.data;
            u32 index_offset = 0;
            for (u32 i = 0; i < num_nodes; ++i)
            {
                u32 n = node_list[i];

                geometry_resource* gr = get_geometry_resource_by_index(PEN_HASH(scene->geometry_names[n]), 0);

                u32 input_index_size = gr->index_type == PEN_FORMAT_R32_UINT ? 4 : 2;
                u32 vb_stride = gr->num_vertices * gr->vertex_size;
                u32 ib_stride = gr->num_indices * output_index_size;

                memcpy(vb_data_pos, gr->cpu_vertex_buffer, vb_stride);

                // offset indices
                for (u32 i = 0; i < gr->num_indices; ++i)
                {
                    // get raw index out of ib
                    u32 raw_i = 0;
                    if (input_index_size == 2)
                    {
                        u16 ii = ((u16*)gr->cpu_index_buffer)[i];
                        raw_i = (u32)ii;
                    }
                    else
                    {
                        u32 ii = ((u32*)gr->cpu_index_buffer)[i];
                        raw_i = (u32)ii;
                    }

                    //offset into new ib
                    raw_i += index_offset;

                    // store in sized format
                    if (output_index_size == 2)
                    {
                        u16* ii = &((u16*)ib_data_pos)[i];
                        *ii = (u16)raw_i;
                    }
                    else
                    {
                        u32* ii = &((u32*)ib_data_pos)[i];
                        *ii = (u32)raw_i;
                    }
                }

                // transform verts
                mat4 wm = scene->world_matrices[n];
                mat4 rm;
                scene->transforms[n].rotation.get_matrix(rm);

                for (u32 v = 0; v < gr->num_vertices; ++v)
                {
                    vertex_model* vm = (vertex_model*)vb_data_pos;

                    vm[v].pos = wm.transform_vector(vm[v].pos);
                    vm[v].normal.xyz = rm.transform_vector(vm[v].normal.xyz);
                    vm[v].tangent.xyz = rm.transform_vector(vm[v].tangent.xyz);
                    vm[v].bitangent.xyz = rm.transform_vector(vm[v].bitangent.xyz);
                }

                vb_data_pos += vb_stride;
                ib_data_pos += ib_stride;

                index_offset += gr->num_indices;

                delete_entity(scene, n);
            }

            // get new node
            u32 nn = parent;

            // instantiate
            scene->entities[nn] |= e_cmp::geometry;
            scene->geometries[nn].vertex_buffer = pen::renderer_create_buffer(vbcp);
            scene->geometries[nn].index_buffer = pen::renderer_create_buffer(ibcp);
            scene->geometries[nn].index_type = index_type;
            scene->geometries[nn].num_vertices = num_vertices;
            scene->geometries[nn].num_indices = num_indices;
            scene->geometries[nn].vertex_size = vertex_size;
            scene->geometries[nn].vertex_shader_class = 0;
            scene->geometries[nn].p_skin = nullptr;

            scene->transforms[nn].scale = vec3f::one();
            scene->transforms[nn].translation = vec3f::zero();
            scene->transforms[nn].rotation = quat();
            scene->entities[nn] |= e_cmp::transform;

            scene->bounding_volumes[nn].min_extents = scene->bounding_volumes[nn].transformed_min_extents;
            scene->bounding_volumes[nn].max_extents = scene->bounding_volumes[nn].transformed_max_extents;

            // todo.. use material.
            material_resource* mr = get_material_resource(PEN_HASH("default_material"));
            instantiate_material(mr, scene, nn);

            instantiate_model_cbuffer(scene, nn);
        }

        u32 bind_animation_to_rig(ecs_scene* scene, anim_handle anim_handle, u32 node_index)
        {
            animation_resource* anim = get_animation_resource(anim_handle);
            anim_instance       anim_instance;
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
                for (u32 e = 0; e < 3; ++e)
                {
                    at.t[e] = t.translation[e];
                    at.t[e + 6] = t.scale[e];
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

            u32 anim_index = sb_count(controller.anim_instances);
            sb_push(controller.anim_instances, anim_instance);

            // todo validate
            scene->entities[node_index] |= e_cmp::anim_controller;
            return anim_index;
        }
    } // namespace ecs
} // namespace put
