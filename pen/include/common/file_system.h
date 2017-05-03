#ifndef _file_system_h
#define _file_system_h

#include "definitions.h"

namespace pen
{
    struct fs_tree_node
    {
        c8* name = nullptr;
        fs_tree_node* children = nullptr;
        u32 num_children = 0;
    };

	//read file, check file info
    pen_error filesystem_read_file_to_buffer( const c8* filename, void** p_buffer, u32 &buffer_size );
	pen_error filesystem_getmtime(const c8* filename, u32& mtime_out);

	//filesystem enumeration
    pen_error filesystem_enum_volumes( fs_tree_node &results );
    
    pen_error filesystem_enum_directory( const c8* directory, fs_tree_node &results, s32 num_wildcards = 0, ... );
    pen_error filesystem_enum_directory( const c8* directory, fs_tree_node &results, s32 num_wildcards, va_list args );
    
    pen_error filesystem_enum_free_mem( fs_tree_node &results );
    
    //utilities
    const c8** filesystem_get_user_directory( s32& directory_depth );
}

#endif
