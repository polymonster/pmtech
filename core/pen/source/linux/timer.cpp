// timer.cpp
// Copyright 2014 - 2023 Alex Dixon.
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
    f64 ticks_to_ms;
    f64 ticks_to_us;
    f64 ticks_to_ns;
} // namespace

namespace pen
{
    struct timer
    {
        uint64_t last_start;

        f64 accumulated;
        f64 longest;
        f64 shortest;

        u32 hit_count;

        const c8* name;
    };

    u64 get_absolute_time()
    {
        static struct timeval tv;
        gettimeofday(&tv, nullptr);
        return (tv.tv_sec * 1000 * 1000) + (tv.tv_usec);
    }

    void timer_system_intialise()
    {
        ticks_to_ns = 1000.0;
        ticks_to_us = 1;
        ticks_to_ms = ticks_to_us / 1000.0;
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

    f64 timer_elapsed_ms(timer* t)
    {
        uint64_t mt = get_absolute_time() - t->last_start;
        return (f64)mt * ticks_to_ms;
    }

    f64 timer_elapsed_us(timer* t)
    {
        uint64_t mt = get_absolute_time() - t->last_start;
        return (f64)mt * ticks_to_us;
    }

    f64 timer_elapsed_ns(timer* t)
    {
        uint64_t mt = get_absolute_time() - t->last_start;
        return (f64)mt * ticks_to_ns;
    }

    f64 get_time_ms()
    {
        uint64_t t = get_absolute_time();
        return (f64)(t)*ticks_to_ms;
    }

    f64 get_time_us()
    {
        uint64_t t = get_absolute_time();
        return (f64)(t)*ticks_to_us;
    }

    f64 get_time_ns()
    {
        uint64_t t = get_absolute_time();
        return (f64)(t)*ticks_to_ns;
    }
} // namespace pen
