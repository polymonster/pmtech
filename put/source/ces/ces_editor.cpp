#include "dev_ui.h"
#include "debug_render.h"

#include "ces/ces_resources.h"
#include "ces/ces_utilities.h"
#include "ces/ces_editor.h"

namespace put
{
    namespace ces
    {
        enum e_debug_draw_flags
        {
            DD_MATRIX = 1<<0,
            DD_BONES = 1<<1,
            DD_AABB = 1<<2,
            DD_GRID = 1<<3,
            DD_HIDE = 1<<4,
            DD_NODE = 1<<5,
            
            DD_NUM_FLAGS = 6
        };
        
        const c8* dd_names[]
        {
            "Matrices",
            "Bones",
            "AABB",
            "Grid",
            "Hide Main Render",
            "Selected Node"
        };
        static_assert(sizeof(dd_names)/sizeof(dd_names[0]) == DD_NUM_FLAGS, "mismatched");
        
        static bool* k_dd_bools = nullptr;
        
        void scene_browser_ui( entity_scene* scene, bool* open )
        {
            ImGui::Begin("Scene Browser", open );
            
            if (ImGui::CollapsingHeader("Debug Draw"))
            {
                if(!k_dd_bools)
                {
                    k_dd_bools = new bool[DD_NUM_FLAGS];
                    pen::memory_set(k_dd_bools, 0x0, sizeof(bool)*DD_NUM_FLAGS);
                }
                
                for( s32 i = 0; i < DD_NUM_FLAGS; ++i )
                {
                    ImGui::Checkbox(dd_names[i], &k_dd_bools[i]);
                    
                    u32 mask = 1<<i;
                    
                    if(k_dd_bools[i])
                        scene->debug_flags |= mask;
                    else
                        scene->debug_flags &= ~(mask);
                    
                    if( i != DD_NUM_FLAGS-1 )
                    {
                        ImGui::SameLine();
                    }
                }
            }
            
            static bool list_view = false;
            if( ImGui::Button(ICON_FA_LIST) )
                list_view = true;
            
            ImGui::SameLine();
            if( ImGui::Button(ICON_FA_USB) )
                list_view = false;
            
            ImGui::Columns( 2 );
            
            ImGui::BeginChild("Entities", ImVec2(0, 0), true );
            
            s32& selected_index = scene->selected_index;
            
            if( list_view )
            {
                for (u32 i = 0; i < scene->num_nodes; ++i)
                {
                    bool selected = false;
                    ImGui::Selectable(scene->names[i].c_str(), &selected);
                    
                    if (selected)
                    {
                        selected_index = i;
                    }
                }
            }
            else
            {
                scene_tree tree;
                
                build_scene_tree( scene, 0, tree );
                
                scene_tree_enumerate(tree, selected_index);
            }
            
            ImGui::EndChild();
            
            ImGui::NextColumn();
            
            ImGui::BeginChild("Selected", ImVec2(0, 0), true );
            
            if (selected_index != -1)
            {
                //header
                ImGui::Text("%s", scene->names[selected_index].c_str());
                
                s32 parent_index = scene->parents[selected_index];
                if( parent_index != selected_index)
                    ImGui::Text("Parent: %s", scene->names[parent_index].c_str());
                
                ImGui::Separator();
                
                //geom
                ImGui::Text("Geometry: %s", scene->geometry_names[selected_index].c_str());
                ImGui::Separator();
                
                //material
                ImGui::Text("Material: %s", scene->material_names[selected_index].c_str());
                
                if (scene->material_names[selected_index].c_str())
                {
                    for (u32 t = 0; t < put::ces::SN_NUM_TEXTURES; ++t)
                    {
                        if (scene->materials[selected_index].texture_id[t] > 0)
                        {
                            if (t > 0)
                                ImGui::SameLine();
                            
                            ImGui::Image(&scene->materials[selected_index].texture_id[t], ImVec2(128, 128));
                        }
                    }
                }
                ImGui::Separator();
                
                if( scene->geometries[selected_index].p_skin )
                {
                    static bool open_anim_import = false;
                    
                    if( ImGui::CollapsingHeader("Animations") )
                    {
                        auto& controller = scene->anim_controller[selected_index];
                        
                        ImGui::Checkbox("Apply Root Motion", &scene->anim_controller[selected_index].apply_root_motion);
                        
                        if( ImGui::Button("Add Animation") )
                            open_anim_import = true;
                        
                        if( ImGui::Button("Reset Root Motion") )
                        {
                            scene->local_matrices[selected_index].create_identity();
                        }
                        
                        s32 num_anims = scene->anim_controller[selected_index].handles.size();
                        for (s32 ih = 0; ih < num_anims; ++ih)
                        {
                            s32 h = scene->anim_controller[selected_index].handles[ih];
                            auto* anim = get_animation_resource(h);
                            
                            bool selected = false;
                            ImGui::Selectable( anim->name.c_str(), &selected );
                            
                            if (selected)
                                controller.current_animation = h;
                        }
                        
                        if (is_valid( controller.current_animation ))
                        {
                            if (ImGui::InputInt( "Frame", &controller.current_frame ))
                                controller.play_flags = 0;
                            
                            ImGui::SameLine();
                            
                            if (controller.play_flags == 0)
                            {
                                if (ImGui::Button( ICON_FA_PLAY ))
                                    controller.play_flags = 1;
                            }
                            else
                            {
                                if (ImGui::Button( ICON_FA_STOP ))
                                    controller.play_flags = 0;
                            }
                        }
                        
                        if( open_anim_import )
                        {
                            const c8* anim_import = put::dev_ui::file_browser(open_anim_import, dev_ui::FB_OPEN, 1, "**.pma" );
                            
                            if(anim_import)
                            {
                                anim_handle ah = load_pma(anim_import);
                                auto* anim = get_animation_resource(ah);
                                
                                if( is_valid(ah) )
                                {
                                    //validate that the anim can fit the rig
                                    std::vector<s32> joint_indices;
                                    
                                    build_joint_list( scene, selected_index, joint_indices );
                                    
                                    s32 channel_index = 0;
                                    s32 joints_offset = -1; //scene tree has a -1 node
                                    bool compatible = true;
                                    for( s32 jj = 0; jj < joint_indices.size(); ++jj )
                                    {
                                        s32 jnode = joint_indices[jj];
                                        
                                        if( scene->entities[jnode] & CMP_BONE )
                                        {
                                            if( anim->channels[channel_index].target != scene->id_name[jnode] )
                                            {
                                                compatible = false;
                                                break;
                                            }
                                            
                                            channel_index++;
                                        }
                                        else
                                        {
                                            joints_offset++;
                                        }
                                    }
                                    
                                    if( compatible )
                                    {
                                        scene->anim_controller[selected_index].joints_offset = joints_offset;
                                        scene->entities[selected_index] |= CMP_ANIM_CONTROLLER;
                                        
                                        bool exists = false;
                                        
                                        for( auto& h : controller.handles )
                                            if( h == ah )
                                                exists = true;
                                        
                                        if(!exists)
                                            scene->anim_controller[selected_index].handles.push_back(ah);
                                    }
                                }
                            }
                        }
                        
                        ImGui::Separator();
                    }
                }
            }
            
            ImGui::EndChild();
            
            ImGui::Columns(1);
            
            ImGui::End();
        }
        
