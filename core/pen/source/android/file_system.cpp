// file_system.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "file_system.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_string.h"

namespace pen
{
    bool filesystem_file_exists(const c8* filename)
    {
        return false;
    }

    pen_error filesystem_read_file_to_buffer(const c8* filename, void** p_buffer, u32& buffer_size)
    {
        return PEN_ERR_FILE_NOT_FOUND;
    }

    pen_error filesystem_enum_volumes(fs_tree_node& results)
    {
        return PEN_ERR_OK;
    }

    void filesystem_toggle_hidden_files()
    {

    }

    bool match_file(struct dirent* ent, s32 num_wildcards, va_list wildcards)
    {
        return false;
    }

    pen_error filesystem_enum_directory(const c8* directory, fs_tree_node& results, s32 num_wildcards, ...)
    {
        return PEN_ERR_OK;
    }

    pen_error filesystem_enum_directory(const c8* directory, fs_tree_node& results, s32 num_wildcards, va_list wildcards)
    {
        return PEN_ERR_OK;
    }

    pen_error filesystem_enum_free_mem(fs_tree_node& tree)
    {
        return PEN_ERR_OK;
    }

    pen_error filesystem_getmtime(const c8* filename, u32& mtime_out)
    {
        return PEN_ERR_OK;
    }

    size_t filesystem_getsize(const c8* filename)
    {
        return 0;
    }

    const c8* filesystem_get_user_directory()
    {
        return nullptr;
    }

    const c8** filesystem_get_user_directory(s32& directory_depth)
    {
        return nullptr;
    }

    s32 filesystem_exclude_slash_depth()
    {
        // directory depth 0 can be a slash
        return 0;
    }
} // namespace pen