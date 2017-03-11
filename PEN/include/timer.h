#ifndef _timer_h
#define _timer_h

#include "definitions.h"

namespace pen
{
	void  timer_system_intialise();
	
	u32	  timer_create( const c8* name );

	f32	  timer_get_time( );

	f32	  timer_get_ms( u32 timer_index );

	void  timer_start( u32 index );
	void  timer_accum( u32 index );
	
	void  timer_reset( u32 timer_index );
	void  timer_reset_all( );

	void  timer_get_data( u32 timer_index, f32 &total_time_ms, u32 &hit_count, f32 &longest_ms, f32 &shortest_ms );
	void  timer_print( u32 timer_index );
}

#define PEN_TIMER_START( name ) static u32 name##_timer_index = (u32)-1; \
								if( name##_timer_index == (u32)-1 ) { name##_timer_index = pen::timer_create( #name ); } \
								pen::timer_start( name##_timer_index )

#define PEN_TIMER_END( name )   pen::timer_accum( name##_timer_index )

#define PEN_TIMER_PRINT( name ) pen::timer_print( name##_timer_index )

#define PEN_TIMER_RESET( name ) pen::timer_reset( name##_timer_index )

#define PEN_TIMER_GET_TIME( name ) pen::timer_get_ms( name##_timer_index )
#endif