        void render_scene_debug( const scene_view& view )
        {
            entity_scene* scene = view.scene;
            
            if( scene->debug_flags & DD_MATRIX )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    put::dbg::add_coord_space(scene->world_matrices[n], 0.5f);
                }
            }
            
            if( scene->debug_flags & DD_AABB )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    put::dbg::add_aabb( scene->bounding_volumes[n].transformed_min_extents, scene->bounding_volumes[n].transformed_max_extents );
                }
            }
            
            if( scene->debug_flags & DD_BONES )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    if( !(scene->entities[n] & CMP_BONE)  )
                        continue;
                    
                    if( scene->entities[n] & CMP_ANIM_TRAJECTORY )
                    {
                        vec3f p = scene->world_matrices[n].get_translation();
                        
                        put::dbg::add_aabb( p - vec3f(0.1f, 0.1f, 0.1f), p + vec3f(0.1f, 0.1f, 0.1f), vec4f::green() );
                    }
                    
                    u32 p = scene->parents[n];
                    if( p != n )
                    {
                        if( !(scene->entities[p] & CMP_BONE) || (scene->entities[p] & CMP_ANIM_TRAJECTORY) )
                            continue;
                        
                        vec3f p1 = scene->world_matrices[n].get_translation();
                        vec3f p2 = scene->world_matrices[p].get_translation();
                        
                        put::dbg::add_line(p1, p2, vec4f::magenta() );
                    }
                }
            }
            
            if( scene->debug_flags & DD_GRID )
            {
                put::dbg::add_grid(vec3f::zero(), vec3f(100.0f), 100);
            }
            
            if( scene->debug_flags & DD_NODE )
            {
                vec3f p = scene->world_matrices[scene->selected_index].get_translation();
                
                put::dbg::add_aabb( p - vec3f(0.1f, 0.1f, 0.1f), p + vec3f(0.1f, 0.1f, 0.1f));
            }
            
            put::dbg::render_3d(view.cb_view);
        }
    }
}
