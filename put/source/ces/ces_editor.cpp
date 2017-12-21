#include "ces/ces_editor.h"
#include "ces/ces_resources.h"
#include "ces/ces_utilities.h"

#include "hash.h"
#include "pen_string.h"
#include "dev_ui.h"
#include "debug_render.h"
#include "input.h"
#include "camera.h"
#include "pmfx_controller.h"
#include "timer.h"
#include "str_utilities.h"
#include "physics_cmdbuf.h"

namespace put
{
    namespace dev_ui
    {
        extern bool k_console_open;
    }
    
    namespace ces
    {
		struct transform_undo
		{
			transform state;
			u32		  node_index;
		};

        static hash_id ID_PICKING_BUFFER = PEN_HASH("picking");

		struct picking_info
		{
			u32 result;
			a_u8 ready;
			u32 x, y;
		};
		static picking_info k_picking_info;

		std::vector<u32> k_selection_list;
		enum e_select_mode : u32
		{
			SELECT_NORMAL = 0,
			SELECT_ADD = 1,
			SELECT_REMOVE = 2
		};

		enum e_picking_state : u32
		{
			PICKING_READY = 0,
			PICKING_SINGLE = 1,
			PICKING_MULTI = 2
		};

		enum e_select_flags : u32
		{
			NONE = 0,
			WIDGET_SELECTED = 1,
		};
		static u32 k_select_flags = 0;
        
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
			bool			invalidated = false;
			bool			invert_y = false;
            e_camera_mode   camera_mode = CAMERA_MODELLING;
			f32				grid_cell_size;
			f32				grid_size;
			Str				current_working_scene = "";
        };
        model_view_controller k_model_view_controller;
        
        enum transform_mode : u32
        {
            TRANSFORM_NONE = 0,
            TRANSFORM_SELECT = 1,
            TRANSFORM_TRANSLATE = 2,
            TRANSFORM_ROTATE = 3,
            TRANSFORM_SCALE = 4,
			TRANSFORM_TYPE_IN 
        };
        static transform_mode k_transform_mode = TRANSFORM_NONE;

		bool shortcut_key(u32 key)
		{
			u32 flags = dev_ui::want_capture();
			
			if (flags & dev_ui::KEYBOARD)
				return false;

			if (flags & dev_ui::TEXT)
				return false;

			return pen_input_key(key);
		}
        
        void update_model_viewer_camera(put::camera_controller* cc)
        {
			if (k_model_view_controller.invalidated)
			{
				cc->camera->fov = k_model_view_controller.main_camera.fov;
				cc->camera->near_plane = k_model_view_controller.main_camera.near_plane;
				cc->camera->far_plane = k_model_view_controller.main_camera.far_plane;
				camera_update_projection_matrix(cc->camera);
				k_model_view_controller.invalidated = false;
			}

			bool has_focus = dev_ui::want_capture() == dev_ui::NO_INPUT;

            switch (k_model_view_controller.camera_mode)
            {
                case CAMERA_MODELLING:
                    put::camera_update_modelling(cc->camera, has_focus, k_model_view_controller.invert_y);
                    break;
                case CAMERA_FLY:
                    put::camera_update_fly(cc->camera, has_focus, k_model_view_controller.invert_y);
                    break;
            }
        }
        
        enum e_debug_draw_flags
        {
            DD_HIDE     = SV_HIDE,
            DD_NODE     = 1<<(SV_BITS_END+1),
            DD_GRID     = 1<<(SV_BITS_END+2),
            DD_MATRIX   = 1<<(SV_BITS_END+3),
            DD_BONES    = 1<<(SV_BITS_END+4),
            DD_AABB     = 1<<(SV_BITS_END+5),
            DD_LIGHTS   = 1<<(SV_BITS_END+6),

            DD_NUM_FLAGS = 7,
        };
        
        const c8* dd_names[]
        {
            "Hide Scene",
            "Selected Node",
            "Grid",
            "Matrices",
            "Bones",
            "AABB",
            "Lights"
        };
        static_assert(sizeof(dd_names)/sizeof(dd_names[0]) == DD_NUM_FLAGS, "mismatched");
        static bool* k_dd_bools = nullptr;

	
        void update_view_flags_ui( entity_scene* scene )
        {
            if(!k_dd_bools)
            {
                k_dd_bools = new bool[DD_NUM_FLAGS];
                pen::memory_set(k_dd_bools, 0x0, sizeof(bool)*DD_NUM_FLAGS);
                
                //set defaults
                static u32 defaults[] = { DD_NODE, DD_GRID, DD_LIGHTS };
                for( s32 i = 1; i < sizeof(defaults)/sizeof(defaults[0]); ++i )
                    k_dd_bools[ i ] = true;
            }
            
            for( s32 i = 0; i < DD_NUM_FLAGS; ++i )
            {
                u32 mask = 1<<i;
                
                if(k_dd_bools[i])
                    scene->view_flags |= mask;
                else
                    scene->view_flags &= ~(mask);
            }
        }
        
        void update_view_flags( entity_scene* scene, bool error )
        {
            if( error )
                scene->view_flags |= (DD_MATRIX | DD_BONES);
            
            for( s32 i = 0; i < DD_NUM_FLAGS; ++i )
            {
                k_dd_bools[i] = false;
                
                if( scene->view_flags & (1<<i) )
                    k_dd_bools[i] = true;
            }
        }
        
        void view_ui( entity_scene* scene, bool* opened )
        {
            if( ImGui::Begin("View", opened, ImGuiWindowFlags_AlwaysAutoResize ) )
            {
                for( s32 i = 0; i < DD_NUM_FLAGS; ++i )
                {
                    ImGui::Checkbox(dd_names[i], &k_dd_bools[i]);
                }

				if (ImGui::CollapsingHeader("Grid Options"))
				{
					if(ImGui::InputFloat("Cell Size", &k_model_view_controller.grid_cell_size))
						dev_ui::set_program_preference("grid_cell_size", k_model_view_controller.grid_cell_size);

					if(ImGui::InputFloat("Grid Size", &k_model_view_controller.grid_size))
						dev_ui::set_program_preference("grid_size", k_model_view_controller.grid_size);
				}
                
                ImGui::End();
            }
            
            update_view_flags_ui( scene );
        }


