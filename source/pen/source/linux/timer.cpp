// timer.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "timer.h"
#include "console.h"
#include "data_struct.h"
#include "pen_string.h"

#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

namespace
{
    f32 ticks_to_ms;
    f32 ticks_to_us;
    f32 ticks_to_ns;
}

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

    s32 get_absolute_time()
    {
        static struct timeval now;
        gettimeofday(&now, nullptr);

        return now.tv_usec;
    }

    void timer_system_intialise()
    {
        ticks_to_ns = 1000.0f;
        ticks_to_us = 1;
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
        t->last_start = get_absolute_time();
    }

    f32 timer_elapsed_ms(timer* t)
    {
        uint64_t mt = get_absolute_time() - t->last_start;
        return mt * ticks_to_ms;
    }

    f32 timer_elapsed_us(timer* t)
    {
        uint64_t mt = get_absolute_time() - t->last_start;
        return mt * ticks_to_us;
    }

    f32 timer_elapsed_ns(timer* t)
    {
        uint64_t mt = get_absolute_time() - t->last_start;
        return mt * ticks_to_ns;
    }

    f32 get_time_ms()
    {
        uint64_t t = get_absolute_time();

        return (f32)(t)*ticks_to_ms;
    }

    f32 get_time_us()
    {
        uint64_t t = get_absolute_time();

        return (f32)(t)*ticks_to_us;
    }

    f32 get_time_ns()
    {
        uint64_t t = get_absolute_time();

        return (f32)(t)*ticks_to_ns;
    }
} // namespace pen
