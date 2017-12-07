#ifndef _pen_h
#define _pen_h

#include "definitions.h"

namespace pen
{
	struct window_creation_params
	{
		u32 width;
		u32 height;
		u32 sample_count;
		const c8* window_title;
	};
    
    struct user_info
    {
        const c8* user_name;
        const c8* full_user_name;
    };

	extern PEN_THREAD_RETURN game_entry( void* params );
}

#endif

