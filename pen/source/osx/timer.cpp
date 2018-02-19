#include "timer.h"
#include "pen_string.h"
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <unistd.h>

namespace pen
{
#define MAX_TIMERS 100
	typedef struct timer
	{
        uint64_t last_start;

		f32 accumulated;
		f32 longest;
		f32 shortest;

		u32	hit_count;

		const c8* name;

	} timer;

	//LARGE_INTEGER permonace_frequency;
	f32			  ticks_to_ms;
    f32			  ticks_to_us;
    f32			  ticks_to_ns;
    
    //fixed array.. todo sort this out
	timer		  timers[ MAX_TIMERS ];

	u32 next_free = 0;

	void timer_system_intialise( )
	{
        static mach_timebase_info_data_t s_timebase_info;
        
        if ( s_timebase_info.denom == 0 )
        {
            (void) mach_timebase_info(&s_timebase_info);
        }
        
        ticks_to_ns = (f32)s_timebase_info.numer / (f32)s_timebase_info.denom;
        ticks_to_us = ticks_to_ns / 1000.0f;
        ticks_to_ms = ticks_to_us / 1000.0f;

		next_free = 0;
	}

	u32 timer_create( const c8* name )
	{
		timers[ next_free ].name = name;
		PEN_ASSERT( next_free < MAX_TIMERS );
		return next_free++;
	}

	void timer_start( u32 index )
	{
        timers[ index ].last_start = mach_absolute_time();
	}
    
	f32 timer_elapsed_ms( u32 timer_index )
	{
        uint64_t t = mach_absolute_time() - timers[ timer_index ].last_start;
		return t * ticks_to_ms;
	}
    
    f32 timer_elapsed_us( u32 timer_index )
    {
        uint64_t t = mach_absolute_time() - timers[ timer_index ].last_start;
        return t * ticks_to_us;
    }
    
    f32 timer_elapsed_ns( u32 timer_index )
    {
        uint64_t t = mach_absolute_time() - timers[ timer_index ].last_start;
        return t * ticks_to_ns;
    }

	f32 get_time_ms( )
	{
        uint64_t t = mach_absolute_time();
        
        return (f32)( t ) * ticks_to_ms;
	}
    
    f32 get_time_us( )
    {
        uint64_t t = mach_absolute_time();
        
        return (f32)( t ) * ticks_to_us;
    }
    
    f32 get_time_ns( )
    {
        uint64_t t = mach_absolute_time();
        
        return (f32)( t ) * ticks_to_ns;
    }
}
