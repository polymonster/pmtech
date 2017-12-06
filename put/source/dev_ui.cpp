#include "pen.h"
#include "pen_string.h"
#include "memory.h"
#include "dev_ui.h"
#include "file_system.h"
#include "pen_json.h"
#include <string>
#include <fstream>

extern pen::window_creation_params pen_window;

namespace put
{
	namespace dev_ui
	{
        static pen::json k_program_preferences;
		static Str k_program_prefs_filename;

		void load_program_preferences()
		{
			k_program_prefs_filename = pen_window.window_title;
			k_program_prefs_filename.append("_prefs.json");

            k_program_preferences = pen::json::load_from_file(k_program_prefs_filename.c_str());
		}
        
        void save_program_preferences( )
        {
            std::ofstream ofs(k_program_prefs_filename.c_str());
            
            ofs << k_program_preferences.dumps().c_str();
            
            ofs.close();
        }

		void set_last_used_directory(Str& dir)
		{
			//make a copy of the string to format
			std::string formatted = dir.c_str();

			//always use / for consistency
			std::replace(formatted.begin(), formatted.end(), '\\', '/');
            std::replace( formatted.begin(), formatted.end(), ':', '@' );

			//strip the file
			s32 last_dir = formatted.rfind("/");
			s32 ext = formatted.rfind(".");

			if (last_dir < ext)
			{
				formatted = formatted.substr(0, last_dir);
			}

            formatted = "\"" + formatted + "\"";

            k_program_preferences.set("last_used_directory", formatted);

            save_program_preferences();
		}
        
        void util_init( )
        {
            load_program_preferences();
        }
        
        void set_program_preference( const c8* name, Str val )
        {
            k_program_preferences.set(name, val);
            save_program_preferences();
        }
        
        pen::json get_program_preference( const c8* name )
        {
            return k_program_preferences[name];
        }

		const c8** get_last_used_directory(s32& directory_depth)
		{
			static const s32 max_directory_depth = 32;
			static c8* directories[max_directory_depth];

			if (k_program_preferences.type() != JSMN_UNDEFINED)
			{
                pen::json last_dir = k_program_preferences["last_used_directory"];

				if (last_dir.type() != JSMN_UNDEFINED)
				{
					std::string path = last_dir.as_str().c_str();
                    std::replace( path.begin(), path.end(), '@', ':' );

					s32 dir_pos = 0;
					directory_depth = 0;
					bool finished = false;
					while (!finished)
					{
						s32 prev_pos = dir_pos;

						dir_pos = path.find("/", prev_pos);

						if (dir_pos != std::string::npos)
						{
							dir_pos += 1;
						}
						else
						{
							dir_pos = path.length();
							finished = true;
						}

						if (dir_pos - prev_pos > 0)
						{
							if (directories[directory_depth])
								pen::memory_free(directories[directory_depth]);

							directories[directory_depth] = (c8*)pen::memory_alloc((dir_pos - prev_pos) + 1);

							s32 j = 0;
							for (s32 i = prev_pos; i < dir_pos; ++i)
							{
								//first dir on unix can be '/' otherwise we just want the diretcory names with no slashes
								if (path[i] == '/' && directory_depth > pen::filesystem_exclude_slash_depth())
								{
									//otherwise exclude slashes from the dirname
									continue;
								}

								directories[directory_depth][j] = path[i];
								++j;
							}

							directories[directory_depth][j] = '\0';

							++directory_depth;
						}
					}

					return (const c8**)directories;
				}
			}

			s32 default_depth = 0;
			const c8** default_dir = pen::filesystem_get_user_directory(default_depth);

			directory_depth = default_depth;

			return default_dir;
		}

