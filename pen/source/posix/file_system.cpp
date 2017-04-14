#include <stdio.h>
#include <dirent.h>
#include <iconv.h>
#include <sys/stat.h>

#include "file_system.h"
#include "memory.h"
#include "pen_string.h"

namespace pen
{
    u32 filesystem_read_file_to_buffer( const char* filename, void** p_buffer, u32 &buffer_size )
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

            return 0;
        }

        return 1;
    }

    u32 filesystem_enum_directory( const c16* directory, filesystem_enumeration &results )
    {
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
        
        //todo handle utf-8 file names
        size_t  dir_name_len = pen::string_length_wide(directory);
        c8*     dir_c8 = (c8*)pen::memory_alloc(dir_name_len);
        
        pen::string_to_ascii(directory, dir_c8);
       
        
        DIR *dir;
        struct dirent *ent;
        
        if ((dir = opendir (dir_c8)) != NULL)
        {
            while ((ent = readdir (dir)) != NULL)
            {
                printf ("%s\n", ent->d_name);
            }

            closedir (dir);
        }

        return 0;
    }
    
    f32 filesystem_getmtime( const c8* filename )
    {
        struct stat stat_res;
        
        stat( filename, &stat_res);
        
        timespec t = stat_res.st_mtimespec;
        
        return (f32)t.tv_sec;
    }
    
}
