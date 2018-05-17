#ifndef _pen_console_h
#define _pen_console_h

#include <assert.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

// Wrapper around assert and print for portability
// and to control and re-direct in the future if required

#define PEN_PRINT_CHAR_LIMIT 1024 * 10
inline void output_debug(const c8 *format, ...)
{
    va_list va;
    va_start(va, format);

    static c8 buf[PEN_PRINT_CHAR_LIMIT];
    vsnprintf(buf, PEN_PRINT_CHAR_LIMIT, format, va);

    va_end(va);

#ifdef _WIN32
    OutputDebugStringA(buf);
#else
    printf("%s\n", buf);
#endif
}
#define PEN_PRINTF output_debug
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
