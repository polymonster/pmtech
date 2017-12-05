#include "ces/ces_editor.h"
#include "ces/ces_resources.h"
#include "ces/ces_utilities.h"

#include "hash.h"
#include "pen_string.h"
#include "dev_ui.h"
#include "debug_render.h"
#include "input.h"
#include "camera.h"
#include "render_controller.h"
#include "timer.h"

namespace put
{
    namespace ces
    {
        static hash_id ID_PICKING_BUFFER = PEN_HASH("picking");
        
        enum e_camera_mode : s32
        {
            CAMERA_MODELLING = 0,
            CAMERA_FLY = 1
        };
        
        const c8* camera_mode_names[] =
        {
            "Modelling",
            "Fly"
        };
        
        struct model_view_controller
        {
            put::camera     main_camera;
            e_camera_mode   camera_mode = CAMERA_MODELLING;
            
        };
        model_view_controller k_model_view_controller;
        
        enum transform_mode : u32
        {
            TRANSFORM_NONE = 0,
            TRANSFORM_SELECT = 1,
            TRANSFORM_TRANSLATE = 2,
            TRANSFORM_ROTATE = 3,
            TRANSFORM_SCALE = 4
        };
        static transform_mode k_transform_mode = TRANSFORM_NONE;
        
        void update_model_viewer_camera(put::camera_controller* cc)
        {
            //update camera
            if( !(dev_ui::want_capture() & dev_ui::MOUSE) )
            {
                switch (k_model_view_controller.camera_mode)
                {
                    case CAMERA_MODELLING:
                        put::camera_update_modelling(cc->camera);
                        break;
                    case CAMERA_FLY:
                        put::camera_update_fly(cc->camera);
                        break;
                }
            }
        }
        
        struct picking_info
        {
            u32 result;
            a_u8 ready;
            u32 offset = 0;
        };
        static picking_info k_picking_info;
        
        std::vector<u32> k_selection_list;
        
        void enumerate_selection_ui( const entity_scene* scene, bool* opened )
        {
            if( ImGui::Begin("Selection List", opened) )
            {
                ImGui::Text("Picking Result: %u", k_picking_info.result );
                
                for( s32 i = 0; i < k_selection_list.size(); ++i )
                {
                    s32 ii = k_selection_list[ i ];
                    
                    ImGui::Text("%s", scene->names[ii].c_str() );
                }
                
                ImGui::End();
            }
        }
        
        void picking_read_back( void* p_data )
        {
            k_picking_info.result = *((u32*)(((u8*)p_data) + k_picking_info.offset));
            k_picking_info.ready = 1;
        }
        
        enum picking_mode : u32
        {
            PICK_NORMAL = 0,
            PICK_ADD = 1,
            PICK_REMOVE = 2
        };
        
        void picking_update( const entity_scene* scene )
        {
            static u32 picking_state = 0;
            static u32 picking_result = (-1);
            u32 picking_mode = PICK_NORMAL;
            
            if( INPUT_PKEY(PENK_CONTROL) )
            {
                picking_mode = PICK_ADD;
                ImGui::SetTooltip("%s", "+");
            }
            
            if( INPUT_PKEY(PENK_MENU) )
            {
                picking_mode = PICK_REMOVE;
                ImGui::SetTooltip("%s", "-");
            }

            if( picking_state == 1 )
            {
                if( k_picking_info.ready )
                {
                    picking_state = 0;
                    picking_result = k_picking_info.result;
                    
                    bool valid = picking_result < scene->num_nodes;
                    
                    if( picking_mode == PICK_NORMAL )
                    {
                        k_selection_list.clear();
                        if( valid )
                             k_selection_list.push_back(picking_result);
                    }
                    else if( valid )
                    {
                        s32 existing = -1;
                        for( s32 i = 0; i < k_selection_list.size(); ++i)
                            if( k_selection_list[i] == picking_result)
                                existing = i;
                        
                        if( existing != -1 && picking_mode == PICK_REMOVE )
                            k_selection_list.erase(k_selection_list.begin()+existing);
                        
                        if( existing == -1 && picking_mode == PICK_ADD )
                            k_selection_list.push_back(picking_result);
                    }

                }
            }
            else
            {
                if( !(dev_ui::want_capture() & dev_ui::MOUSE) )
                {
                    pen::mouse_state ms = pen::input_get_mouse_state();
                    
                    if (ms.buttons[PEN_MOUSE_L] && pen::mouse_coords_valid( ms.x, ms.y ) )
                    {
                        const put::render_target* rt = render_controller::get_render_target(ID_PICKING_BUFFER);
                        
                        f32 w, h;
                        render_controller::get_render_target_dimensions(rt, w, h);
                        
                        u32 pitch = (u32)w*4;
                        u32 data_size = (u32)h*pitch;
                        c8* p_data = new c8[data_size];
                        
                        pen::resource_read_back_params rrbp =
                        {
                            rt->handle,
                            (void*)p_data,
                            rt->format,
                            data_size,
                            &picking_read_back
                        };
                        
                        pen::renderer_read_back_resource( rrbp );
                        
                        k_picking_info.ready = 0;
                        k_picking_info.offset = ms.y * pitch + ms.x * 4;
                        
                        picking_state = 1;
                    }
                }
            }
        }
        
