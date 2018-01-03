#include <fstream>
#include "dev_ui.h"

#include "ces/ces_utilities.h"

namespace put
{
    namespace ces
    {
        Str read_parsable_string(const u32** data)
        {
            Str name;
            
            const u32* p_len = *data;
            u32 name_len = *p_len++;
            c8* char_reader = (c8*)p_len;
            for(s32 j = 0; j < name_len; ++j)
            {
                name.append((c8)*char_reader);
                char_reader+=4;
            }
            
            *data += name_len + 1;
            
            return name;
        }
        
        Str read_parsable_string( std::ifstream& ifs )
        {
            Str name;
            u32 len = 0;
            
            ifs.read((c8*)&len, sizeof(u32));
            
            for(s32 i = 0; i < len; ++i)
            {
                c8 c;
                ifs.read((c8*)&c, 1);
                name.append(c);
            }
            
            return name;
        }
        
        void write_parsable_string( const Str& str, std::ofstream& ofs )
        {
            if(str.c_str())
            {
                u32 len = str.length();
                ofs.write( (const c8*)&len, sizeof(u32) );
                ofs.write( (const c8*)str.c_str(), len);
            }
            else
            {
                u32 zero = 0;
                ofs.write( (const c8*)&zero, sizeof(u32) );
            }
        }

		void get_new_nodes_contiguous(entity_scene* scene, s32 num, s32& start, s32& end)
		{
            //o(n) - has to find contiguous nodes within the free list
            
            if (scene->num_nodes + num >= scene->nodes_size || !scene->free_list_head)
                resize_scene_buffers(scene);
            
            free_node_list* fnl_iter = scene->free_list_head;
            free_node_list* fnl_start = fnl_iter;
            
            s32 count = num;
            
            //todo improve the free list
            
            //finds contiguous nodes
            for(;;)
            {
                if(!fnl_iter->next)
                    break;
                
                s32 diff = fnl_iter->next->node - fnl_iter->node;
                
                fnl_iter = fnl_iter->next;
                
                if(diff == 1)
                {
                    count--;
                    
                    if(count == 0)
                        break;
                }
                else
                {
                    fnl_start = fnl_iter;
                    count = num;
                }
            }
            
            if(count == 0)
            {
                start = fnl_start->node;
                end = start + num;
                
                //iterate over nodes allocating
                fnl_iter = fnl_start;
                for( s32 i = 0; i < num+1; ++i )
                {
                    scene->entities[fnl_iter->node] |= CMP_ALLOCATED;
                    fnl_iter = fnl_iter->next;
                }
                
                scene->free_list_head = fnl_iter;
                
                if (end > scene->num_nodes)
                    scene->num_nodes = end;
            }
		}
        
        u32 get_new_node( entity_scene* scene )
        {
            //o(1) - uses free list
            
            if(!scene->free_list_head)
                resize_scene_buffers(scene);
            
            u32 ii = 0;
            ii = scene->free_list_head->node;
            scene->free_list_head = scene->free_list_head->next;
            
            u32 i = ii;
            
			scene->flags |= INVALIDATE_SCENE_TREE;

            if( i > scene->num_nodes )
                scene->num_nodes = i+1;
            
			scene->entities[i] = CMP_ALLOCATED;

            return i;
        }
        
        void scene_tree_add_node( scene_tree& tree, scene_tree& node, std::vector<s32>& heirarchy )
        {
            if( heirarchy.empty() )
                return;
            
            if( heirarchy[0] == tree.node_index )
            {
                heirarchy.erase(heirarchy.begin());
                
                if( heirarchy.empty() )
                {
                    tree.children.push_back(node);
                    return;
                }
                
                for( auto& child : tree.children )
                {
                    scene_tree_add_node(child, node, heirarchy );
                }
            }
        }
        
