#include <dirent.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>

#include "file_system.h"
#include "memory.h"
#include "pen.h"
#include "pen_string.h"

extern pen::user_info pen_user_info;

#ifdef __linux__
#define get_mtime(s) s.st_mtime
#else
#define get_mtime(s) s.st_mtimespec.tv_sec
#endif

namespace pen
{
    pen_error filesystem_read_file_to_buffer(const char* filename, void** p_buffer, u32& buffer_size)
    {
        *p_buffer = NULL;

        FILE* p_file = fopen(filename, "rb");

        if (p_file)
        {
            fseek(p_file, 0L, SEEK_END);
            long size = ftell(p_file);

            fseek(p_file, 0L, SEEK_SET);

            buffer_size = (u32)size;

            *p_buffer = pen::memory_alloc(buffer_size + 1);

            fread(*p_buffer, 1, buffer_size, p_file);

            ((u8*)*p_buffer)[buffer_size] = '\0';

            fclose(p_file);

            return PEN_ERR_OK;
        }

        return PEN_ERR_FILE_NOT_FOUND;
    }

    pen_error filesystem_enum_volumes(fs_tree_node& results)
    {
#ifndef __linux__
        struct statfs* mounts;
        int num_mounts = getmntinfo(&mounts, MNT_WAIT);

        results.children = (fs_tree_node*)pen::memory_alloc(sizeof(fs_tree_node) * num_mounts);
        results.num_children = num_mounts;

        static const c8* volumes_name = "Volumes";

        u32 len = pen::string_length(volumes_name);
        results.name = (c8*)pen::memory_alloc(len + 1);
        pen::memory_cpy(results.name, volumes_name, len);
        results.name[len] = '\0';

        for (int i = 0; i < num_mounts; ++i)
        {
            len = pen::string_length(mounts[i].f_mntonname);
            results.children[i].name = (c8*)pen::memory_alloc(len + 1);

            pen::memory_cpy(results.children[i].name, mounts[i].f_mntonname, len);
            results.children[i].name[len] = '\0';

            results.children[i].children = nullptr;
            results.children[i].num_children = 0;
        }
#endif
        return PEN_ERR_OK;
    }

    bool match_file(struct dirent* ent, s32 num_wildcards, va_list wildcards)
    {
        if (num_wildcards <= 0)
        {
            return true;
        }

        va_list wildcards_consume;
        va_copy(wildcards_consume, wildcards);

        if (ent->d_type != DT_DIR)
        {
            for (s32 i = 0; i < num_wildcards; ++i)
            {
                const char* ft = va_arg(wildcards_consume, const char*);

                if (fnmatch(ft, ent->d_name, 0) == 0)
                {
                    return true;
                }
            }
        }
        else
        {
            return true;
        }

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
        DIR* dir;
        struct dirent* ent;

        u32 num_items = 0;
        if ((dir = opendir(directory)) != NULL)
        {
            while ((ent = readdir(dir)) != NULL)
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
        if ((dir = opendir(directory)) != NULL)
        {
            while ((ent = readdir(dir)) != NULL)
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

                    pen::memory_cpy(results.children[i].name, ent->d_name, len);
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
        struct stat stat_res;

        stat(filename, &stat_res);

        mtime_out = get_mtime(stat_res);

        return PEN_ERR_OK;
    }

    const c8** filesystem_get_user_directory(s32& directory_depth)
    {
        static const u32 max_dir_depth = 3;

        static c8 default_dir[max_dir_depth][1024];

        static c8* dir_list[max_dir_depth];

        pen::string_format(default_dir[0], 1024, "/");
        pen::string_format(default_dir[1], 1024, "Users");
        pen::string_format(default_dir[2], 1024, "%s", pen_user_info.user_name);

        for (s32 i = 0; i < max_dir_depth; ++i)
        {
            dir_list[i] = &default_dir[i][0];
        }

        directory_depth = max_dir_depth;

        return (const c8**)&dir_list[0];
    }

    s32 filesystem_exclude_slash_depth()
    {
        // directory depth 0 can be a slash
        return 0;
    }
} // namespace pen