		void default_scene(entity_scene* scene)
		{
			//add default view flags
			scene->view_flags = 0;
			delete[] k_dd_bools;
			k_dd_bools = nullptr;
			update_view_flags_ui(scene);

			//add front light
			u32 light = get_new_node(scene);
			scene->names[light] = "front_light";
			scene->id_name[light] = PEN_HASH("front_light");
			scene->lights[light].colour = vec3f::one();
			scene->transforms->translation = vec3f(100.0f, 100.0f, 100.0f);
			scene->transforms->rotation = quat();
			scene->transforms->scale = vec3f::one();

			scene->entities[light] |= CMP_LIGHT;
			scene->entities[light] |= CMP_TRANSFORM;

			k_selection_list.clear();
		}
        
        void editor_init( entity_scene* scene )
        {
            update_view_flags_ui( scene );

			bool auto_load_last_scene = dev_ui::get_program_preference("load_last_scene").as_bool();
			if (auto_load_last_scene)
			{
				Str last_loaded_scene = dev_ui::get_program_preference_filename("last_loaded_scene");

				if (last_loaded_scene.length() > 0)
					load_scene(last_loaded_scene.c_str(), scene);

				k_model_view_controller.current_working_scene = last_loaded_scene;

				auto_load_last_scene = false;
			}
			else
			{
				default_scene(scene);
			}

			//grid
			k_model_view_controller.grid_cell_size = dev_ui::get_program_preference("grid_cell_size").as_f32(1.0f);
			k_model_view_controller.grid_size = dev_ui::get_program_preference("grid_size").as_f32(100.0f);

			//camera
			k_model_view_controller.main_camera.fov = dev_ui::get_program_preference("camera_fov").as_f32(60.0f);
			k_model_view_controller.main_camera.near_plane = dev_ui::get_program_preference("camera_near").as_f32(0.1f);
			k_model_view_controller.main_camera.far_plane = dev_ui::get_program_preference("camera_far").as_f32(1000.0f);
			k_model_view_controller.invert_y = dev_ui::get_program_preference("camera_invert_y").as_bool();
			k_model_view_controller.invalidated = true;
        }

		void editor_shutdown()
		{
			delete[] k_dd_bools;
		}
        
		void parent_selection(entity_scene* scene )
		{
			if (k_selection_list.size() > 1)
			{
				s32 parent = k_selection_list[0];

				for (auto& i : k_selection_list)
					if (scene->parents[i] == i)
						set_node_parent(scene, parent, i);
			}
		}

		void add_selection( const entity_scene* scene, u32 index, u32 select_mode = SELECT_NORMAL )
		{
			if (pen::input_is_key_down(PENK_CONTROL))
				select_mode = SELECT_REMOVE;
			else if (pen::input_is_key_down(PENK_SHIFT))
				select_mode = SELECT_ADD;

			bool valid = index < scene->num_nodes;

			if (select_mode == SELECT_NORMAL)
			{
				k_selection_list.clear();
				if (valid)
					k_selection_list.push_back(index);
			}
			else if (valid)
			{
				s32 existing = -1;
				for (s32 i = 0; i < k_selection_list.size(); ++i)
					if (k_selection_list[i] == index)
						existing = i;

				if (existing != -1 && select_mode == SELECT_REMOVE)
					k_selection_list.erase(k_selection_list.begin() + existing);

				if (existing == -1 && select_mode == SELECT_ADD)
					k_selection_list.push_back(index);
			}
		}

        void enumerate_selection_ui( const entity_scene* scene, bool* opened )
        {
            if( ImGui::Begin("Selection List", opened) )
            {
                //ImGui::Text("Picking Result: %u", k_picking_info.result );
                
                for( s32 i = 0; i < k_selection_list.size(); ++i )
                {
                    s32 ii = k_selection_list[ i ];
                    
                    ImGui::Text("%s", scene->names[ii].c_str() );
                }
                
                ImGui::End();
            }
        }
        
		void picking_read_back(void* p_data, u32 row_pitch, u32 depth_pitch, u32 block_size )
        {
            k_picking_info.result = *((u32*)(((u8*)p_data) + k_picking_info.y * row_pitch + k_picking_info.x * block_size));
            
			k_picking_info.ready = 1;
        }
                
