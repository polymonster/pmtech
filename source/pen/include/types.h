// types.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#ifndef _pen_types_h
#define _pen_types_h

#include <algorithm>
#include <atomic>
#include <float.h>
#include <stdint.h>
#include <stdio.h>

// Define small types for extra cuteness

typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;
typedef uint32_t hash_id;

typedef char    c8;
typedef wchar_t c16;

typedef uint16_t f16;
typedef float    f32;
typedef double   f64;

typedef unsigned long ulong;
typedef unsigned long dword; // for win32

typedef std::atomic<uint8_t>  a_u8;
typedef std::atomic<uint32_t> a_u32;
typedef std::atomic<uint64_t> a_u64;
typedef std::atomic<size_t>   a_size_t;

// Thread return value just for win32 portability
#ifdef _WIN32
#define PEN_TRV dword __stdcall
#else
#define PEN_TRV void*
#endif
#define PEN_THREAD_OK 0

// Use min max and swap everywhere and undef windows
#define NOMINMAX
#ifdef WIN32
#undef min
#undef max
#endif

using std::max;
using std::min;
using std::swap;

// Use small generic handles for resource types
typedef uint32_t peh;

#define PEN_INVALID_HANDLE ((u32)-1)
inline bool is_valid(u32 handle)
{
    return handle != PEN_INVALID_HANDLE;
}

inline bool is_invalid(u32 handle)
{
    return handle == PEN_INVALID_HANDLE;
}

inline bool is_valid_non_null(u32 handle)
{
    return handle != PEN_INVALID_HANDLE && handle != 0;
}

inline bool is_invalid_or_null(u32 handle)
{
    return handle == PEN_INVALID_HANDLE || handle == 0;
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

#define PEN_FOURCC(ch0, ch1, ch2, ch3)                                                                                       \
    ((ulong)(c8)(ch0) | ((ulong)(c8)(ch1) << 8) | ((ulong)(c8)(ch2) << 16) | ((ulong)(c8)(ch3) << 24))

#define PEN_ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))
#define PEN_UNUSED (void)

inline f16 float_to_half(f32 f)
{
    union bits {
        float    f;
        int32_t  si;
        uint32_t ui;
    };

    static int const     shift = 13;
    static int const     shift_sign = 16;
    static int32_t const infN = 0x7F800000;  // flt32 infinity
    static int32_t const maxN = 0x477FE000;  // max flt16 normal as a flt32
    static int32_t const minN = 0x38800000;  // min flt16 normal as a flt32
    static int32_t const signN = 0x80000000; // flt32 sign bit
    static int32_t const infC = infN >> shift;
    static int32_t const nanN = (infC + 1) << shift; // minimum flt16 nan as a flt32
    static int32_t const maxC = maxN >> shift;
    static int32_t const minC = minN >> shift;
    static int32_t const mulN = 0x52000000; // (1 << 23) / minN
    static int32_t const subC = 0x003FF;    // max flt32 subnormal down shifted
    static int32_t const maxD = infC - maxC - 1;
    static int32_t const minD = minC - subC - 1;

    bits v, s;
    v.f = f;
    uint32_t sign = v.si & signN;
    v.si ^= sign;
    sign >>= shift_sign; // logical shift
    s.si = mulN;
    s.si = s.f * v.f; // correct subnormals
    v.si ^= (s.si ^ v.si) & -(minN > v.si);
    v.si ^= (infN ^ v.si) & -((infN > v.si) & (v.si > maxN));
    v.si ^= (nanN ^ v.si) & -((nanN > v.si) & (v.si > infN));
    v.ui >>= shift; // logical shift
    v.si ^= ((v.si - maxD) ^ v.si) & -(v.si > maxC);
    v.si ^= ((v.si - minD) ^ v.si) & -(v.si > subC);
    return v.ui | sign;
}

#endif //_pen_types_h
