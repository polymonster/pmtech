#ifndef _string_h
#define _string_h

// String API which has functions for win32 / unix portability

#include "memory.h"
#include "pen.h"

namespace pen
{
    void string_to_ascii(const c16* src, c8* dest);
    void string_to_wide(const c8* src, c16* dest);
    u32 string_compare(const c8* string_a, const c8* string_b);
    u32 string_compare_wide(const c16* string_a, const c16* string_b);
    void string_format_va(c8* dest, u32 buffer_size, const c8* format, va_list& va);
    void string_format(c8* dest, u32 buffer_size, const c8* format, ...);
    void string_format_wide(c16* dest, u32 buffer_size, const c16* format, ...);
    void string_concatonate(c8* dest, const c8* src, u32 buffer_size);
    void string_concatonate_wide(c16* dest, const c16* src, u32 buffer_size);
    u32 string_length(const c8* string);
    u32 string_length_wide(const c16* string);
    c8* sub_string(c8* src, u32 length);

    // Implementation
    
    inline c8* sub_string(const c8* src, u32 length)
    {
        u32 padded_length = length + 1;
        c8* new_string = (c8*)malloc(padded_length);
        memcpy(new_string, src, length);
        new_string[length] = '\0';

        return new_string;
    }

    inline void sub_string(const c8* src, c8* buf, u32 length)
    {
        memcpy(buf, src, length);
        buf[length] = '\0';
    }
} // namespace pen

#endif
