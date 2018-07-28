#ifndef _pen_console_h
#define _pen_console_h

#include "types.h"

#include <assert.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Wrapper around assert and print for portability
// and to control and re-direct in the future if required

inline void output_debug(const c8* format, ...)
{
    va_list va;
    va_start(va, format);

    static u32 s_buffer_size = 1024;
    static c8* buf = new c8[s_buffer_size];

    u32 n = vsnprintf(buf, s_buffer_size, format, va);
    if (n > s_buffer_size)
    {
        s_buffer_size = n * 2;
        delete buf;
        buf = new c8[s_buffer_size];
        vsnprintf(buf, s_buffer_size, format, va);
    }

    va_end(va);

#ifdef _WIN32
    OutputDebugStringA(buf);
#else
    printf("%s\n", buf);
#endif
}

#if TARGET_OS_IPHONE
#define PEN_SYSTEM
#else
#define PEN_SYSTEM system
#endif
#define PEN_LOG output_debug
#define PEN_ASSERT assert
#define PEN_ASSERT_MSG(A, M)                                                                                                 \
    assert(A);                                                                                                               \
    output_debug(M)
#define PEN_ERROR assert(0)

// Some useful macros for calling the pmtech build script from system()
// Directory to tools is configured in build_config.json
// Make sure python3 is setup in path or bash_profile

#ifdef _WIN32
#define PEN_DIR '\\'
#define PEN_PYTHON3 "py -3 "
#define PEN_SHADER_COMPILE_CMD "tools\\build_shaders.py -root_dir ..\\..\\"
#define PEN_BUILD_CMD "tools\\build.py -root_dir ..\\..\\"
#else // Unix
#define PEN_DIR '/'
#define PEN_PYTHON3 ". ~/.bash_profile;  python3 "
#define PEN_SHADER_COMPILE_CMD "tools/build_shaders.py -root_dir ../../"
#define PEN_BUILD_CMD "tools/build.py -root_dir ../../"
#endif

#endif //_pen_console_h
