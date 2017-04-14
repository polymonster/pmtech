#ifndef _filesystem_h
#define _filesystem_h

#include "definitions.h"

namespace pen
{
    typedef struct filesystem_enumeration
    {
        c8** paths;
        u32	 num_files;
    }filesystem_enumeration;

    u32 filesystem_read_file_to_buffer( const c8* filename, void** p_buffer, u32 &buffer_size );

    u32 filesystem_enum_directory( const c16* directory, filesystem_enumeration &results );
    
    u32 filesystem_getmtime( const c8* filename );
}

#endif