        void update_model_viewer_scene(put::scene_controller* sc)
        {
            static bool open_scene_browser = false;
            static bool open_import = false;
            static bool open_save = false;
            static bool open_camera_menu = false;
            static bool open_resource_menu = false;
            static bool dev_open = false;
            static bool set_project_dir = false;
            static bool picking_mode = false;
            static bool selection_list = false;
            static Str project_dir_str = dev_ui::get_program_preference("project_dir").as_str();
            
            ImGui::BeginMainMenuBar();
            
            if (ImGui::BeginMenu(ICON_FA_LEMON_O))
            {
                ImGui::MenuItem("Save");
                ImGui::MenuItem("Import", NULL, &open_import);
                
                if( ImGui::BeginMenu("Project Directory") )
                {
                    ImGui::MenuItem("Set..", NULL, &set_project_dir);
                    ImGui::Text("Dir: %s", project_dir_str.c_str());
                    
                    ImGui::EndMenu();
                }
                
                ImGui::MenuItem("Dev", NULL, &dev_open);
                
                ImGui::EndMenu();
            }
            
            if (ImGui::Button(ICON_FA_FLOPPY_O))
            {
                open_save = true;
            }
            
            if (ImGui::Button(ICON_FA_FOLDER_OPEN))
            {
                open_import = true;
            }
            
            if (ImGui::Button(ICON_FA_SEARCH))
            {
                open_scene_browser = true;
            }
            
            if (ImGui::Button(ICON_FA_VIDEO_CAMERA))
            {
                open_camera_menu = true;
            }
            
            if (ImGui::Button(ICON_FA_CUBES))
            {
                open_resource_menu = true;
            }
            
            ImGui::Separator();
            
            ImVec4 grey( 0.5, 0.5, 0.5, 1.0 );
            
            bool p = picking_mode;
            if(p)
                ImGui::PushStyleColor( ImGuiCol_Button, grey);
            
            if (ImGui::Button(ICON_FA_MOUSE_POINTER))
            {
                k_transform_mode = TRANSFORM_SCALE;
                
                picking_mode = !picking_mode;
            }
            
            if(p)
                ImGui::PopStyleColor();
            
            if (ImGui::Button(ICON_FA_LIST))
            {
                selection_list = true;
            }
            
            if ( ImGui::Button(ICON_FA_ARROWS) )
            {
                k_transform_mode = TRANSFORM_TRANSLATE;
            }
            
            ImGui::Separator();
            
            ImGui::EndMainMenuBar();
            
            if( open_import )
            {
                const c8* import = put::dev_ui::file_browser(open_import, 2, dev_ui::FB_OPEN, "**.pmm", "**.pms" );
                
                if( import )
                {
                    u32 len = pen::string_length( import );
                    
                    if( import[len-1] == 'm' )
                    {
                        put::ces::load_pmm( import, sc->scene );
                    }
                    else if( import[len-1] == 's' )
                    {
                        put::ces::load_scene( import, sc->scene );
                    }
                }
            }
            
            if (open_scene_browser)
            {
                ces::scene_browser_ui(sc->scene, &open_scene_browser);
            }
            
            if( open_camera_menu )
            {
                if( ImGui::Begin("Camera", &open_camera_menu) )
                {
                    ImGui::Combo("Camera Mode", (s32*)&k_model_view_controller.camera_mode, (const c8**)&camera_mode_names, 2);
                    
                    ImGui::End();
                }
            }
            
            if( open_resource_menu )
            {
                put::ces::enumerate_resources( &open_resource_menu );
            }
            
            if( set_project_dir )
            {
                const c8* set_proj = put::dev_ui::file_browser(set_project_dir, dev_ui::FB_OPEN, 1, "**." );
                
                if(set_proj)
                {
                    project_dir_str = set_proj;
                    dev_ui::set_program_preference("project_dir", project_dir_str);
                }
            }
            
            if( open_save )
            {
                const c8* save_file = put::dev_ui::file_browser(open_save, dev_ui::FB_SAVE, 1, "**.pms" );
                
                if(save_file)
                {
                    put::ces::save_scene(save_file, sc->scene);
                }
            }
            
            if( dev_open )
            {
                if( ImGui::Begin("Dev", &dev_open) )
                {
                    if( ImGui::CollapsingHeader("Icons") )
                    {
                        debug_show_icons();
                    }
                    
                    ImGui::End();
                }
            }
            
            if( picking_mode )
            {
                picking_update( sc->scene );
            }
            
            if( selection_list )
            {
                enumerate_selection_ui( sc->scene, &selection_list );
            }
            
            static u32 timer_index = pen::timer_create("scene_update_timer");
            
            pen::timer_accum(timer_index);
            f32 dt_ms = pen::timer_get_ms(timer_index);
            pen::timer_reset(timer_index);
            pen::timer_start(timer_index);
            
            //update render data
            put::ces::update_scene(sc->scene, dt_ms);
        }
        
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
        
        void render_scene_editor( const scene_view& view )
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
            
            if( scene->debug_flags & DD_NODE )
            {
                for( auto& s : k_selection_list )
                {
                    put::dbg::add_aabb( scene->bounding_volumes[s].transformed_min_extents, scene->bounding_volumes[s].transformed_max_extents );
                }
            }
            
            if( k_transform_mode == TRANSFORM_TRANSLATE )
            {
                vec3f pos = vec3f::zero();
                
                for( auto& s : k_selection_list )
                    pos += scene->world_matrices[s].get_translation();
                
                pos /= (f32)k_selection_list.size();
                
                mat4 widget;
                widget.set_vectors(vec3f::unit_x(), vec3f::unit_y(), vec3f::unit_z(), pos);
                
                put::dbg::add_coord_space(widget, 2.0f);
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
            
            put::dbg::render_3d(view.cb_view);
        }
    }
}
