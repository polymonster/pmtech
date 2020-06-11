// timer.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "timer.h"
#include "console.h"
#include "data_struct.h"
#include "pen_string.h"

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <unistd.h>

namespace
{
    f32 ticks_to_ms;
    f32 ticks_to_us;
    f32 ticks_to_ns;
} // namespace

namespace pen
{
    struct timer
    {
        uint64_t last_start;

        f32 accumulated;
        f32 longest;
        f32 shortest;

        u32 hit_count;

        const c8* name;
    };

    void timer_system_intialise()
    {
        static mach_timebase_info_data_t s_timebase_info;

        if (s_timebase_info.denom == 0)
            (void)mach_timebase_info(&s_timebase_info);

        ticks_to_ns = (f32)s_timebase_info.numer / (f32)s_timebase_info.denom;
        ticks_to_us = ticks_to_ns / 1000.0f;
        ticks_to_ms = ticks_to_us / 1000.0f;
    }

    timer* timer_create()
    {
        return (timer*)memory_alloc(sizeof(timer));
    }

    void timer_destroy(timer* t)
    {
        memory_free(t);
    }

    void timer_start(timer* t)
    {
        t->last_start = mach_absolute_time();
    }

    f32 timer_elapsed_ms(timer* t)
    {
        uint64_t mt = mach_absolute_time() - t->last_start;
        return (f32)mt * ticks_to_ms;
    }

    f32 timer_elapsed_us(timer* t)
    {
        uint64_t mt = mach_absolute_time() - t->last_start;
        return (f32)mt * ticks_to_us;
    }

    f32 timer_elapsed_ns(timer* t)
    {
        uint64_t mt = mach_absolute_time() - t->last_start;
        return (f32)mt * ticks_to_ns;
    }

    f32 get_time_ms()
    {
        uint64_t t = mach_absolute_time();
        return (f32)(t)*ticks_to_ms;
    }

    f32 get_time_us()
    {
        uint64_t t = mach_absolute_time();
        return (f32)(t)*ticks_to_us;
    }

    f32 get_time_ns()
    {
        uint64_t t = mach_absolute_time();
        return (f32)(t)*ticks_to_ns;
    }
} // namespace pen
