#ifndef _memory_h
#define _memory_h

#include "pen.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#define BAD_ALLOC
#define PEN_MEM_ALIGN_ALLOC( mem, align, size ) mem = _aligned_malloc( align, size )
#define PEN_MEM_ALIGN_FREE _aligned_free
#else
#define PEN_MEM_ALIGN_ALLOC( mem, align, size ) posix_memalign( &mem, align, size )
#define PEN_MEM_ALIGN_FREE free
#define BAD_ALLOC std::bad_alloc
#endif

namespace pen
{
	// Minimalist C-Style memory API wrapping up malloc and free
	// It provides some very minor portability solutions between windows and unix
	// But mostly it is here to intercept all allocations
	// So at a later date custom allocation or mem tracking schemes could be used.

	// Functions

	void*	memory_alloc( u32 size_bytes );
	void*	memory_alloc_align( u32 size_bytes, u32 alignment );
	void*	memory_realloc(void* mem, u32 size_bytes);
	void	memory_free( void* mem );
	void	memory_free_align( void* mem );
	void	memory_cpy( void* dest, const void* src, u32 size_bytes );
	void	memory_set( void* dest, u8 val, u32 size_bytes );
	void	memory_zero( void* dest, u32 size_bytes );

	// Implementation

    inline void* memory_alloc( u32 size_bytes )
    {
        return malloc( size_bytes );
    }

    inline void* memory_realloc( void* mem, u32 size_bytes )
    {
        return realloc( mem, size_bytes );
    }

    inline void memory_free( void* mem )
    {
        free( mem );
    }

    inline void memory_zero( void* dest, u32 size_bytes )
    {
        memory_set( dest, 0x00, size_bytes );
    }

    inline void memory_set( void* dest, u8 val, u32 size_bytes )
    {
        memset( dest, val, size_bytes );
    }

    inline void memory_cpy( void* dest, const void* src, u32 size_bytes )
    {
        memcpy( dest, src, size_bytes );
    }

    inline void* memory_alloc_align( u32 size_bytes, u32 alignment )
    {
        void* mem;
        PEN_MEM_ALIGN_ALLOC( mem, size_bytes, alignment );
        
        return mem;
    }

    inline void memory_free_align( void* mem )
    {
        PEN_MEM_ALIGN_FREE( mem );
    }
}

// And override global new and delete

void*	operator new(size_t n) throw(BAD_ALLOC);
void	operator delete(void *p) throw();

void*	operator new[](size_t n) throw(BAD_ALLOC);
void	operator delete[](void *p) throw();

#endif
