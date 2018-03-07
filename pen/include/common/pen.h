#ifndef _pen_h
#define _pen_h

//platform specific
#ifdef _WIN32
#include <windows.h>
#define PEN_THREAD_OK                   0
#define PEN_THREAD_ROUTINE( FP )        LPTHREAD_START_ROUTINE FP
#define PEN_THREAD_RETURN               DWORD WINAPI
#define PEN_DIR                            '\\'
#define PEN_SHADER_COMPILE_PRE_CMD        "py -3 "
#define PEN_SHADER_COMPILE_CMD          "tools\\build_shaders.py -root_dir ..\\..\\"
#define PEN_BUILD_CMD                    "tools\\build.py -root_dir ..\\..\\"
#else
#define PEN_THREAD_OK                   nullptr
#define PEN_THREAD_RETURN               void*
#define PEN_THREAD_ROUTINE( FP )        PEN_THREAD_RETURN (*FP)(void* data)
#define PEN_SHADER_COMPILE_PRE_CMD      ". ~/.bash_profile;  python3 "
#define PEN_SHADER_COMPILE_CMD          "tools/build_shaders.py -root_dir ../../"
#define PEN_DIR                            '/'
#define PEN_BUILD_CMD                    "tools/build.py -root_dir ../../"
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
typedef char            c8;

typedef float           f32;
typedef double          f64;

typedef unsigned long   ulong;

typedef std::atomic<uint8_t> a_u8;
typedef std::atomic<uint32_t> a_u32;
typedef std::atomic<uint64_t> a_u64;

#define PEN_F32_MAX FLT_MAX

#define PEN_FMAX fmax
#define PEN_FMIN fmin

#define PEN_UMIN( A, B ) A < B ? A : B
#define PEN_UMAX( A, B ) A > B ? A : B

#define PEN_ARRAY_SIZE( A ) sizeof(A)/sizeof(A[0])

//--------------------------------------------------------------------------------------
// Print / Assert
//--------------------------------------------------------------------------------------
#define    PEN_PRINT_CHAR_LIMIT 1024*10
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
#define PEN_PRINTF                output_debug
#define PEN_ASSERT                assert
#define PEN_ASSERT_MSG(A,M)        assert(A); output_debug(M)
#define PEN_ERR                    assert( 0 )

//--------------------------------------------------------------------------------------
// Multithreaded Resource allocations
//--------------------------------------------------------------------------------------
enum resource_types
{
    MAX_RENDERER_RESOURCES = 10000,
    MAX_AUDIO_RESOURCES = 100
};

#define PEN_INVALID_HANDLE (u32)-1

#define PEN_FOURCC(ch0, ch1, ch2, ch3)                                            \
((ulong)(c8)(ch0) | ((ulong)(c8)(ch1) << 8) |                \
((ulong)(c8)(ch2) << 16) | ((ulong)(c8)(ch3) << 24 ))


namespace pen
{
    struct window_creation_params
    {
        u32 width;
        u32 height;
        u32 sample_count;
        const c8* window_title;
    };
    
    struct user_info
    {
        const c8* user_name;
        const c8* full_user_name;
    };
    
    extern PEN_THREAD_RETURN game_entry( void* params );
}

#ifndef NO_STRETCHY_BUFFER_SHORT_NAMES
#define sb_free   stb_sb_free
#define sb_push   stb_sb_push
#define sb_count  stb_sb_count
#define sb_add    stb_sb_add
#define sb_last   stb_sb_last
#endif

#define stb_sb_free(a)         ((a) ? free(stb__sbraw(a)),0 : 0)
#define stb_sb_push(a,v)       (stb__sbmaybegrow(a,1), (a)[stb__sbn(a)++] = (v))
#define stb_sb_count(a)        ((a) ? stb__sbn(a) : 0)
#define stb_sb_add(a,n)        (stb__sbmaybegrow(a,n), stb__sbn(a)+=(n), &(a)[stb__sbn(a)-(n)])
#define stb_sb_last(a)         ((a)[stb__sbn(a)-1])

#define stb__sbraw(a) ((int *) (a) - 2)
#define stb__sbm(a)   stb__sbraw(a)[0]
#define stb__sbn(a)   stb__sbraw(a)[1]

#define stb__sbneedgrow(a,n)  ((a)==0 || stb__sbn(a)+(n) >= stb__sbm(a))
#define stb__sbmaybegrow(a,n) (stb__sbneedgrow(a,(n)) ? stb__sbgrow(a,n) : 0)
#define stb__sbgrow(a,n)      (*((void **)&(a)) = stb__sbgrowf((a), (n), sizeof(*(a))))

static void * stb__sbgrowf(void *arr, int increment, int itemsize)
{
    int dbl_cur = arr ? 2*stb__sbm(arr) : 0;
    int min_needed = stb_sb_count(arr) + increment;
    int m = dbl_cur > min_needed ? dbl_cur : min_needed;
    int *p = (int *) realloc(arr ? stb__sbraw(arr) : 0, itemsize * m + sizeof(int)*2);
    if (p) {
        if (!arr)
            p[1] = 0;
        p[0] = m;
        return p+2;
    } else {
#ifdef STRETCHY_BUFFER_OUT_OF_MEMORY
        STRETCHY_BUFFER_OUT_OF_MEMORY ;
#endif
        return (void *) (2*sizeof(int)); // try to force a NULL pointer exception later
    }
}

#endif