        void picking_update( const entity_scene* scene, const camera* cam )
        {
            static u32 picking_state = PICKING_READY;
            static u32 picking_result = (-1);

            if( picking_state == PICKING_SINGLE )
            {
                if( k_picking_info.ready )
                {
                    picking_state = PICKING_READY;
                    picking_result = k_picking_info.result;
                    
					add_selection(scene, picking_result);
                }
            }
            else
            {
                if( !(dev_ui::want_capture() & dev_ui::MOUSE) )
                {
                    pen::mouse_state ms = pen::input_get_mouse_state();
					f32 corrected_y = pen_window.height - ms.y;

					static s32		drag_timer = 0;
					static vec2f	drag_start;
					static vec3f	frustum_points[2][4];

					//frustum select
					vec3f n[6];
					vec3f p[6];

					vec3f plane_vectors[] =
					{
						frustum_points[0][0], frustum_points[1][0], frustum_points[0][2],	//left
						frustum_points[0][0], frustum_points[0][1],	frustum_points[1][0],	//top

						frustum_points[0][1], frustum_points[0][3], frustum_points[1][1],	//right
						frustum_points[0][2], frustum_points[1][2], frustum_points[0][3],	//bottom

						frustum_points[0][0], frustum_points[0][2], frustum_points[0][1],	//near
						frustum_points[1][0], frustum_points[1][1], frustum_points[1][2]	//far
					};

					for (s32 i = 0; i < 6; ++i)
					{
						s32 offset = i * 3;
						vec3f v1 = maths::normalise(plane_vectors[offset + 1] - plane_vectors[offset + 0]);
						vec3f v2 = maths::normalise(plane_vectors[offset + 2] - plane_vectors[offset + 0]);

						n[i] = put::maths::cross(v1, v2);
						p[i] = plane_vectors[offset];
					}
					
					if (!ms.buttons[PEN_MOUSE_L])
					{
						if (picking_state == PICKING_MULTI)
						{
							u32 pm = SELECT_NORMAL;
							if (!pen::input_is_key_down(PENK_CONTROL) && !pen::input_is_key_down(PENK_SHIFT))
							{
								k_selection_list.clear();
								pm = SELECT_ADD;
							}

							for (s32 node = 0; node < scene->num_nodes; ++node)
							{
								if (!(scene->entities[node] & CMP_ALLOCATED))
									continue;

								if (!(scene->entities[node] & CMP_GEOMETRY))
									continue;

								bool selected = true;
								for (s32 i = 0; i < 6; ++i)
								{
									vec3f& min = scene->bounding_volumes[node].transformed_min_extents;
									vec3f& max = scene->bounding_volumes[node].transformed_max_extents;

									vec3f pos = min + (max - min) * 0.5f;
									f32 radius = scene->bounding_volumes[node].radius;

									f32 d = maths::point_vs_plane(pos, p[i], n[i]);
									
									if (d > radius)
									{
										selected = false;
										break;
									}
								}

								if (selected)
								{
									add_selection(scene, node, SELECT_ADD);
								}
							}

							picking_state = PICKING_READY;
						}
							
						drag_timer = 0;
						drag_start = vec2f(ms.x, corrected_y);
					}

                    if (ms.buttons[PEN_MOUSE_L] && pen::mouse_coords_valid( ms.x, ms.y ) )
                    {
						vec2f cur_mouse = vec2f(ms.x, corrected_y);

						vec2f c1 = vec2f(cur_mouse.x, drag_start.y);
						vec2f c2 = vec2f(drag_start.x, cur_mouse.y);

						//frustum selection
						vec2f source_points[] =
						{
							drag_start, c1,
							c2, cur_mouse
						};

						//sort source points 
						vec2f min = vec2f::flt_max();
						vec2f max = vec2f::flt_min();
						for (s32 i = 0; i < 4; ++i)
						{
							min = vec2f::vmin(source_points[i], min);
							max = vec2f::vmax(source_points[i], max);
						}

						source_points[0] = vec2f(min.x, max.y);
						source_points[1] = vec2f(max.x, max.y);
						source_points[2] = vec2f(min.x, min.y);
						source_points[3] = vec2f(max.x, min.y);

						put::dbg::add_line_2f(source_points[0], source_points[1]);
						put::dbg::add_line_2f(source_points[0], source_points[2]);
						put::dbg::add_line_2f(source_points[2], source_points[3]);
						put::dbg::add_line_2f(source_points[3], source_points[1]);

						if (maths::magnitude(max - min) < 4.0)
						{
							const put::render_target* rt = pmfx::get_render_target(ID_PICKING_BUFFER);

							f32 w, h;
							pmfx::get_render_target_dimensions(rt, w, h);

							u32 pitch = (u32)w * 4;
							u32 data_size = (u32)h*pitch;

							pen::resource_read_back_params rrbp =
							{
								rt->handle,
								rt->format,
								pitch,
								data_size,
								4,
								data_size,
								&picking_read_back
							};

							pen::renderer_read_back_resource(rrbp);

							k_picking_info.ready = 0;
							k_picking_info.x = ms.x;
							k_picking_info.y = ms.y;

							picking_state = 1;
						}
						else
						{
							picking_state = 2;

							//todo this should really be passed in, incase we want non window sized viewports
							vec2i vpi = vec2i(pen_window.width, pen_window.height);

							for (s32 i = 0; i < 4; ++i)
							{
								frustum_points[0][i] = maths::unproject(vec3f(source_points[i], 0.0f), cam->view, cam->proj, vpi);
								frustum_points[1][i] = maths::unproject(vec3f(source_points[i], 1.0f), cam->view, cam->proj, vpi);
							}
						}
                    }
                }
            }
        }

		void settings_ui(bool* opened)
		{
			static bool set_project_dir = false;

			static bool load_last_scene = dev_ui::get_program_preference("load_last_scene").as_bool();
			static Str project_dir_str = dev_ui::get_program_preference_filename("project_dir");

			if (ImGui::Begin("Settings", opened))
			{
				Str setting_str;
				if (ImGui::Checkbox("Invert Camera Y", &k_model_view_controller.invert_y))
				{
					dev_ui::set_program_preference("invert_camera_y", k_model_view_controller.invert_y);
				}

				if (ImGui::Checkbox("Auto Load Last Scene", &load_last_scene))
				{
					dev_ui::set_program_preference("load_last_scene", load_last_scene);
				}

				set_project_dir = ImGui::Button("Set Project Dir");
				ImGui::SameLine();
				ImGui::Text("%s", project_dir_str.c_str());

				ImGui::End();
			}

			if (set_project_dir)
			{
				const c8* set_proj = put::dev_ui::file_browser(set_project_dir, dev_ui::FB_OPEN, 1, "**.");

				if (set_proj)
				{
					project_dir_str = set_proj;
					dev_ui::set_program_preference_filename("project_dir", project_dir_str);
				}
			}
		}
        
