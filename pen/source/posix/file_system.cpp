#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include "file_system.h"
#include "memory.h"
#include "pen_string.h"

namespace pen
{
    pen_error filesystem_read_file_to_buffer( const char* filename, void** p_buffer, u32 &buffer_size )
    {
        *p_buffer = NULL;

        FILE* p_file = fopen( filename, "rb" );

        if( p_file )
        {
            fseek( p_file, 0L, SEEK_END );
            long size = ftell( p_file );

            fseek( p_file, 0L, SEEK_SET );

            buffer_size = ( u32 ) size;

            *p_buffer = pen::memory_alloc( buffer_size );

            fread( *p_buffer, 1, buffer_size, p_file );

            fclose( p_file );

            return PEN_ERR_OK;
        }

        return PEN_ERR_FILE_NOT_FOUND;
    }
    
    pen_error filesystem_enum_volumes( fs_tree_node &results )
    {
        struct statfs* mounts;
        int num_mounts = getmntinfo(&mounts, MNT_WAIT);
        
        results.children = (fs_tree_node*)pen::memory_alloc( sizeof(fs_tree_node) * num_mounts );
        results.num_children = num_mounts;
        
        static const c8* volumes_name = "Volumes";
        
        u32 len = pen::string_length(volumes_name);
        results.name = (c8*)pen::memory_alloc( len + 1 );
        pen::memory_cpy(results.name, volumes_name, len);
        results.name[ len ] = '\0';
        
        for( int i = 0; i < num_mounts; ++i )
        {
            len = pen::string_length( mounts[i].f_mntonname );
            results.children[i].name = (c8*)pen::memory_alloc( len + 1 );
            
            pen::memory_cpy(results.children[i].name, mounts[i].f_mntonname, len);
            results.children[i].name[len] = '\0';
            
            results.children[i].children = nullptr;
            results.children[i].num_children = 0;
        }
        
        return PEN_ERR_OK;
    }
    
    pen_error filesystem_enum_directory( const c8* directory, fs_tree_node &results )
    {
        DIR *dir;
        struct dirent *ent;
        
        u32 num_items = 0;
        if ((dir = opendir (directory)) != NULL)
        {
            while ((ent = readdir (dir)) != NULL)
            {
                num_items++;
            }
            
            closedir (dir);
        }
        
        if( num_items == 0 )
        {
            return PEN_ERR_FILE_NOT_FOUND;
        }
        
        if( results.children == nullptr )
        {
            //alloc new mem
            results.children = (fs_tree_node*)pen::memory_alloc( sizeof(fs_tree_node) * num_items );
            pen::memory_zero(results.children, sizeof(fs_tree_node) * num_items );
        }
        else
        {
            //grow buffer
            if( results.num_children < num_items )
            {
                results.children = (fs_tree_node*)pen::memory_realloc( results.children, sizeof(fs_tree_node) * num_items );
            }
        }
        
        results.num_children = num_items;
        
        u32 i = 0;
        if ((dir = opendir (directory)) != NULL)
        {
            while ((ent = readdir (dir)) != NULL)
            {
                if( results.children[i].name == nullptr )
                {
                    //allocate 1024 file buffer
                    results.children[i].name = (c8*)pen::memory_alloc( 1024 );
                    pen::memory_zero(results.children[i].name, 1024);
                }
                
                u32 len = pen::string_length( ent->d_name );
                len = std::min<u32>( len, 1022 );
                
                pen::memory_cpy(results.children[i].name, ent->d_name, len);
                results.children[i].name[len] = '\0';
                
                results.children[i].num_children = 0;
                
                ++i;
            }
            
            closedir (dir);
        }
        
        return PEN_ERR_OK;
    }
    
    pen_error filesystem_enum_free_mem( fs_tree_node &tree )
    {
        for( s32 i = 0; i < tree.num_children; ++i )
        {
            filesystem_enum_free_mem( tree.children[ i ] );
        }
        
        pen::memory_free( tree.children );
        pen::memory_free( tree.name );
        
        return PEN_ERR_OK;
    }
    
    pen_error filesystem_getmtime( const c8* filename, u32& mtime_out )
    {
        struct stat stat_res;
        
        stat( filename, &stat_res);
        
        timespec t = stat_res.st_mtimespec;
        
        mtime_out = t.tv_sec;
        
        return PEN_ERR_OK;
    }
	
	//ICONV ref for utf8 -> wchar conversion
	 /*
		iconv_t ic;
		ic = iconv_open("UTF8", "WCHAR_T");
		
		u32 sowc = sizeof( wchar_t );
		
		size_t dir_name_len = pen::string_length_wide(directory);
		size_t target_buffer_len = dir_name_len+1;
		size_t dir_name_size_bytes = target_buffer_len * sowc;
		
		signed char* utf8_dir = (signed char*)pen::memory_alloc(target_buffer_len);
		
		size_t ret = iconv( ic, (c8**)&directory, &dir_name_size_bytes, (c8**)&utf8_dir, &target_buffer_len );
    */
    
}
