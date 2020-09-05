// file_system.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include <dirent.h>
#include <fnmatch.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

#include "file_system.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_string.h"

#if PEN_PLATFORM_LINUX
#define NO_MOUNT_POINTS
#define get_mtime(s) s.st_mtime
#define HOME_DIR "home"
#elif PEN_PLATFORM_WEB
#define NO_MOUNT_POINTS
#define get_mtime(s) 0
#define HOME_DIR "home"
#else
#define HOME_DIR "Users"
#define get_mtime(s) s.st_mtimespec.tv_sec
#endif

#define ENABLE_WRITE_FILE_DEPENDENCIES 0
#if ENABLE_WRITE_FILE_DEPENDENCIES
#define WRITE_FILE_DEPENDENCIES(fn) write_file_dependency(fn)
#else
#define WRITE_FILE_DEPENDENCIES(fn)
#endif

namespace
{
    // utility function to output file dependencies use by a pmtech app to trim data sizes in wasm .data bundles
    void write_file_dependency(const c8* filename)
    {
        Str fn = "";
        fn.append(pen::window_get_title());
        fn.append("_data.txt");
        static bool s_first = true;
        if(s_first)
        {
            if(pen::filesystem_file_exists(fn.c_str()))
                remove(fn.c_str());
                        
            s_first = false;
        }
        FILE* p_file = fopen(fn.c_str(), "a");
        fwrite(filename, strlen(filename), 1, p_file);
        fwrite("\n", 1, 1, p_file);
        fclose(p_file);
    }
}

namespace pen
{
    bool filesystem_file_exists(const c8* filename)
    {
        return access(filename, F_OK );
    }
    
    pen_error filesystem_read_file_to_buffer(const c8* filename, void** p_buffer, u32& buffer_size)
    {
        WRITE_FILE_DEPENDENCIES(filename);
        
        const c8* resource_name = os_path_for_resource(filename);

        *p_buffer = NULL;

        FILE* p_file = fopen(resource_name, "rb");

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
        static const c8* volumes_name = "Volumes";

        u32 len = pen::string_length(volumes_name);
        results.name = (c8*)pen::memory_alloc(len + 1);
        memcpy(results.name, volumes_name, len);
        results.name[len] = '\0';

#ifdef NO_MOUNT_POINTS
        // Stick them at "/" instead
        results.children = (fs_tree_node*)pen::memory_alloc(sizeof(fs_tree_node));
        results.num_children = 1;

        results.children[0].name = (c8*)pen::memory_alloc(2);
        results.children[0].name[0] = '/';
        results.children[0].name[1] = '\0';
        results.children[0].children = nullptr;
        results.children[0].num_children = 0;
#else
        struct statfs* mounts;
        s32            num_mounts = getmntinfo(&mounts, MNT_WAIT);

        results.children = (fs_tree_node*)pen::memory_alloc(sizeof(fs_tree_node) * num_mounts);
        results.num_children = num_mounts;

        for (s32 i = 0; i < num_mounts; ++i)
        {
            len = pen::string_length(mounts[i].f_mntonname);
            results.children[i].name = (c8*)pen::memory_alloc(len + 1);

            memcpy(results.children[i].name, mounts[i].f_mntonname, len);
            results.children[i].name[len] = '\0';

            results.children[i].children = nullptr;
            results.children[i].num_children = 0;
        }
#endif
        return PEN_ERR_OK;
    }

    static bool s_show_hidden = false;
    void        filesystem_toggle_hidden_files()
    {
        s_show_hidden = !s_show_hidden;
    }

    bool match_file(struct dirent* ent, s32 num_wildcards, va_list wildcards)
    {
        if (!s_show_hidden)
        {
            if (ent->d_name[0] == '.')
            {
                return false;
            }
        }

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
                const c8* ft = va_arg(wildcards_consume, const c8*);

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
        struct stat stat_res;

        stat(filename, &stat_res);

        mtime_out = get_mtime(stat_res);

        return PEN_ERR_OK;
    }

    const c8* filesystem_get_user_directory()
    {
        static c8 default_dir[1024];
        pen::string_format(default_dir, 1024, "/%s/%s/", HOME_DIR, os_get_user_info().user_name);
        return &default_dir[0];
    }

    const c8** filesystem_get_user_directory(s32& directory_depth)
    {
        // returns array of dirs
        static const u32 max_dir_depth = 3;

        static c8 default_dir[max_dir_depth][1024];

        static c8* dir_list[max_dir_depth];

        pen::string_format(default_dir[0], 1024, "/");
        pen::string_format(default_dir[1], 1024, HOME_DIR);
        pen::string_format(default_dir[2], 1024, "%s", os_get_user_info().user_name);

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