        void update_model_viewer_scene(put::scene_controller* sc)
        {
            static bool open_scene_browser = false;
            static bool open_merge = false;
			static bool open_open = false;
            static bool open_save = false;
			static bool open_save_as = false;
            static bool open_camera_menu = false;
            static bool open_resource_menu = false;
            static bool dev_open = false;
            static bool selection_list = false;
            static bool view_menu = false;
			static bool settings_open = false;
            
            ImGui::BeginMainMenuBar();
            
            if (ImGui::BeginMenu(ICON_FA_LEMON_O))
            {
				if (ImGui::MenuItem("New"))
				{
					clear_scene(sc->scene);
					default_scene(sc->scene);
				}
					
				if (ImGui::MenuItem("Open", NULL, &open_open))
				{
					open_merge = false;
				}
				if (ImGui::MenuItem("Import", NULL, &open_open))
				{
					open_merge = true;
				}

				if (ImGui::MenuItem("Save", NULL, &open_save))
				{
					open_save_as = false;
				}
				
				if (ImGui::MenuItem("Save As", NULL, &open_save))
				{
					open_save_as = true;
				}

                ImGui::MenuItem("Console", NULL, &put::dev_ui::k_console_open);
				ImGui::MenuItem("Settings", NULL, &settings_open);
                ImGui::MenuItem("Dev", NULL, &dev_open);

                ImGui::EndMenu();
            }
            
			//play pause
            if( sc->scene->flags & PAUSE_UPDATE )
            {
                if(ImGui::Button(ICON_FA_PLAY))
                    sc->scene->flags &= (~sc->scene->flags);
            }
            else
            {
                if(ImGui::Button(ICON_FA_PAUSE))
                    sc->scene->flags |= PAUSE_UPDATE;
            }

			static bool debounce_pause = false;
			if (shortcut_key(PENK_SPACE))
			{
				if (!debounce_pause)
				{
					if(!(sc->scene->flags & PAUSE_UPDATE))
						sc->scene->flags |= PAUSE_UPDATE;
					else
						sc->scene->flags &= ~PAUSE_UPDATE;

					debounce_pause = true;
				}
			}
			else
			{
				debounce_pause = false;
			}
			            
            if (ImGui::Button(ICON_FA_FLOPPY_O))
            {
				open_save = true;
				open_save_as = false;
            }
            dev_ui::set_tooltip("Save");
            
            if (ImGui::Button(ICON_FA_FOLDER_OPEN))
            {
				open_open = true;
            }
            dev_ui::set_tooltip("Open");
            
            if (ImGui::Button(ICON_FA_SEARCH))
            {
                open_scene_browser = true;
            }
            dev_ui::set_tooltip("Scene Browser");
            
            if (ImGui::Button(ICON_FA_EYE))
            {
                view_menu = true;
            }
            dev_ui::set_tooltip("View Settings");
            
            if (ImGui::Button(ICON_FA_VIDEO_CAMERA))
            {
                open_camera_menu = true;
            }
            dev_ui::set_tooltip("Camera Settings");
            
            if (ImGui::Button(ICON_FA_CUBES))
            {
                open_resource_menu = true;
            }
            dev_ui::set_tooltip("Resource Browser");
            
            ImGui::Separator();
            
            if (ImGui::Button(ICON_FA_LIST))
            {
                selection_list = true;
            }
            dev_ui::set_tooltip("Selection List");
            
            static const c8* transform_icons[] =
            {
                ICON_FA_MOUSE_POINTER,
                ICON_FA_ARROWS,
                ICON_FA_REPEAT,
                ICON_FA_EXPAND
            };
            static s32 num_transform_icons = PEN_ARRAY_SIZE(transform_icons);
            
            static const c8* transform_tooltip[] =
            { 
                "Select (Q)",
                "Translate (W)",
                "Rotate (E)",
                "Scale (R)"
            };
            static_assert(PEN_ARRAY_SIZE(transform_tooltip) == PEN_ARRAY_SIZE(transform_icons), "mistmatched elements");
            
            static u32 widget_shortcut_key[] =
            {
                PENK_Q, PENK_W, PENK_E, PENK_R
            };
            static_assert(PEN_ARRAY_SIZE(widget_shortcut_key) == PEN_ARRAY_SIZE(transform_tooltip), "mismatched elements");
            
            for( s32 i = 0; i < num_transform_icons; ++i )
            {
                u32 mode = TRANSFORM_SELECT + i;
                if( shortcut_key(widget_shortcut_key[i]))
                    k_transform_mode = (transform_mode)mode;
                
                if( put::dev_ui::state_button(transform_icons[i], k_transform_mode == mode ) )
                {
                    if( k_transform_mode == mode )
                        k_transform_mode = TRANSFORM_NONE;
                    else
                        k_transform_mode = (transform_mode)mode;
                }
                put::dev_ui::set_tooltip(transform_tooltip[i]);
            }

			if (ImGui::Button(ICON_FA_LINK) || shortcut_key(PENK_P) )
			{
				parent_selection( sc->scene );
			}
			put::dev_ui::set_tooltip("Parent (P)");
            
            ImGui::Separator();
            
            ImGui::EndMainMenuBar();
            
            if( open_open )
            {
                const c8* import = put::dev_ui::file_browser(open_open, dev_ui::FB_OPEN, 2, "**.pmm", "**.pms" );
                
                if( import )
                {
                    u32 len = pen::string_length( import );
                    
                    if( import[len-1] == 'm' )
                    {
                        put::ces::load_pmm( import, sc->scene );
                    }
                    else if( import[len-1] == 's' )
                    {
                        put::ces::load_scene( import, sc->scene, open_merge );

						if (!open_merge)
							k_model_view_controller.current_working_scene = import;

						Str fn = import;
						dev_ui::set_program_preference_filename("last_loaded_scene", import);
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
                    
					if (ImGui::SliderFloat("FOV", &k_model_view_controller.main_camera.fov, 10, 180))
						dev_ui::set_program_preference("camera_fov", k_model_view_controller.main_camera.fov);

					if (ImGui::InputFloat("Near", &k_model_view_controller.main_camera.near_plane))
						dev_ui::set_program_preference("camera_near", k_model_view_controller.main_camera.near_plane);

					if (ImGui::InputFloat("Far", &k_model_view_controller.main_camera.far_plane))
						dev_ui::set_program_preference("camera_far", k_model_view_controller.main_camera.far_plane);

					k_model_view_controller.invalidated = true;

                    ImGui::End();
                }
            }
            
            if( open_resource_menu )
            {
                put::ces::enumerate_resources( &open_resource_menu );
            }
                        
            if( open_save )
            {
				const c8* save_file = nullptr;
				if (open_save_as || k_model_view_controller.current_working_scene.length() == 0)
				{
					save_file = put::dev_ui::file_browser(open_save, dev_ui::FB_SAVE, 1, "**.pms");
				}
				else
				{
					save_file = k_model_view_controller.current_working_scene.c_str();
				}

                if(save_file)
                {
                    put::ces::save_scene(save_file, sc->scene);
					k_model_view_controller.current_working_scene = save_file;
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

			if (settings_open)
				settings_ui(&settings_open);

			//disable selection when we are doing something else
			static bool disable_picking = false;
			if (pen_input_key(PENK_MENU) || pen_input_key(PENK_COMMAND) || (k_select_flags & WIDGET_SELECTED))
			{
				disable_picking = true;
			}
			else
			{
				if (!pen_input_mouse(PEN_MOUSE_L))
					disable_picking = false;
			}
         
			//selection / picking
            if( !disable_picking )
            {
                picking_update( sc->scene, sc->camera );
            }

			if (selection_list)
			{
				enumerate_selection_ui(sc->scene, &selection_list);
			}

			//duplicate
			static bool debounce_duplicate = false;
			if (pen_input_key(PENK_CONTROL) && shortcut_key(PENK_D))
			{
				debounce_duplicate = true;
			}
			else if (debounce_duplicate)
			{
				clone_selection_hierarchical(sc->scene, k_selection_list, "_cloned");
				debounce_duplicate = false;
			}

			//delete
			if (shortcut_key(PENK_DELETE) || shortcut_key(PENK_BACK))
			{
				for (auto& s : k_selection_list)
				{					
					std::vector<s32> node_index_list;
					build_heirarchy_node_list(sc->scene, s, node_index_list);

					for (auto& c : node_index_list)
						if (c > -1)
							zero_entity_components(sc->scene, c);
				}
				k_selection_list.clear();

				sc->scene->flags |= put::ces::INVALIDATE_SCENE_TREE;
			}
            
            if( view_menu )
            {
                view_ui( sc->scene, &view_menu );
            }
            
            static u32 timer_index = pen::timer_create("scene_update_timer");
            
            pen::timer_accum(timer_index);
            f32 dt_ms = pen::timer_get_ms(timer_index);
            pen::timer_reset(timer_index);
            pen::timer_start(timer_index);
            
            //update render data
            put::ces::update_scene(sc->scene, dt_ms);
        }
        
        void scene_physics_ui( entity_scene* scene, s32 selected_index )
        {
            if( ImGui::CollapsingHeader("Physics") )
            {
                scene_node_physics& snp = scene->physics_data[selected_index];
                
                ImGui::InputFloat("Mass", &snp.mass);
                ImGui::Combo("Shape", (s32*)&snp.collision_shape, "Box\0Cylinder\0Sphere\0Capsule\0Hull\0Mesh\0Compound\0", 7 );
                
                static s32 dummy = 0;
                if( snp.collision_shape == physics::CYLINDER || snp.collision_shape == physics::CAPSULE )
                    ImGui::Combo("Shape Up-Axis", (s32*)&dummy, "X\0Y\0Z\0", 7 );
                
                if( ImGui::Button("Add Physics") )
                {
                    vec3f min = scene->bounding_volumes[selected_index].min_extents;
                    vec3f max = scene->bounding_volumes[selected_index].max_extents;
                    vec3f centre = min + (max - min);

                    vec3f pos = scene->transforms[selected_index].translation;
                    vec3f scale = scene->transforms[selected_index].scale;
                    quat rotation = scene->transforms[selected_index].rotation;
                    
                    scene->offset_matrices[selected_index] = mat4::create_scale(scale);
                    
                    physics::rigid_body_params rb = { 0 };
                    rb.dimensions = (max - min) * scale * 0.5;
                    rb.mass = snp.mass;
                    rb.group = 1;
                    rb.position = pos;
                    rb.rotation = rotation;
                    rb.shape = snp.collision_shape + 1;
                    rb.shape_up_axis = physics::UP_Y;
                    rb.mask = 0xffffffff;
                    
                    scene->physics_handles[selected_index] = physics::add_rb(rb);
                    
                    scene->entities[selected_index] |= CMP_PHYSICS;
                }
            }
        }
        
        void scene_anim_ui( entity_scene* scene, s32 selected_index )
        {
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
								build_heirarchy_node_list( scene, selected_index, joint_indices );
                                
                                s32 channel_index = 0;
                                s32 joints_offset = -1; //scene tree has a -1 node
                                bool compatible = true;
                                for( s32 jj = 0; jj < joint_indices.size(); ++jj )
                                {
                                    s32 jnode = joint_indices[jj];
                                    
                                    if( scene->entities[jnode] & CMP_BONE && jnode > -1)
                                    {
                                        if( anim->channels[channel_index].target != scene->id_name[jnode] )
                                        {
											dev_console_log_level(dev_ui::CONSOLE_ERROR, "%s", "[error] animation - does not fit rig" );
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
                                    
									s32 size = controller.handles.size();
									for( s32 h = 0; h < size; ++h )
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
        
        void scene_browser_ui( entity_scene* scene, bool* open )
        {
            if( ImGui::Begin("Scene Browser", open ) )
            {
                if( ImGui::Button( ICON_FA_PLUS ) )
                {
                    u32 nn = ces::get_new_node( scene );
                    
                    scene->entities[nn] |= CMP_ALLOCATED;
                    
                    scene->names[nn] = "node_";
                    scene->names[nn].appendf("%u", nn);
                    
                    scene->parents[nn] = nn;

					scene->transforms[nn].translation = vec3f::zero();
					scene->transforms[nn].rotation.euler_angles(0.0f, 0.0f, 0.0f);
					scene->transforms[nn].scale = vec3f::one();

					add_selection(scene, nn);
                }
                put::dev_ui::set_tooltip("Add New Node");
                
                ImGui::SameLine();
                
                static bool list_view = false;
                if( ImGui::Button(ICON_FA_LIST) )
                    list_view = true;
                dev_ui::set_tooltip("List View");
                
                ImGui::SameLine();
                if( ImGui::Button(ICON_FA_USB) )
                    list_view = false;
                dev_ui::set_tooltip("Tree View");
                
                //ImGui::Columns( 1 );
                
                ImGui::BeginChild("Entities", ImVec2(0, 300), true );
                
				s32 selected_index = -1;
				if (k_selection_list.size() == 1)
					selected_index = k_selection_list[0];
                
                if( list_view )
                {
                    for (u32 i = 0; i < scene->num_nodes; ++i)
                    {
						if (!(scene->entities[i] & CMP_ALLOCATED))
							continue;

                        bool selected = false;
                        ImGui::Selectable(scene->names[i].c_str(), &selected);
                        
                        if (selected)
                        {
							add_selection(scene, i);
                        }
                    }
                }
                else
                {
                    static scene_tree tree;
					if (scene->flags & INVALIDATE_SCENE_TREE)
					{
						tree = scene_tree();
						build_scene_tree(scene, -1, tree);

						scene->flags &= ~INVALIDATE_SCENE_TREE;
					}
                                        
					s32 pre_selected = selected_index;
                    scene_tree_enumerate(tree, selected_index);

					if(pre_selected != selected_index)
						add_selection(scene, selected_index);
                }
                
                ImGui::EndChild();
                
				struct scene_num_dump
				{
					u32 component;
					const c8* display_name;
					u32 count;
				};

				static scene_num_dump dumps[] = 
				{
					{ CMP_ALLOCATED, "Allocated", 0 },
					{ CMP_GEOMETRY, "Geometries", 0 },
					{ CMP_BONE, "Bones", 0 },
					{ CMP_ANIM_CONTROLLER, "Anim Controllers", 0 }
				};

				if (ImGui::CollapsingHeader("Scene Info"))
				{
					ImGui::Text("Total Scene Nodes: %i", scene->num_nodes);
					ImGui::Text("Selected: %i", k_selection_list.size());

					for (s32 i = 0; i < _countof(dumps); ++i)
						dumps[i].count = 0;

					for (s32 i = 0; i < scene->num_nodes; ++i)
						for (s32 j = 0; j < _countof(dumps); ++j)
							if (scene->entities[i] & dumps[j].component)
								dumps[j].count++;

					for (s32 i = 0; i < _countof(dumps); ++i)
						ImGui::Text("%s: %i", dumps[i].display_name, dumps[i].count);
				}
                
                if (selected_index != -1)
                {
                    //header
                    static c8 buf[64];
                    u32 end_pos = std::min<u32>(scene->names[selected_index].length(), 64);
                    pen::memory_cpy(buf, scene->names[selected_index].c_str(), end_pos);
                    buf[end_pos] = '\0';
                    
                    if( ImGui::InputText("", buf, 64 ))
                    {
                        scene->names[selected_index] = buf;
                        scene->id_name[selected_index] = PEN_HASH(buf);
                    }
                    
                    s32 parent_index = scene->parents[selected_index];
                    if( parent_index != selected_index)
                        ImGui::Text("Parent: %s", scene->names[parent_index].c_str());
                    
                    ImGui::Separator();

					//transform
					bool perform_transform = false;
					transform& t = scene->transforms[selected_index];
					perform_transform |= ImGui::InputFloat3("Translation", (float*)&t.translation);

					vec3f euler = t.rotation.to_euler();
					euler = euler * _PI_OVER_180;
					
					if (ImGui::InputFloat3("Rotation", (float*)&euler) )
					{
						euler = euler * _180_OVER_PI;
						t.rotation.euler_angles(euler.z, euler.y, euler.x);
						perform_transform = true;
					}

					perform_transform |= ImGui::InputFloat3("Scale", (float*)&t.scale);

					if (perform_transform)
					{
						scene->entities[selected_index] |= CMP_TRANSFORM;
					}

					apply_transform_to_selection(scene, vec3f::zero());

					ImGui::Separator();
                    
                    //geom
                    ImGui::Text("Geometry: %s", scene->geometry_names[selected_index].c_str());
                    ImGui::Separator();
                    
                    //material
					if (ImGui::CollapsingHeader("Material"))
					{
						ImGui::Text("%s", scene->material_names[selected_index].c_str());

						if (scene->material_names[selected_index].c_str())
						{
							u32 count = 0;
							for (u32 t = 0; t < put::ces::SN_NUM_TEXTURES; ++t)
							{
								if (scene->materials[selected_index].texture_id[t] > 0)
								{
									if (count++ > 0)
										ImGui::SameLine();

									ImGui::Image(&scene->materials[selected_index].texture_id[t], ImVec2(64, 64));
								}
							}
						}
					}
                    ImGui::Separator();
                    
                    scene_anim_ui(scene, selected_index );
                    
                    ImGui::Separator();
                    
                    scene_physics_ui(scene, selected_index );
                    
                    ImGui::Separator();
                    
                    if( ImGui::CollapsingHeader("Light") )
                    {
                        if( scene->entities[selected_index] & CMP_LIGHT )
                        {
                            ImGui::Combo("Type", (s32*)&scene->lights[selected_index].type, "Directional\0Point\0Spot\0", 3 );
                            
                            switch(scene->lights[selected_index].type)
                            {
                                case LIGHT_TYPE_DIR:
                                    ImGui::SliderAngle("Azimuth", &scene->lights[selected_index].data.x);
                                    ImGui::SliderAngle("Zenith", &scene->lights[selected_index].data.y);
                                    break;
                                    
                                case LIGHT_TYPE_POINT:
                                    ImGui::SliderFloat("Radius##slider", &scene->lights[selected_index].data.x, 0.0f, 100.0f );
                                    ImGui::InputFloat("Radius##input", &scene->lights[selected_index].data.x);
                                    break;
                                    
                                case LIGHT_TYPE_SPOT:
                                    ImGui::SliderAngle("Azimuth", &scene->lights[selected_index].data.x);
                                    ImGui::SliderAngle("Zenith", &scene->lights[selected_index].data.y);
                                    ImGui::SliderAngle("Cos Cutoff", &scene->lights[selected_index].data.z);
                                    break;
                            }
                            
                            ImGui::ColorPicker3("Colour", (f32*)&scene->lights[selected_index].colour);
                        }
                        else
                        {
                            if( ImGui::Button("Add Light") )
                            {
                                scene->entities[selected_index] |= CMP_LIGHT;
                            }
                        }
                    }
                }
                
                //ImGui::EndChild();
                
                //ImGui::Columns(1);
                
                ImGui::End();
            }
        }
        
        void apply_transform_to_selection( entity_scene* scene, const vec3f move_axis )
        {
            if( move_axis == vec3f::zero() )
                return;
            
            for (auto& s : k_selection_list)
            {
                //only move if parent isnt selected
                s32 parent = scene->parents[s];
                if (parent != s)
                {
                    bool found = false;
                    for (auto& pp : k_selection_list)
                        if (pp == parent)
                        {
                            found = true;
                            break;
                        }
                    
                    if (found)
                        continue;
                }
                
                transform& t = scene->transforms[s];
                if (k_transform_mode == TRANSFORM_TRANSLATE)
                    t.translation += move_axis;
                if (k_transform_mode == TRANSFORM_SCALE)
                    t.scale += move_axis * 0.1;
                if (k_transform_mode == TRANSFORM_ROTATE)
                {
                    quat q;
                    q.euler_angles(move_axis.z, move_axis.y, move_axis.x);
                    
                    t.rotation = q * t.rotation;
                }

				if (!(scene->entities[s] & CMP_TRANSFORM))
				{
					//save history
				}
                
                scene->entities[s] |= CMP_TRANSFORM;
            }
        }

		void transform_widget( const scene_view& view )
		{
			k_select_flags &= ~(WIDGET_SELECTED);

            if( k_selection_list.empty() )
                return;
            
			entity_scene* scene = view.scene;
			vec2i vpi = vec2i(view.viewport->width, view.viewport->height);

			static vec3f widget_points[4];
			static vec3f pre_click_axis_pos[3];
			static u32 selected_axis = 0;
			static f32 selection_radius = 5.0f;

			const pen::mouse_state& ms = pen::input_get_mouse_state();
			vec3f mousev3 = vec3f(ms.x, view.viewport->height - ms.y, 0.0f);

			vec3f r0 = put::maths::unproject(vec3f(mousev3.x, mousev3.y, 0.0f), view.camera->view, view.camera->proj, vpi);
			vec3f r1 = put::maths::unproject(vec3f(mousev3.x, mousev3.y, 1.0f), view.camera->view, view.camera->proj, vpi);
			vec3f vr = put::maths::normalise(r1 - r0);
            
            vec3f pos = vec3f::zero();
			vec3f min = vec3f::flt_max();
			vec3f max = vec3f::flt_min();

            for (auto& s : k_selection_list)
            {
                vec3f& _min = scene->bounding_volumes[s].transformed_min_extents;
                vec3f& _max = scene->bounding_volumes[s].transformed_max_extents;
                
				min = vec3f::vmin(min, _min);
				max = vec3f::vmax(max, _max);

                pos += _min + (_max - _min) * 0.5f;
            }

			f32 extents_mag = maths::magnitude(max - min);
			            
            pos /= (f32)k_selection_list.size();
            
            mat4 widget;
            widget.set_vectors(vec3f::unit_x(), vec3f::unit_y(), vec3f::unit_z(), pos);
            
			if (shortcut_key(PENK_F))
			{
				view.camera->focus = pos;
				view.camera->zoom = extents_mag;
			}

            //distance for consistent-ish size
            mat4 res = view.camera->proj * view.camera->view;
            
            f32 w = 1.0;
            vec3f screen_pos = res.transform_vector(pos, &w);
            f32 d = fabs(screen_pos.z) * 0.1f;
            
            if( screen_pos.z < -0.0 )
                return;
            
            if( k_transform_mode == TRANSFORM_ROTATE )
            {
                float rd = d * 0.75;
                
                vec3f plane_normals[ ] =
                {
                    vec3f( 1.0f, 0.0f, 0.0f ),
                    vec3f( 0.0f, 1.0f, 0.0f ),
                    vec3f( 0.0f, 0.0f, 1.0f )
                };

                vec3f _cp = vec3f::zero();
                static bool selected[3] = { 0 };
                for( s32 i = 0; i < 3; ++i )
                {
                    vec3f cp = maths::ray_vs_plane( vr, r0, plane_normals[i], pos );
                    
                    if(!ms.buttons[PEN_MOUSE_L])
                    {
                        selected[i] = false;
                        f32 dd = maths::magnitude(cp - pos);
                        if( dd < rd + rd * 0.05 &&
                           dd > rd - rd * 0.05 )
                            selected[i] = true;
                    }
                    
                    vec3f col = plane_normals[i] * 0.7;
                    if( selected[i] )
                    {
                        _cp = cp;
                        col = vec3f::one();
                    }
                    
                    dbg::add_circle(plane_normals[i], pos, rd, vec4f( col, 1.0));
                }
                
                static vec3f attach_point = vec3f::zero();
                for( s32 i = 0; i < 3; ++i )
                {
                    if(!ms.buttons[PEN_MOUSE_L])
                    {
                        attach_point = _cp;
                        continue;
                    }
                    
                    if( selected[i] )
                    {
						k_select_flags |= WIDGET_SELECTED;

                        vec3f prev_line = maths::normalise(attach_point - pos);
                        vec3f cur_line = maths::normalise(_cp - pos);
                        
                        dbg::add_line(pos, attach_point, vec4f::cyan());
                        dbg::add_line(pos, _cp, vec4f::magenta());
                        
                        vec3f x = maths::cross(prev_line, cur_line);
                        f32 amt = maths::dot( x, plane_normals[i] );
                        
                        apply_transform_to_selection( view.scene, plane_normals[i] * amt);
                        
                        attach_point = _cp;
                        break;
                    }
                }
                
                return;
            }

			if (k_transform_mode == TRANSFORM_TRANSLATE || k_transform_mode == TRANSFORM_SCALE)
			{
				static vec3f unit_axis[] =
				{
					vec3f::zero(),
					vec3f::unit_x(),
					vec3f::unit_y(),
					vec3f::unit_z(),
				};

				//work out major axes
				vec3f pp[4];
				for (s32 i = 0; i < 4; ++i)
				{
					widget_points[i] = pos + unit_axis[i] * d;

					pp[i] = put::maths::project(widget_points[i], view.camera->view, view.camera->proj, vpi);
					pp[i].z = 0.0f;
				}

				//work out joint axes
				vec3f ppj[6];
				for (s32 i = 0; i < 3; ++i)
				{
					u32 j_index = i * 2;

					u32 next_index = i + 2;
					if (next_index > 3)
						next_index = 1;

					ppj[j_index] = put::maths::project(pos + unit_axis[i+1] * d * 0.3f, view.camera->view, view.camera->proj, vpi);
					ppj[j_index].z = 0.0f;

					ppj[j_index + 1] = put::maths::project(pos + unit_axis[next_index] * d * 0.3, view.camera->view, view.camera->proj, vpi);
					ppj[j_index + 1].z = 0.0f;
				}

				if (!ms.buttons[PEN_MOUSE_L])
				{
					selected_axis = 0;
					for (s32 i = 1; i < 4; ++i)
					{
						vec3f cp = put::maths::closest_point_on_line(pp[0], pp[i], mousev3);

						if (put::maths::distance(cp, mousev3) < selection_radius)
							selected_axis |= (1 << i);
					}

					for (s32 i = 0; i < 3; ++i)
					{
						u32 j_index = i * 2;
						u32 i_next = i + 2;
						u32 ii = i + 1;

						if (i_next > 3)
							i_next = 1;

						vec3f cp = put::maths::closest_point_on_line(ppj[j_index], ppj[j_index + 1], mousev3);

						if (put::maths::distance(cp, mousev3) < selection_radius)
							selected_axis |= (1<< ii) | (1<<i_next);
					}
				}

				//draw axes
				for (s32 i = 1; i < 4; ++i)
				{
					vec4f col = vec4f(unit_axis[i] * 0.7f, 1.0f);

					if (selected_axis & (1 << i))
					{
						k_select_flags |= WIDGET_SELECTED;
						col = vec4f::one();
					}

					put::dbg::add_line_2f(pp[0].xy(), pp[i].xy(), col);

					if (k_transform_mode == TRANSFORM_TRANSLATE)
					{
						vec2f v = put::maths::normalise(pp[i].xy() - pp[0].xy());
						vec2f perp = put::maths::perp(v, LEFT_HAND) * 5.0;

						vec2f base = pp[i].xy() - v * 5.0;

						put::dbg::add_line_2f(pp[i].xy(), base + perp, col);
						put::dbg::add_line_2f(pp[i].xy(), base - perp, col);
					}
					else if (k_transform_mode == TRANSFORM_SCALE)
					{
						put::dbg::add_quad_2f(pp[i].xy(), vec2f(3.0f, 3.0f), col);
					}
				}

				//draw joins
				for (s32 i = 0; i < 3; ++i)
				{
					u32 j_index = i * 2;
					u32 i_next = i + 2;
					if (i_next > 3)
						i_next = 1;

					u32 ii = i + 1;

					vec4f col = vec4f(0.2f, 0.2f, 0.2f, 1.0f);

					if((selected_axis & (1<<ii)) && (selected_axis & (1 << i_next)))
						col = vec4f::one();

					put::dbg::add_line_2f(ppj[j_index].xy(), ppj[j_index+1].xy(), col);
				}

				//project mouse to planes
				static vec3f translation_axis[] =
				{
					vec3f::unit_x(),
					vec3f::unit_y(),
					vec3f::unit_z(),
				};

				vec3f axis_pos[3];
				vec3f move_axis = vec3f::zero();
                
                vec3f restrict_axis = vec3f::zero();
                for (s32 i = 0; i < 3; ++i)
                    if ((selected_axis & 1 << (i + 1)))
                        restrict_axis += translation_axis[i];
                                
				for (s32 i = 0; i < 3; ++i)
				{
					if (!(selected_axis & 1 << (i + 1)))
						continue;

					static vec3f box_size = vec3f(0.5, 0.5, 0.5);
                    
					vec3f plane_normal = maths::cross(translation_axis[i], view.camera->view.get_up());

					if (i == 1)
						plane_normal = maths::cross(translation_axis[i], view.camera->view.get_right());

					axis_pos[i] = put::maths::ray_vs_plane(vr, r0, plane_normal, widget_points[0]);

					if (!ms.buttons[PEN_MOUSE_L])
						pre_click_axis_pos[i] = axis_pos[i];

					vec3f line = (axis_pos[i] - pre_click_axis_pos[i]);

					vec3f line_x = line * restrict_axis;

					move_axis += line_x;
                    
                    //only move in one plane at a time
                    break;
				}

                apply_transform_to_selection( view.scene, move_axis );
				
				for (s32 i = 0; i < 3; ++i)
				{
					pre_click_axis_pos[i] = axis_pos[i];
				}
			}
		}
        
        void render_scene_editor( const scene_view& view )
        {            
            vec2i vpi = vec2i( view.viewport->width, view.viewport->height );
            
            entity_scene* scene = view.scene;

			dbg::add_frustum(view.camera->camera_frustum.corners[0], view.camera->camera_frustum.corners[1]);
            
            if( scene->view_flags & DD_LIGHTS )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    if( scene->entities[n] & CMP_LIGHT)
                    {
                        vec3f p = scene->world_matrices[n].get_translation();
                        
                        p = put::maths::project(p, view.camera->view,  view.camera->proj, vpi);
                        
						if( p.z > 0.0f )
							put::dbg::add_quad_2f( p.xy(), vec2f( 3.0f, 3.0f ), vec4f( scene->lights[n].colour, 1.0f ) );
                    }
                }
            }
            
            if( scene->view_flags & DD_MATRIX )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    put::dbg::add_coord_space(scene->world_matrices[n], 0.5f);
                }
            }
            
            if( scene->view_flags & DD_AABB )
            {
                for (u32 n = 0; n < scene->num_nodes; ++n)
                {
                    put::dbg::add_aabb( scene->bounding_volumes[n].transformed_min_extents, scene->bounding_volumes[n].transformed_max_extents );
                }
            }
            
            if( scene->view_flags & DD_NODE )
            {
                for( auto& s : k_selection_list )
                {
                    put::dbg::add_aabb( scene->bounding_volumes[s].transformed_min_extents, scene->bounding_volumes[s].transformed_max_extents );
                }
            }
            
            if( scene->view_flags & DD_BONES )
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
                        
                        put::dbg::add_line(p1, p2, vec4f::one() );
                    }
                }
            }
            
            if( scene->view_flags & DD_GRID )
            {
				f32 divisions = k_model_view_controller.grid_size / k_model_view_controller.grid_cell_size;
                put::dbg::add_grid(vec3f::zero(), vec3f(k_model_view_controller.grid_size), divisions);
            }

            put::dbg::render_3d(view.cb_view);
            
            //no depth test
            static u32 depth_disabled = pmfx::get_render_state_by_name(PEN_HASH("disabled_depth_stencil_state"));
            
            pen::renderer_set_depth_stencil_state(depth_disabled);
            
			transform_widget( view );

            put::dbg::render_3d(view.cb_view);
            
            put::dbg::render_2d( view.cb_2d_view );
            
            //reset depth state
            pen::renderer_set_depth_stencil_state(view.depth_stencil_state);
        }
    }
}
