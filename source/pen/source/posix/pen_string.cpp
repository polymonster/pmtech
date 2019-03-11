// pen_string.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "pen_string.h"
#include "memory.h"
#include <stdarg.h>
#include <string.h>
#include <wchar.h>

namespace pen
{
    void string_to_wide(const c8* src, c16* dest)
    {
        u32 len = pen::string_length(src);

        for (u32 i = 0; i < len; ++i)
        {
            dest[i] = (c16)src[i];
        }
    }

    void string_to_ascii(const c16* src, c8* dest)
    {
        u32 len = pen::string_length_wide(src);

        for (u32 i = 0; i < len; ++i)
        {
            dest[i] = (c8)src[i];
        }
    }

    u32 string_compare(const c8* string_a, const c8* string_b)
    {
        return strcmp(string_a, string_b);
    }

    u32 string_compare_wide(const c16* string_a, const c16* string_b)
    {
        return wcscmp(string_a, string_b);
    }

    void string_format_va(c8* dest, u32 buffer_size, const c8* format, va_list& va)
    {
        vsnprintf(dest, buffer_size, format, va);
    }

    void string_format(c8* dest, u32 buffer_size, const c8* format, ...)
    {
        va_list va;
        va_start(va, format);

        vsnprintf(dest, buffer_size, format, va);

        va_end(va);
    }

    void string_format_wide(c16* dest, u32 buffer_size, const c16* format, ...)
    {
        va_list va;
        va_start(va, format);

        vswprintf(dest, buffer_size, format, va);

        va_end(va);
    }

    void string_concatonate(c8* dest, const c8* src, u32 buffer_size)
    {
        strcat(dest, src);
    }

    void string_concatonate_wide(c16* dest, const c16* src, u32 buffer_size)
    {
        wcsncat(dest, src, buffer_size);
    }

    u32 string_length(const c8* string)
    {
        return strlen(string);
    }

    u32 string_length_wide(const c16* string)
    {
        return wcslen(string);
    }
} // namespace pen
