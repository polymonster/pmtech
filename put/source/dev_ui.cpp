#include "pen.h"
#include "dev_ui.h"
#include "file_system.h"
#include "json.hpp"
#include <string>
#include <fstream>

using json = nlohmann::json;

extern pen::window_creation_params pen_window;

namespace put
{
    static json k_program_preferences;
    static std::string k_program_prefs_filename;
    
    void load_program_preferences()
    {
        k_program_prefs_filename = pen_window.window_title;
        k_program_prefs_filename += "_prefs.json";
        
        std::ifstream ifs(k_program_prefs_filename);
        
        if( ifs )
        {
            k_program_preferences = json::parse(ifs);
            ifs.close();
        }
    }
    
    void set_last_ued_directory( std::string& dir )
    {
        //make a copy of the string to format
        std::string formatted = dir;
        
        //always use / for consistency
        std::replace( formatted.begin(), formatted.end(), '\\', '/' );
        
        //strip the file
        s32 last_dir = formatted.rfind("/");
        s32 ext = formatted.rfind(".");
        
        if( last_dir < ext )
        {
            formatted = formatted.substr( 0, last_dir );
        }
        
        k_program_preferences["last_used_directory"] = formatted;
        
        std::ofstream ofs(k_program_prefs_filename);
        
        ofs << k_program_preferences.dump();
        
        ofs.close();
    }
    
    const c8** get_last_used_directory( s32& directory_depth )
    {
        static const s32 max_directory_depth = 32;
        static c8* directories[ max_directory_depth ];
        
        if( k_program_preferences.type() != json::value_t::null )
        {
            json last_dir = k_program_preferences["last_used_directory"];
            
            if( last_dir.type() != json::value_t::null )
            {
                std::string path = last_dir;
                
                s32 dir_pos = 0;
                directory_depth = 0;
                bool finished = false;
                while( !finished )
                {
                    s32 prev_pos = dir_pos;
                    
                    dir_pos = path.find( "/", prev_pos );
                    
                    if( dir_pos != std::string::npos )
                    {
                        dir_pos += 1;
                    }
                    else
                    {
                        dir_pos = path.length();
                        finished = true;
                    }
                        
                    if( dir_pos - prev_pos > 0 )
                    {
                        if( directories[directory_depth] )
                            pen::memory_free(directories[directory_depth]);
                        
                        directories[directory_depth] = (c8*)pen::memory_alloc( (dir_pos - prev_pos) + 1 );
                        
                        s32 j = 0;
                        for( s32 i = prev_pos; i < dir_pos; ++i )
                        {
                            //first dir on osx can be '/'
                            if( directory_depth > 0 && path[i] == '/' )
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
    
    const c8* file_browser( bool& dialog_open, s32 num_filetypes, ...  )
    {
        static bool initialise = true;
        static s32 current_depth = 1;
        static s32 selection_stack[128] = { -1 };
        
        std::string current_path;
        std::string search_path;
        static std::string selected_path;
        
        static pen::fs_tree_node fs_enumeration;
        
        if( initialise )
        {
            load_program_preferences();
            
            s32 default_depth = 0;
            const c8** default_dir = get_last_used_directory(default_depth);
            
            pen::filesystem_enum_volumes(fs_enumeration);
            
            pen::fs_tree_node* fs_iter = &fs_enumeration;
            
            for( s32 c = 0; c < default_depth; ++c )
            {
                for( u32 entry = 0; entry < fs_iter->num_children; ++entry )
                {
                    if( pen::string_compare( fs_iter->children[entry].name, default_dir[c] ) == 0 )
                    {
                        current_path += fs_iter->children[ entry ].name;
                        current_path += "/";
                        
                        va_list wildcards;
                        va_start( wildcards, num_filetypes );
                        
                        pen::filesystem_enum_directory( current_path.c_str(), fs_iter->children[ entry ], num_filetypes, wildcards );
                        
                        va_end(wildcards);
                        
                        selection_stack[c] = entry;
                        
                        fs_iter = &fs_iter->children[ entry ];
                        
                        current_depth = c + 2;
                        
                        break;
                    }
                }
            }
            
            initialise = false;
        }
        
        ImGui::Begin("File Browser");
        
        ImGui::Text("%s", selected_path.c_str());
        
        const c8* return_value = nullptr;
        
        ImGuiButtonFlags button_flags = 0;
        if( selected_path == "" )
        {
            button_flags |= ImGuiButtonFlags_Disabled;
        }
        
        if( ImGui::ButtonEx("OK", ImVec2(0,0), button_flags ) )
        {
            return_value = selected_path.c_str();
            set_last_ued_directory(selected_path);
            dialog_open = false;
        }
        
        ImGui::SameLine();
        if( ImGui::Button("Cancel") )
        {
            dialog_open = false;
        }
        
        ImGui::BeginChild("scrolling", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        
        ImGui::Columns(current_depth, "directories");
        ImGui::Separator();
        pen::fs_tree_node* fs_iter = &fs_enumeration;
        
        s32 frame_depth = current_depth;
        for( s32 d = 0; d < frame_depth; ++d )
        {
            ImGui::Text("%s", fs_iter->name); ImGui::NextColumn();
            fs_iter = &fs_iter->children[ selection_stack[ d ] ];
        }
        
        ImGui::Separator();
        
        current_path = "";
        search_path = "";
        
        fs_iter = &fs_enumeration;
        
        for( s32 c = 0; c < frame_depth; ++c )
        {
            for( u32 entry = 0; entry < fs_iter->num_children; ++entry )
            {
                if( ImGui::Selectable( fs_iter->children[entry].name) )
                {
                    search_path = current_path;
                    search_path += fs_iter->children[entry].name;
                    
                    va_list wildcards;
                    va_start( wildcards, num_filetypes );
                    
                    pen::filesystem_enum_directory( search_path.c_str(), fs_iter->children[entry], num_filetypes, wildcards );
                    
                    va_end(wildcards);
                    
                    if( fs_iter->children[entry].num_children > 0 )
                    {
                        for( s32 i = c; i < current_depth; ++i )
                        {
                            selection_stack[i] = -1;
                        }
                        
                        current_depth = c + 2;
                        selection_stack[c] = entry;
                    }
                    else
                    {
                        selected_path = "";
                        selected_path = search_path;
                    }
                }
            }
            
            s32 selected = selection_stack[c];
            
            if (selected >= 0)
            {
                fs_iter = &fs_iter->children[selection_stack[c]];
                current_path += fs_iter->name;
                
                if(current_path != "/")
                    current_path += "/";
            }
            else
            {
                break;
            }
            
            ImGui::NextColumn();
        }
        
        
        
        ImGui::EndChild();
        ImGui::Columns(1);
        
        ImGui::End();
        
        if( !dialog_open )
        {
            initialise = true;
            filesystem_enum_free_mem(fs_enumeration);
        }
        
        return return_value;
    }
}
