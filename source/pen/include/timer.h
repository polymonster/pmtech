#ifndef _timer_h
#define _timer_h

// Barebones high resolution c-style timer api.

#include "pen.h"

namespace pen
{
    void timer_system_intialise();     // query performance frequency, setup ticks to ms, us conversions.
    u32  timer_create(const c8* name); // retruns timer handle in u32.
    void timer_start(u32 index);

    f32  timer_elapsed_ms(u32 timer_index); // ms since last call to start
    f32  timer_elapsed_us(u32 timer_index); // us since last call to start
    f32  timer_elapsed_ns(u32 timer_index); // ns since last call to start

    f32 get_time_ms();
    f32 get_time_us();
    f32 get_time_ns();
} // namespace pen

#endif
