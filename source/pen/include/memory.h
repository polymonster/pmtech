// memory.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

// Minimalist memory api wrapping up malloc and free.
// It provides some very minor portability solutions between win32 and osx and linux.
// Mostly it is here to intercept allocs, so at a later date custom allocation or tracking schemes could be used.

#pragma once

#include "pen.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <string.h> //memcpy
#define THROW_BAD_ALLOC
#define THROW_NO_EXCEPT noexcept(true)
#define PEN_MEM_ALIGN_ALLOC(mem, align, size) posix_memalign(&mem, align, size)
#define PEN_MEM_ALIGN_FREE free
#elif _WIN32
#define THROW_BAD_ALLOC
#define THROW_NO_EXCEPT
#define PEN_MEM_ALIGN_ALLOC(mem, align, size) mem = _aligned_malloc(size, align)
#define PEN_MEM_ALIGN_FREE _aligned_free
#else // macOS, iOS
#define PEN_MEM_ALIGN_ALLOC(mem, align, size) posix_memalign(&mem, align, size)
#define PEN_MEM_ALIGN_FREE free
#define THROW_BAD_ALLOC throw(std::bad_alloc)
#define THROW_NO_EXCEPT throw()
#endif

namespace pen
{
    // Functions

    void* memory_alloc(size_t size_bytes);
    void* memory_alloc_align(size_t size_bytes, size_t alignment);
    void* memory_realloc(void* mem, size_t size_bytes);
    void  memory_free(void* mem);
    void  memory_free_align(void* mem);
    void  memory_zero(void* dest, size_t size_bytes);

    // Implementation

    inline void* memory_alloc(size_t size_bytes)
    {
        return malloc(size_bytes);
    }

    inline void* memory_calloc(size_t count, size_t size_bytes)
    {
        return calloc(count, size_bytes);
    }

    inline void* memory_realloc(void* mem, size_t size_bytes)
    {
        return realloc(mem, size_bytes);
    }

    inline void memory_free(void* mem)
    {
        free(mem);
    }

    inline void memory_zero(void* dest, size_t size_bytes)
    {
        memset(dest, 0x00, size_bytes);
    }

    inline void* memory_alloc_align(size_t size_bytes, size_t alignment)
    {
        void* mem;
        PEN_MEM_ALIGN_ALLOC(mem, alignment, size_bytes);

        return mem;
    }

    inline void memory_free_align(void* mem)
    {
        PEN_MEM_ALIGN_FREE(mem);
    }
} // namespace pen

// And override global new and delete

void* operator new(std::size_t size, const std::nothrow_t& nothrow_value) THROW_NO_EXCEPT;
void* operator new(size_t n) THROW_BAD_ALLOC;
void* operator new[](size_t n) THROW_BAD_ALLOC;
void  operator delete[](void* p) THROW_NO_EXCEPT;
void  operator delete(void* p)THROW_NO_EXCEPT;
