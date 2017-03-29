#ifndef _memory_h
#define _memory_h

#include "definitions.h"

namespace pen
{
	//c 
	void*	memory_alloc( u32 size_bytes );
	void*	memory_alloc_align( u32 size_bytes, u32 alignment );
	void	memory_free( void* mem );
	void	memory_free_align( void* mem );
	void	memory_cpy( void* dest, const void* src, u32 size_bytes );
	void	memory_set( void* dest, u8 val, u32 size_bytes );
	void	memory_zero( void* dest, u32 size_bytes );
}

void*	operator new(size_t n) throw(std::bad_alloc);
void	operator delete(void *p) throw();

#endif
