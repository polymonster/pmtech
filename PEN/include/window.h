#ifndef _window_h
#define _window_h

#include "definitions.h"

namespace pen
{
	u32 window_init( void* params );
	void* window_get_primary_display_handle();
}


#endif