        void scene_tree_enumerate( const scene_tree& tree, s32& selected )
        {
            for( auto& child : tree.children )
            {
                if( child.children.empty() )
                {
					if (!child.node_name)
						continue;

                    ImGui::Selectable(child.node_name);
                    if (ImGui::IsItemClicked())
                        selected = child.node_index;
                }
                else
                {
                    if(tree.node_index == -1)
                        ImGui::Unindent(ImGui::GetTreeNodeToLabelSpacing());
                    
                    ImGuiTreeNodeFlags node_flags = selected == child.node_index ? ImGuiTreeNodeFlags_Selected : 0;
                    bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)child.node_index, node_flags, child.node_name, child.node_index);
                    if (ImGui::IsItemClicked())
                        selected = child.node_index;
                    
                    if( node_open )
                    {
                        scene_tree_enumerate(child, selected);
                        ImGui::TreePop();
                    }
                    
                    if(tree.node_index == -1)
                        ImGui::Indent(ImGui::GetTreeNodeToLabelSpacing());
                }
            }
        }
        
        void build_scene_tree( entity_scene* scene, s32 start_node, scene_tree& tree_out )
        {
            //tree view
            tree_out.node_index = -1;

			bool enum_all = start_node == -1;
			if(enum_all)
				start_node = 0;

            for( s32 n = start_node; n < scene->num_nodes; ++n )
            {
				if (!(scene->entities[n] & CMP_ALLOCATED))
					continue;

                scene_tree node;
                node.node_name = scene->names[n].c_str();
                node.node_index = n;

                if( scene->parents[n] == n )
                {
					if (!enum_all && n != start_node)
						break;

                    tree_out.children.push_back(node);
                }
                else
                {
                    std::vector<s32> heirarchy;
                    
                    u32 p = n;
                    while( scene->parents[p] != p )
                    {
                        p = scene->parents[p];
                        heirarchy.insert(heirarchy.begin(), p);
                    }
                    
                    heirarchy.insert(heirarchy.begin(),-1);
                    
                    scene_tree_add_node( tree_out, node, heirarchy );
                }
            }
        }
        
        void tree_to_node_index_list( const scene_tree& tree, s32 start_node, std::vector<s32>& list_out )
        {
            list_out.push_back(tree.node_index);
            
            for( auto& child : tree.children )
            {
				tree_to_node_index_list( child, start_node, list_out );
            }
        }
        
        void build_heirarchy_node_list( entity_scene* scene, s32 start_node, std::vector<s32>& list_out )
        {
            scene_tree tree;
            build_scene_tree( scene, start_node, tree );
            
            tree_to_node_index_list( tree, start_node, list_out );
        }

		void set_node_parent( entity_scene* scene, u32 parent, u32 child)
		{
			if (child == parent)
				return;

			scene->parents[child] = parent;

			mat4 parent_mat = scene->world_matrices[parent];

			scene->local_matrices[child] = parent_mat.inverse4x4() * scene->local_matrices[child];
		}

		void clone_selection_hierarchical(entity_scene* scene, std::vector<u32>& selection_list, const c8* suffix )
		{
			std::vector<u32> parent_list;

			for (u32 i : selection_list)
			{
				if( scene->parents[i] == i || i == 0 )
					parent_list.push_back(i);
			}
            
            selection_list.clear( );

			for (u32 i : parent_list)
			{
				scene_tree tree;
				build_scene_tree( scene, i, tree );

				std::vector<s32> node_index_list;
				tree_to_node_index_list(tree, i, node_index_list);

				s32 nodes_start, nodes_end;
				get_new_nodes_contiguous( scene, node_index_list.size()-1, nodes_start, nodes_end);

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

					u32 new_child = clone_node(scene, j, nodes_start + node_counter, parent, vec3f::zero(), "");
					node_counter++;

					if(new_child == parent)
						selection_list.push_back(new_child);
                    
					dev_console_log("[clone] node [%i]%s to [%i]%s, parent [%i]%s", 
						j,
						scene->names[j].c_str(),
						new_child,
						scene->names[new_child].c_str(), 
						parent,
						scene->names[parent].c_str());
				}

				//flush cmd buff
				pen::renderer_consume_cmd_buffer();
			}
		}
    }
}
