#include "file_system.h"
#include "memory.h"
#include <stdio.h>
#include "pen_string.h"

namespace pen
{
    u32 filesystem_read_file_to_buffer( const char* filename, void** p_buffer, u32 &buffer_size )
    {
        *p_buffer = NULL;

        FILE* p_file = nullptr;
        fopen_s( &p_file, filename, "rb" );

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
}