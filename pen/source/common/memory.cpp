#include "memory.h"

void* operator new(size_t n) throw(BAD_ALLOC)
{
    return pen::memory_alloc( n );
}

void operator delete(void *p) throw()
{
    pen::memory_free( p );
}
