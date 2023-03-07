// str_utilities.h
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "console.h"
#include "memory.h"
#include "pen.h"
#include "pen_string.h"

#include "str/Str.h"

#if PEN_PLATFORM_WIN32
#define PEN_OS_SEP '\\'
#define PEN_OS_RSEP '/'
#else
#define PEN_OS_SEP '/'
#define PEN_OS_RSEP '\\'
#endif

namespace pen
{
    // Function decl

    s32  str_find(const Str& string, const c8* search, u32 start_pos = 0);
    s32  str_find_reverse(const Str& string, const c8* search, s32 start_pos = -1);
    Str  str_substr(const Str& string, s32 start, s32 end);
    bool str_ends_with(const Str& string, const c8* ends);
    Str  str_remove_ext(const Str& string);
    Str  str_replace_chars(const Str& string, const c8 search, const c8 replace);
    Str  str_replace_string(const Str& string, const c8* search, const c8* replace);
    Str  str_normalize_filepath(const Str& filepath);
    Str  str_sanitize_filepath(const Str& filepath);
    Str  str_basename(const Str& filepath);
    Str  str_to_lower(const Str& string);
    Str  str_to_upper(const Str& string);

    // Implementation

    inline s32 str_find_reverse(const Str& string, const c8* search, s32 start_pos)
    {
        s32 len = string.length();
        s32 search_len = pen::string_length(search);

        if (start_pos == -1)
            start_pos += len;

        s32 i = start_pos;
        s32 j = search_len - 1;
        s32 find_start = start_pos;
        while (j <= i)
        {
            if (string[i] == search[j])
            {
                j--;
            }
            else
            {
                find_start = i - 1;
                j = search_len - 1;
            }

            if (j < 0)
                return i; // find_start - search_len;

            --i;
        }

        return -1;
    }

    inline Str str_substr(const Str& string, s32 start, s32 end)
    {
        Str sub = "";
        for (s32 i = start; i < end; ++i)
        {
            sub.appendf("%c", string[i]);
        }

        return sub;
    }

    inline bool str_ends_with(const Str& string, const c8* ends)
    {
        s32 len = string.length();
        s32 ii = str_find_reverse(string, ends);

        if (len - ii == pen::string_length(ends))
            return true;

        return false;
    }

    inline Str str_remove_ext(const Str& string)
    {
        s32 ext = str_find_reverse(string, ".");
        s32 dir = str_find_reverse(string, "/");

        if (ext > dir)
            return str_substr(string, 0, ext);

        return string;
    }

    inline s32 str_find(const Str& string, const c8* search, u32 start_pos)
    {
        s32 len = string.length();
        s32 search_len = pen::string_length(search);

        s32 i = start_pos;
        s32 j = 0;
        s32 find_start = 0;
        while (search_len - j <= len - i)
        {
            if (string[i] == search[j])
            {
                j++;
            }
            else
            {
                find_start = i + 1;
                j = 0;
            }

            if (j == search_len)
                return find_start;

            ++i;
        }

        return -1;
    }

    inline Str str_replace_chars(const Str& string, const c8 search, const c8 replace)
    {
        s32 len = string.length();

        Str r = string;
        c8* iter = &r[0];
        while (iter && len > 0)
        {
            if (*iter == search)
                *iter = replace;

            ++iter;
            --len;
        }

        return r;
    }

    inline Str str_replace_string(const Str& string, const c8* search, const c8* replace)
    {
        s32 len = string.length();
        s32 search_len = pen::string_length(search);

        s32 find_start = str_find(string, search);

        if (find_start == -1)
            return string;

        s32 end_start = find_start + search_len;
        s32 end_len = len - end_start;

        c8* start_buf = new c8[find_start + 1];
        c8* end_buf = new c8[end_len + 1];

        memcpy(start_buf, string.c_str(), find_start);
        start_buf[find_start] = '\0';

        memcpy(end_buf, string.c_str() + end_start, end_len);
        end_buf[end_len] = '\0';

        Str result = start_buf;
        result.append(replace);
        result.append(end_buf);

        delete[] start_buf;
        delete[] end_buf;

        return result;
    }

    inline Str str_sanitize_filepath(const Str& filepath)
    {
        return str_replace_chars(filepath, PEN_OS_RSEP, PEN_OS_SEP);
    }

    inline Str str_normalize_filepath(const Str& filepath)
    {
        Str f = str_replace_chars(filepath, '\\', '/');

        for (;;)
        {
            u32 dir = str_find(f, "..", 0);
            if (dir == -1)
                break;

            u32 back_dir = str_find_reverse(f, "/", dir - 2);

            // remove the back dir and the ..
            Str a = str_substr(f, 0, back_dir);
            Str b = str_substr(f, dir + 2, f.length());

            f = a;
            f.append(b.c_str());
        }

        return f;
    }

    inline Str str_basename(const Str& filepath)
    {
        Str f = str_replace_chars(filepath, '\\', '/');
        u32 last = str_find_reverse(f, "/");
        if (last != -1)
        {
            return str_substr(f, last + 1, f.length());
        }
        return filepath;
    }

    inline Str str_to_lower(const Str& string)
    {
        Str result = "";
        s32 len = string.length();
        for (s32 i = 0; i < len; ++i)
        {
            if ((string[i] >= 'a' && string[i] <= 'z') || (string[i] >= 'A' && string[i] <= 'Z'))
                result.append(string[i] | (1 << 5));
            else
                result.append(string[i]);
        }
        return result;
    }

    inline Str str_to_upper(const Str& string)
    {
        Str result = "";
        s32 len = string.length();
        for (s32 i = 0; i < len; ++i)
        {
            if ((string[i] >= 'a' && string[i] <= 'z') || (string[i] >= 'A' && string[i] <= 'Z'))
                result.append(string[i] & ~(1 << 5));
            else
                result.append(string[i]);
        }
        return result;
    }
} // namespace pen
