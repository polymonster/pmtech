// console.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

// Wrapper around assert and print for portability, to control and re-direct in the future if required

#pragma once

#include "types.h"

#include <assert.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#elif __APPLE__
#include "TargetConditionals.h"
#endif

inline void output_debug(const c8* format, ...)
{
    va_list va;
    va_start(va, format);

    static u32 s_buffer_size = 1024 * 1024;
    static c8* buf = new c8[s_buffer_size];

    u32 n = vsnprintf(buf, s_buffer_size, format, va);
    va_end(va);

    if (n > s_buffer_size)
    {
        va_start(va, format);

        s_buffer_size = n * 2;
        delete[] buf;
        buf = new c8[s_buffer_size];

        vsnprintf(buf, s_buffer_size, format, va);

        va_end(va);
    }

    va_end(va);

#ifdef _WIN32
	buf[n] = '\n';
	buf[n + 1] = '\0';
    OutputDebugStringA(buf);
#else
    printf("%s\n", buf);
#endif
}

#if TARGET_OS_IPHONE
#define PEN_SYSTEM(c) (void)c
#else
#define PEN_SYSTEM system
#endif
#define PEN_LOG output_debug
#define PEN_CONSOLE printf
#define PEN_ASSERT assert
#define PEN_ASSERT_MSG(A, M)                                                                                                 \
    assert(A);                                                                                                               \
    output_debug(M)
#define PEN_ERROR assert(0)
