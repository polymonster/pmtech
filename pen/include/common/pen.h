#ifndef _pen_h
#define _pen_h

#include "definitions.h"
#include "renderer.h"
#include "memory.h"
#include "pen_string.h"
#include "threads.h"

namespace pen
{
	struct window_creation_params
	{
		u32 width;
		u32 height;
		u32 sample_count;
		const c8* window_title;
	};

	extern PEN_THREAD_RETURN game_entry( void* params );
}

#endif

