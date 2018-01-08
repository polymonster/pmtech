#ifndef _definitions_h
#define _definitions_h

//platform specific
#ifdef _WIN32
#include <windows.h>
#define PEN_THREAD_OK                   0
#define PEN_THREAD_ROUTINE( FP )        LPTHREAD_START_ROUTINE FP
#define PEN_THREAD_RETURN               DWORD WINAPI
#define PEN_DIR							'\\' 
#define PEN_SHADER_COMPILE_PRE_CMD		"py -3 " 
#define PEN_SHADER_COMPILE_CMD          "tools\\build_shaders.py -root_dir ..\\..\\"
#define PEN_BUILD_CMD					"tools\\build.py -root_dir ..\\..\\"
#else
#define PEN_THREAD_OK                   nullptr
#define PEN_THREAD_RETURN               void*
#define PEN_THREAD_ROUTINE( FP )		PEN_THREAD_RETURN (*FP)(void* data)
#define PEN_SHADER_COMPILE_PRE_CMD      ". ~/.bash_profile;  python3 "
#define PEN_SHADER_COMPILE_CMD          "tools/build_shaders.py -root_dir ../../"
#define PEN_DIR							'/' 
#define PEN_BUILD_CMD					"tools/build.py -root_dir ../../"
#endif

enum pen_error
{
    PEN_ERR_OK = 0,
    PEN_ERR_FILE_NOT_FOUND = 1,
    PEN_ERR_NOT_READY = 2,
    PEN_ERR_FAILED = 3
};

#include <stdio.h>
#include <assert.h> 
#include <float.h>
#include <stdint.h>
#include <atomic>
#include <algorithm>

//--------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------
typedef int32_t         s32;
typedef uint32_t        u32;
typedef int16_t         s16;
typedef uint16_t        u16;
typedef int64_t         u64;
typedef uint32_t        hash_id;

typedef char            s8;
typedef unsigned char   u8;

typedef wchar_t         c16;
typedef char	        c8;

typedef float           f32;
typedef double          f64;


typedef unsigned long ulong;

typedef std::atomic<uint8_t> a_u8;
typedef std::atomic<uint32_t> a_u32;
typedef std::atomic<uint64_t> a_u64;

#define PEN_F32_MAX FLT_MAX

#define PEN_FMAX fmax
#define PEN_FMIN fmin

#define PEN_UMIN std::min<u32>
#define PEN_UMAX std::min<u32>

#define PEN_ARRAY_SIZE( A ) sizeof(A)/sizeof(A[0])

//--------------------------------------------------------------------------------------
// Print / Assert 
//--------------------------------------------------------------------------------------
#define	PEN_PRINT_CHAR_LIMIT 4096
inline void output_debug( const c8* format, ... )
{
    va_list va;
    va_start( va, format );
    
    static c8 buf[ PEN_PRINT_CHAR_LIMIT ];
    vsnprintf( buf, PEN_PRINT_CHAR_LIMIT, format, va );
    
    va_end( va );
    
#ifdef _WIN32
    OutputDebugStringA( buf );
#else
    printf( "%s\n", buf );
#endif
}
#define PEN_PRINTF				output_debug
#define PEN_ASSERT				assert
#define PEN_ASSERT_MSG(A,M)		assert(A); output_debug(M)
#define PEN_ERR					assert( 0 ) 

//--------------------------------------------------------------------------------------
// Multithreaded Resource allocations 
//--------------------------------------------------------------------------------------
enum resource_types
{
    MAX_RENDERER_RESOURCES = 10000,
    MAX_AUDIO_RESOURCES = 100
};

#define PEN_INVALID_HANDLE (u32)-1

#endif
