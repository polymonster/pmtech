#include "memory.h"

namespace pen
{

}

void* operator new(u32 n) throw(BAD_ALLOC)
{
    return pen::memory_alloc( n );
}

void operator delete(void *p) throw()
{
    pen::memory_free( p );
}