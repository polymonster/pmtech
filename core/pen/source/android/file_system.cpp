// file_system.cpp
// Copyright 2014 - 2025 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "file_system.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_string.h"

#include <sys/stat.h>


namespace pen
{
    pen_error filesystem_enum_volumes(fs_tree_node& results)
    {
        return PEN_ERR_OK;
    }

    void filesystem_toggle_hidden_files()
    {
        // stub
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
        struct stat st;
        if (stat(filename, &st) == 0) {
            mtime_out = (u32)st.st_mtime; // modification time
            return PEN_ERR_OK;
        }

        return PEN_ERR_FILE_NOT_FOUND;
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