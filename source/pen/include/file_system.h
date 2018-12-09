#ifndef _file_system_h
#define _file_system_h

#include "pen.h"

namespace pen
{
    // Minimalist C-Style filesystem api for portability between windows and unix
    // Implemented in win32 and using the c file system and windir stat on unix
    // Can read files and also enumerate file system and volumes as an fs_tree_node

    // Make sure to free p_buffer yourself allocated from filesystem_read_file_to_buffer
    // Make sure to call filesystem_enum_free_mem with your fs_tree_node once finished with it.

    struct fs_tree_node
    {
        c8*           name = nullptr;
        fs_tree_node* children = nullptr;
        u32           num_children = 0;
    };

    pen_error  filesystem_read_file_to_buffer(const c8* filename, void** p_buffer, u32& buffer_size);
    pen_error  filesystem_getmtime(const c8* filename, u32& mtime_out);
    void       filesystem_toggle_hidden_files();
    pen_error  filesystem_enum_volumes(fs_tree_node& results);
    pen_error  filesystem_enum_directory(const c8* directory, fs_tree_node& results, s32 num_wildcards = 0, ...);
    pen_error  filesystem_enum_directory(const c8* directory, fs_tree_node& results, s32 num_wildcards, va_list args);
    pen_error  filesystem_enum_free_mem(fs_tree_node& results);
    const c8*  filesystem_get_user_directory(); // returns /Users/user.name (osx), /home/user.name (linux) etc
    const c8** filesystem_get_user_directory(s32& directory_depth); // returns array of directories like the above
    s32        filesystem_exclude_slash_depth();
} // namespace pen

#endif
