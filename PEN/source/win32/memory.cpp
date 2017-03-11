#include "memory.h"
#include <stdlib.h>
#include <stdio.h>

namespace pen
{
	void* memory_alloc( u32 size_bytes )
	{
		return malloc( size_bytes );
	}

	void memory_free( void* mem )
	{
		free( mem );
	}

	void memory_zero( void* dest, u32 size_bytes )
	{
		memory_set( dest, 0x00, size_bytes );
	}

	void memory_set( void* dest, u8 val, u32 size_bytes )
	{
		memset( dest, val, size_bytes );
	}

	void memory_cpy( void* dest, const void* src, u32 size_bytes )
	{
		memcpy( dest, src, size_bytes );
	}

	void* memory_alloc_align( u32 size_bytes, u32 alignment )
	{
		return _aligned_malloc( size_bytes, alignment );
	}

	void memory_free_align( void* mem )
	{
		_aligned_free( mem );
	}
}

void*	operator new(u32 n)
{
	return pen::memory_alloc( n );
}

void	operator delete(void *p)
{
	pen::memory_free( p );
}