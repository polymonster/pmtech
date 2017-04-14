#include "file_system.h"
#include "memory.h"
#include <stdio.h>
#include "pen_string.h"

namespace pen
{
#define WINDOWS_TICK 10000000
#define SEC_TO_UNIX_EPOCH 11644473600LL

	u32 win32_time_to_unix_seconds(long long ticks)
	{
		return (u32)(ticks / WINDOWS_TICK - SEC_TO_UNIX_EPOCH);
	}

    u32 filesystem_read_file_to_buffer( const char* filename, void** p_buffer, u32 &buffer_size )
    {
        //swap "/" for "\\"
        const char* p_src_char = filename;

        u32 str_len = pen::string_length( filename );
        char* windir_filename = (char*)pen::memory_alloc(str_len + 1);

        char* p_dest_char = windir_filename;

        while( p_src_char < filename + str_len )
        {
            if( *p_src_char == '/' )
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

            return 0;
        }

        return 1;
    }

    u32 filesystem_enum_directory( const c16* directory, filesystem_enumeration &results )
    {
        WIN32_FIND_DATA ffd;
        HANDLE hFind = INVALID_HANDLE_VALUE;

        hFind = FindFirstFileW( directory, &ffd );

        do
        {
            if( ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                PEN_PRINTF( "Dir : %s\n", ffd.cFileName );
            }
            else
            {
                PEN_PRINTF( "File : %s\n", ffd.cFileName );
            }
        } while( FindNextFile( hFind, &ffd ) != 0 );

        return 0;
    }

	f32 filesystem_getmtime( const c8* filename )
	{
		OFSTRUCT of_struct;
		HFILE f = OpenFile(filename, &of_struct, OF_READ );

		FILETIME c, m, a;

		BOOL res = GetFileTime((HANDLE)f, &c, &a, &m);

		long long* wt = (long long*)&m;

		return (f32)win32_time_to_unix_seconds(*wt);;
	}
}