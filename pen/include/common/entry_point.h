#ifndef _window_h
#define _window_h

#include "definitions.h"

namespace pen
{
	//os, window stuff
	u32 window_init( void* params );
	void* window_get_primary_display_handle();
    
    void os_set_cursor_pos( u32 client_x, u32 client_y );
    void os_show_cursor( bool show );
}


#endif
