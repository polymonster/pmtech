#include "memory.h"

//C++ standard says these must be in cpp file no inline in header.

using namespace pen;

void* operator new(size_t n) throw(BAD_ALLOC)
{
    return memory_alloc(n);
}

void operator delete(void *p) throw()
{
    memory_free(p);
}

void* operator new[](size_t n) throw(BAD_ALLOC)
{
	return memory_alloc(n);
}
void operator delete[](void *p) throw()
{
	memory_free(p);
}
