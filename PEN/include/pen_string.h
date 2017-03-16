#ifndef _string_h
#define _string_h

#include "definitions.h"

namespace pen
{
    void string_to_ascii( const c16* src, c8* dest );
    void string_to_wide( const c8* src, c16* dest );

    u32	 string_compare( const c8* string_a, const c8* string_b );
    u32	 string_compare_wide( const c16* string_a, const c16* string_b );

    void string_format( c8* dest, u32 buffer_size, const c8* format, ... );
    void string_format_wide( c16* dest, u32 buffer_size, const c16* format, ... );

    void string_concatonate( c8* dest, const c8* src, u32 buffer_size );
    void string_concatonate_wide( c16* dest, const c16* src, u32 buffer_size );

    u32  string_length( const c8* string );
    u32  string_length_wide( const c16* string );

    void string_output_debug( const c8* format, ... );
}

#endif
