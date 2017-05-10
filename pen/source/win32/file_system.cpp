#include "file_system.h"
#include "memory.h"
#include <stdio.h>
#include "pen_string.h"
#include <Shlwapi.h>
#include <Shlobj.h>

namespace pen
{
#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

	u32 win32_time_to_unix_seconds(long long ticks)
	{
		return (u32)(ticks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
	}

	pen_error filesystem_getmtime(const c8* filename, u32& mtime_out)
	{
		OFSTRUCT of_struct;
		HFILE f = OpenFile(filename, &of_struct, OF_READ);

		if (f != HFILE_ERROR)
		{
			FILETIME c, m, a;

			BOOL res = GetFileTime((HANDLE)f, &c, &a, &m);

			long long* wt = (long long*)&m;

			u32 unix_ts = win32_time_to_unix_seconds(*wt);

			mtime_out = unix_ts;

			return PEN_ERR_OK;
		}

		return PEN_ERR_FILE_NOT_FOUND;
	}

	c8* swap_slashes( const c8* filename )
	{
		//swap "/" for "\\"
		const char* p_src_char = filename;

		u32 str_len = pen::string_length(filename);
		char* windir_filename = (char*)pen::memory_alloc(str_len + 1);

		char* p_dest_char = windir_filename;

		while (p_src_char < filename + str_len)
		{
			if (*p_src_char == '/')
			{
				*p_dest_char = '\\';
			}
			else
			{
				*p_dest_char = *p_src_char;
			}

			p_dest_char++;
			p_src_char++;
		}
		*p_dest_char = '\0';

		return windir_filename;
	}

	pen_error filesystem_read_file_to_buffer( const c8* filename, void** p_buffer, u32 &buffer_size )
    {
		c8* windir_filename = swap_slashes(filename);

        *p_buffer = NULL;

        FILE* p_file = nullptr;
        fopen_s( &p_file, windir_filename, "rb" );

        pen::memory_free(windir_filename);

        if( p_file )
        {
            fseek( p_file, 0L, SEEK_END );
            LONG size = ftell( p_file );

            fseek( p_file, 0L, SEEK_SET );

            buffer_size = ( u32 ) size;

            *p_buffer = pen::memory_alloc( buffer_size );

            fread( *p_buffer, 1, buffer_size, p_file );

            fclose( p_file );

            return PEN_ERR_OK;
        }

        return PEN_ERR_FILE_NOT_FOUND;
    }

	pen_error filesystem_enum_volumes(fs_tree_node &tree)
	{
		DWORD drive_bit_mask = GetLogicalDrives();

		if (drive_bit_mask == 0)
		{
			return PEN_ERR_FAILED;
		}

		const c8* volumes_str = "Volumes";
		u32 volume_strlen = pen::string_length(volumes_str);

		tree.name = (c8*)pen::memory_alloc(volume_strlen + 1);
		pen::memory_cpy(tree.name, volumes_str, volume_strlen);
		tree.name[volume_strlen] = '\0';

		const c8* drive_letters = { "ABCDEFGHIJKLMNOPQRSTUVWXYZ" };
		
		u32 num_drives = pen::string_length(drive_letters);

		u32 num_used_drives = 0;
		u32 bit = 1;
		for ( u32 i = 0; i < num_drives; ++i )
		{
			if (drive_bit_mask & bit)
			{
				++num_used_drives;
			}

			bit <<= 1;
		}

		tree.num_children = num_used_drives;
		tree.children = (fs_tree_node*)pen::memory_alloc(sizeof(fs_tree_node)*num_used_drives);

		bit = 1;
		u32 child = 0;
		for (u32 i = 0; i < num_drives; ++i)
		{
			if (drive_bit_mask & bit)
			{
				tree.children[child].name = (c8*)pen::memory_alloc(3);

				tree.children[child].name[0] = drive_letters[i];
				tree.children[child].name[1] = ':';
				tree.children[child].name[2] = '\0';

				tree.children[child].num_children = 0;
				tree.children[child].children = nullptr;

				++child;
			}

			bit <<= 1;
		}

		return PEN_ERR_OK;
	}

	bool match_file(const WIN32_FIND_DATAA ffd, s32 num_wildcards, va_list wildcards)
	{
		if (num_wildcards <= 0)
		{
			return true;
		}

		va_list wildcards_consume;
		va_copy(wildcards_consume, wildcards);

		if ( !(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) )
		{
			for (s32 i = 0; i < num_wildcards; ++i)
			{
				const char* ft = va_arg(wildcards_consume, const char*);

				if ( PathMatchSpecExA(ffd.cFileName, ft, PMSF_NORMAL ) == S_OK )
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


	pen_error filesystem_enum_directory(const c8* directory, fs_tree_node &results, s32 num_wildcards, ...)
	{
		va_list filetypes;

		va_start(filetypes, num_wildcards);

		pen_error result = filesystem_enum_directory( directory, results, num_wildcards, filetypes);

		va_end(filetypes);

		return result;
	}

	pen_error filesystem_enum_directory(const c8* directory, fs_tree_node &tree, s32 num_wildcards, va_list args )
	{
		WIN32_FIND_DATAA ffd;
		HANDLE hFind = INVALID_HANDLE_VALUE;

		//need to make add a wildcard to the dirctory
		u32 dir_len = pen::string_length(directory);
		static c8 wildcard_dir[1024];
		pen::memory_cpy(wildcard_dir, directory, dir_len);
		wildcard_dir[dir_len] = '\\';
		wildcard_dir[dir_len + 1] = '*';
		wildcard_dir[dir_len + 2] = '\0';

		c8* windir_filename = swap_slashes(wildcard_dir);

		u32 total_num = 0;
		hFind = FindFirstFileA(windir_filename, &ffd );
		do
		{
			if (hFind != INVALID_HANDLE_VALUE)
			{
				if (match_file(ffd, num_wildcards, args))
				{
					++total_num;
				}
			}

		} while (FindNextFileA(hFind, &ffd) != 0);

		//allocate children into the fs tree structure
		tree.num_children = total_num;

		if (tree.num_children == 0)
		{
			tree.children = nullptr;
			return PEN_ERR_FILE_NOT_FOUND;
		}

		tree.children = (fs_tree_node*)pen::memory_alloc( sizeof(fs_tree_node) * total_num);

		//reiterate and get the file names
		u32 child = 0;
		hFind = FindFirstFileA(windir_filename, &ffd);
		do
		{
			if (hFind != INVALID_HANDLE_VALUE)
			{
				if (match_file(ffd, num_wildcards, args))
				{
					u32 file_name_len = pen::string_length(ffd.cFileName);

					tree.children[child].name = (c8*)pen::memory_alloc(file_name_len + 1);
					pen::memory_cpy(tree.children[child].name, ffd.cFileName, file_name_len);
					tree.children[child].name[file_name_len] = '\0';

					tree.children[child].num_children = 0;
					tree.children[child].children = nullptr;

					++child;
				}
			}
		} while (FindNextFileA(hFind, &ffd) != 0);

		pen::memory_free(windir_filename);

		return PEN_ERR_OK;
	}

	pen_error filesystem_enum_free_mem(fs_tree_node &tree)
	{
		for ( u32 i = 0; i < tree.num_children; ++i )
		{
			filesystem_enum_free_mem(tree.children[i]);
		}

		pen::memory_free(tree.children);
		pen::memory_free(tree.name);

		return PEN_ERR_OK;
	}

	const c8** filesystem_get_user_directory(s32& directory_depth)
	{
		const u32 max_depth = 4;
		static c8 dirs[max_depth][MAX_PATH];
		
		static c8* dir_list[max_depth];

		c8 path[MAX_PATH];
		if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, path))) 
		{
			u32 len = pen::string_length(path);

			u32 cur_pos = 0;
			u32 cur_depth = 0;

			for (u32 i = 0; i < len; ++i)
			{
				bool last_char = i == len - 1;

				if (path[i] == '\\' || last_char)
				{
					u32 sub_dir_len = i - cur_pos;

					if (last_char)
						sub_dir_len++;

					pen::memory_cpy(dirs[cur_depth], &path[cur_pos], sub_dir_len);

					dirs[cur_depth][sub_dir_len] = '\0';

					cur_depth++;
					cur_pos = i + 1;
				}
			}

			directory_depth = cur_depth;

			for (s32 i = 0; i < directory_depth; ++i)
			{
				dir_list[i] = &dirs[i][0];
			}

			return (const c8**)&dir_list[0];
		}

		return nullptr;
	}

	s32 filesystem_exclude_slash_depth()
	{
		//all windows directories when listed can exclude slashes
		return -1;
	}

}