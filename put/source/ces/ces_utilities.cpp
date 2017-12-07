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
        
        u32 get_new_node( entity_scene* scene )
        {
            u32 i = 0;
            while( scene->entities[i++] & CMP_ALLOCATED )
                continue;
            
            if( i > scene->num_nodes )
                scene->num_nodes = i;
            
            return i-1;
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
        
        void build_scene_tree( entity_scene* scene, u32 start_node, scene_tree& tree_out )
        {
            //tree view
            tree_out.node_index = -1;
            
            //todo this could be cached
            for( s32 n = start_node; n < scene->num_nodes; ++n )
            {
                scene_tree node;
                node.node_name = scene->names[n].c_str();
                node.node_index = n;
                
                if( scene->parents[n] == n )
                {
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
        
        void tree_to_node_index_list( const scene_tree& tree, std::vector<s32>& list_out )
        {
            list_out.push_back(tree.node_index);
            
            for( auto& child : tree.children )
            {
                tree_to_node_index_list( child, list_out );
            }
        }
        
        void build_joint_list( entity_scene* scene, u32 start_node, std::vector<s32>& list_out )
        {
            scene_tree tree;
            build_scene_tree( scene, start_node, tree );
            
            tree_to_node_index_list( tree, list_out );
        }
    }
}
