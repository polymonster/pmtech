#ifndef _definitions_h
#define _definitions_h

//platform specific
#ifdef _WIN32
#include <windows.h>
#define PEN_THREAD_ROUTINE		LPTHREAD_START_ROUTINE
#define PEN_THREAD_RETURN		DWORD WINAPI
#else
#define PEN_THREAD_RETURN		unsigned long
#define PEN_THREAD_ROUTINE		void (p_function*)()
#endif


//c++
#include <stdio.h>
#include <assert.h> 
#include <float.h>
#include <stdint.h>
#include <atomic>

//--------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------
typedef int s32;
typedef unsigned int u32;
typedef short s16;
typedef unsigned short u16;
typedef char s8;
typedef unsigned char u8;
typedef int64_t u64;

typedef wchar_t c16;
typedef char	c8;

typedef float f32;
typedef double f64;

typedef unsigned long ulong;

//typedef std::atomic_uint8_t a_u8;
//typedef std::atomic_uint32_t a_u32;
//typedef std::atomic_uint64_t a_u64;

typedef std::atomic<uint8_t> a_u8;
typedef std::atomic<uint32_t> a_u32;
typedef std::atomic<uint64_t> a_u64;

#define PEN_F32_MAX FLT_MAX

#define PEN_FMAX fmax
#define PEN_FMIN fmin


//--------------------------------------------------------------------------------------
// Print / Assert 
//--------------------------------------------------------------------------------------
#define	PEN_PRINT_CHAR_LIMIT	64
#define PEN_PRINTF				pen::string_output_debug
#define PEN_ASSERT				assert
#define PEN_ERR					assert( 0 ) 

#endif
