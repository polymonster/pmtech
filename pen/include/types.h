#ifndef _pen_types_h
#define _pen_types_h

#include <algorithm>
#include <assert.h>
#include <atomic>
#include <float.h>
#include <stdint.h>
#include <stdio.h>

// Almost all files will include this
// Define small types for extra cuteness

typedef int32_t s32;
typedef uint32_t u32;
typedef int16_t s16;
typedef uint16_t u16;
typedef int64_t u64;
typedef uint32_t hash_id;

typedef char s8;
typedef unsigned char u8;

typedef wchar_t c16;
typedef char c8;

typedef float f32;
typedef double f64;

typedef unsigned long ulong;
typedef unsigned long dword; // for win32

typedef std::atomic<uint8_t> a_u8;
typedef std::atomic<uint32_t> a_u32;
typedef std::atomic<uint64_t> a_u64;

// Thread return value just for win32 portability

#ifdef _WIN32
#define PEN_TRV dword __stdcall
#else
#define PEN_TRV void *
#endif

#define PEN_THREAD_OK 0

// Use min max and swap everywhere and undef windows

#ifdef WIN32
#undef min
#undef max
#endif

using std::max;
using std::min;
using std::swap;

// Use small generic handles for all resource types
// Auido, Renderer etc

typedef uint32_t peh;

#define PEN_INVALID_HANDLE ( ( u32 )-1 )
inline bool is_valid( u32 handle )
{
    return handle != PEN_INVALID_HANDLE;
}

inline bool is_invalid( u32 handle )
{
    return handle == PEN_INVALID_HANDLE;
}

// Generic erors for the few cases erors are handled :)

enum pen_error
{
    PEN_ERR_OK = 0,
    PEN_ERR_FILE_NOT_FOUND = 1,
    PEN_ERR_NOT_READY = 2,
    PEN_ERR_FAILED = 3
};

// Minimal amount of macros that are handy to have evrywhere

// For making texture formats ('D' 'X' 'T' '1') etc

#define PEN_FOURCC( ch0, ch1, ch2, ch3 )                                                                                     \
    ( ( ulong )( c8 )( ch0 ) | ( ( ulong )( c8 )( ch1 ) << 8 ) | ( ( ulong )( c8 )( ch2 ) << 16 ) |                          \
      ( ( ulong )( c8 )( ch3 ) << 24 ) )

#define PEN_ARRAY_SIZE( A ) ( sizeof( A ) / sizeof( A[ 0 ] ) )
#define PEN_REQUIRE( A ) // for tests

#endif //_pen_types_h
