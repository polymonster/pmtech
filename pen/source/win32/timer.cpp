#include "timer.h"
#include "pen_string.h"

namespace pen
{
#define MAX_TIMERS 100

	typedef struct timer
	{
		LARGE_INTEGER last_start;

		f32 accumulated;
		f32 longest;
		f32 shortest;

		u32	hit_count;

		const c8* name;

	} timer;

	LARGE_INTEGER permonace_frequency;
	f32			  ticks_to_ms;
	timer		  timers[ MAX_TIMERS ];

	u32 next_free = 0;

	void timer_system_intialise( )
	{
		QueryPerformanceFrequency( &permonace_frequency );

		ticks_to_ms = (f32)( 1.0 / ( permonace_frequency.QuadPart / 1000.0 ) );

		next_free = MAX_TIMERS;

		timer_reset_all( );

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
		QueryPerformanceCounter( &timers[ index ].last_start );
	}

	void timer_accum( u32 index )
	{
		LARGE_INTEGER end_time;
		QueryPerformanceCounter( &end_time );

		f32 last_duration = (f32)(end_time.QuadPart - timers[ index ].last_start.QuadPart);

		timers[ index ].accumulated += last_duration;
		timers[ index ].hit_count++;
		timers[ index ].shortest = last_duration < timers[ index ].shortest ? last_duration : timers[ index ].shortest;
		timers[ index ].longest = last_duration > timers[ index ].longest ? last_duration : timers[ index ].longest;
	}

	void timer_reset( u32 timer_index )
	{
		timers[timer_index].accumulated = 0;
		timers[timer_index].hit_count = 0;
		timers[timer_index].longest = 0;
		timers[timer_index].shortest = PEN_F32_MAX;
	}

	void timer_reset_all( )
	{
		for( u32 i = 0; i < next_free; ++i )
		{
			timers[ i ].accumulated = 0;
			timers[ i ].hit_count = 0;
			timers[ i ].longest = 0;
			timers[ i ].shortest = PEN_F32_MAX;
		}
	}

	f32 timer_get_ms( u32 timer_index )
	{
		return timers[ timer_index ].accumulated * ticks_to_ms;
	}

	void timer_get_data( u32 timer_index, f32 &total_time_ms, u32 &hit_count, f32 &longest_ms, f32 &shortest_ms )
	{
		total_time_ms = timers[ timer_index ].accumulated * ticks_to_ms;
		longest_ms = timers[ timer_index ].longest * ticks_to_ms;
		shortest_ms = timers[ timer_index ].shortest * ticks_to_ms;
		hit_count = timers[ timer_index ].hit_count;
	}

	f32 timer_get_time( )
	{
		LARGE_INTEGER perf;
		QueryPerformanceCounter( &perf );

		return (f32)( perf.QuadPart ) * ticks_to_ms;
	}

	void timer_print( u32 timer_index )
	{
		f32 total_time_ms = timers[ timer_index ].accumulated * ticks_to_ms;
		PEN_PRINTF( "timer %s = %f\n", timers[ timer_index ].name, total_time_ms );
	}
}