		const c8* file_browser(bool& dialog_open, u32 flags, s32 num_filetypes, ...)
		{
			static bool initialise = true;
			static s32 current_depth = 1;
			static s32 selection_stack[128] = { -1 };
            static c8 user_filename_buf[1024];
            
			Str current_path;
			Str search_path;
			static Str selected_path;
            static Str last_result;
            
			static pen::fs_tree_node fs_enumeration;

			if (initialise)
			{
				s32 default_depth = 0;
				const c8** default_dir = get_last_used_directory(default_depth);

				pen::filesystem_enum_volumes(fs_enumeration);

				pen::fs_tree_node* fs_iter = &fs_enumeration;

				for (s32 c = 0; c < default_depth; ++c)
				{
					for (u32 entry = 0; entry < fs_iter->num_children; ++entry)
					{
						if (pen::string_compare(fs_iter->children[entry].name, default_dir[c]) == 0)
						{
							current_path.append(fs_iter->children[entry].name);
							current_path.append("/");

							va_list wildcards;
							va_start(wildcards, num_filetypes);

							pen::filesystem_enum_directory(current_path.c_str(), fs_iter->children[entry], num_filetypes, wildcards);

							va_end(wildcards);

							selection_stack[c] = entry;

							fs_iter = &fs_iter->children[entry];

							current_depth = c + 2;

							break;
						}
					}
				}

				initialise = false;
			}

			if( ImGui::Begin("File Browser", &dialog_open) )
            {
                ImGui::Text("%s", selected_path.c_str());
                
                const c8* return_value = nullptr;
                
                ImGuiButtonFlags button_flags = 0;
                if (selected_path == "")
                {
                    button_flags |= ImGuiButtonFlags_Disabled;
                }
                
                if( flags & FB_SAVE )
                {
                    user_filename_buf[0] = '/';
                    ImGui::InputText("", &user_filename_buf[1], 1023);
                }
                else
                {
                    user_filename_buf[0] = '\0';
                }
                
                if (ImGui::ButtonEx("OK", ImVec2(0, 0), button_flags))
                {
                    selected_path.append(user_filename_buf);
                    
                    last_result = selected_path;
                    return_value = last_result.c_str();
                    
                    set_last_used_directory(selected_path);
                    
                    selected_path.clear();
                    
                    dialog_open = false;
                }
                
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    dialog_open = false;
                }
                
                ImGui::BeginChild("scrolling", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
                
                static s32 base_frame = current_depth - 4;
                if (ImGui::Button(ICON_FA_ARROW_LEFT))
                    base_frame--;
                ImGui::SameLine();
                if (ImGui::Button(ICON_FA_ARROW_RIGHT))
                    base_frame++;
                
                base_frame = std::min<s32>(current_depth-4,base_frame);
                base_frame = std::max<s32>(0,base_frame);
                
                static const s32 max_depth = 4;
                ImGui::Columns(std::min<s32>(max_depth, current_depth), "directories");
                
                ImGui::Separator();
                pen::fs_tree_node* fs_iter = &fs_enumeration;
                
                for (s32 d = 0; d < current_depth; ++d)
                {
                    if( d >= base_frame && d < base_frame+max_depth)
                    {
                        ImGui::Text("%s", fs_iter->name);
                        ImGui::NextColumn();
                    }
                    
                    fs_iter = &fs_iter->children[selection_stack[d]];
                }
                
                ImGui::Separator();
                
                current_path = "";
                search_path = "";
                
                fs_iter = &fs_enumeration;
                
                for (s32 c = 0; c < current_depth; ++c)
                {
                    if( c >= base_frame && c < base_frame+max_depth )
                    {
                        for (u32 entry = 0; entry < fs_iter->num_children; ++entry)
                        {
                            ImGui::PushID(fs_iter);
                            
                            if (ImGui::Selectable(fs_iter->children[entry].name))
                            {
                                search_path = current_path;
                                search_path.append(fs_iter->children[entry].name);
                                
                                va_list wildcards;
                                va_start(wildcards, num_filetypes);
                                
                                pen::filesystem_enum_directory(search_path.c_str(), fs_iter->children[entry], num_filetypes, wildcards);
                                
                                va_end(wildcards);
                                
                                if (fs_iter->children[entry].num_children > 0)
                                {
                                    for (s32 i = c; i < current_depth; ++i)
                                    {
                                        selection_stack[i] = -1;
                                    }
                                    
                                    current_depth = c + 2;
                                    selection_stack[c] = entry;
                                    
                                    selected_path = search_path;
                                }
                                else
                                {
                                    selected_path = "";
                                    selected_path = search_path;
                                }
                                
                                base_frame++;
                            }
                            
                            ImGui::PopID();
                        }
                        
                        if (selection_stack[c] >= 0)
                            ImGui::NextColumn();
                    }
                    
                    if (selection_stack[c] >= 0)
                    {
                        fs_iter = &fs_iter->children[selection_stack[c]];
                        current_path.append(fs_iter->name);
                        
                        if ( !(current_path.c_str()[0] == '/' && current_path.length() == 1) )
                            current_path.append("/");
                    }
                    else
                    {
                        break;
                    }
                }
                
                ImGui::EndChild();
                ImGui::Columns(1);
                
                ImGui::End();
                
                if (!dialog_open)
                {
                    initialise = true;
                    filesystem_enum_free_mem(fs_enumeration);
                }
                
                return return_value;
            }

            return nullptr;
		}
        
        bool state_button( const c8* text, bool state_active )
        {
            ImVec4 button_col = (&ImGui::GetStyle())->Colors[ImGuiCol_Button];
            
            if( state_active )
            {
                button_col.x *= 0.5f;
                button_col.y *= 0.5f;
                button_col.z *= 0.5f;
            }
            
            ImGui::PushStyleColor( ImGuiCol_Button, button_col );
            
            if (ImGui::Button(text))
            {
                ImGui::PopStyleColor();
                return true;
            }

            ImGui::PopStyleColor();
        
            return false;
        }

        void set_tooltip( const c8* fmt, ... )
        {
            static f32 k_tooltip_timer = 0.0f;
            static const f32 k_delay = 1.0f;
            static ImGuiID k_current = 0;
            
            f32 dt = ImGui::GetIO().DeltaTime;
            
            if(!ImGui::IsItemHovered())
            {
                return;
            }
            
            ImGuiID now = ImGui::GetID(fmt);
            if( k_current != now )
            {
                k_tooltip_timer = 0.0f;
                k_current = now;
            }
            
            k_tooltip_timer += dt;
            if( k_tooltip_timer > k_delay )
            {
                va_list args;
                va_start(args, fmt);
                ImGui::SetTooltipV(fmt, args);
                va_end(args);
            }
        }
	}
}
