#ifndef _timer_h
#define _timer_h

#include "pen.h"

namespace pen
{
	void  timer_system_intialise();
	
	u32	  timer_create( const c8* name );
    void  timer_start( u32 index );
    
    f32	  get_time_ms( );
    f32   get_time_us( );
    f32   get_time_ns( );
    
	f32	  timer_elapsed_ms( u32 timer_index );
    f32	  timer_elapsed_us( u32 timer_index );
    f32	  timer_elapsed_ns( u32 timer_index );
}

#endif

