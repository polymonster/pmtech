// file_system.cpp
// Copyright 2014 - 2025 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "file_system.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_string.h"

#include <sys/stat.h>
#include <dirent.h>

namespace pen
{
    pen_error filesystem_enum_volumes(fs_tree_node& results)
    {
        // stub
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
        va_list wc;
        va_start(wc, num_wildcards);

        pen_error res = filesystem_enum_directory(directory, results, num_wildcards, wc);

        va_end(wc);

        return res;
    }

    pen_error filesystem_enum_directory(const c8* directory, fs_tree_node& results, s32 num_wildcards, va_list wildcards)
    {
        DIR*           dir;
        struct dirent* ent;

        u32 num_items = 0;
        if ((dir = opendir(directory)) != nullptr)
        {
            while ((ent = readdir(dir)) != nullptr)
            {
                if (match_file(ent, num_wildcards, wildcards))
                {
                    num_items++;
                }
            }

            closedir(dir);
        }

        if (num_items == 0)
        {
            return PEN_ERR_FILE_NOT_FOUND;
        }

        if (results.children == nullptr)
        {
            // alloc new mem
            results.children = (fs_tree_node*)pen::memory_alloc(sizeof(fs_tree_node) * num_items);
            pen::memory_zero(results.children, sizeof(fs_tree_node) * num_items);
        }
        else
        {
            // grow buffer
            if (results.num_children < num_items)
            {
                results.children = (fs_tree_node*)pen::memory_realloc(results.children, sizeof(fs_tree_node) * num_items);
            }
        }

        results.num_children = num_items;

        u32 i = 0;
        if ((dir = opendir(directory)) != nullptr)
        {
            while ((ent = readdir(dir)) != nullptr)
            {
                if (match_file(ent, num_wildcards, wildcards))
                {
                    if (results.children[i].name == nullptr)
                    {
                        // allocate 1024 file buffer
                        results.children[i].name = (c8*)pen::memory_alloc(1024);
                        pen::memory_zero(results.children[i].name, 1024);
                    }

                    u32 len = pen::string_length(ent->d_name);
                    len = min<u32>(len, 1022);

                    memcpy(results.children[i].name, ent->d_name, len);
                    results.children[i].name[len] = '\0';

                    results.children[i].num_children = 0;

                    ++i;
                }
            }

            closedir(dir);
        }

        return PEN_ERR_OK;
    }

    pen_error filesystem_enum_free_mem(fs_tree_node& tree)
    {
        for (s32 i = 0; i < tree.num_children; ++i)
        {
            filesystem_enum_free_mem(tree.children[i]);
        }

        pen::memory_free(tree.children);
        pen::memory_free(tree.name);

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