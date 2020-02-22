// timer.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "timer.h"
#include "console.h"
#include "data_struct.h"
#include "pen_string.h"

namespace
{
    LARGE_INTEGER performance_frequency;
    f32           ticks_to_ms;
    f32           ticks_to_us;
    f32           ticks_to_ns;
} // namespace

namespace pen
{
    struct timer
    {
        LARGE_INTEGER last_start;
        f32           accumulated;
        f32           longest;
        f32           shortest;
        u32           hit_count;
        const c8*     name;
    };

    void timer_system_intialise()
    {
        QueryPerformanceFrequency(&performance_frequency);

        ticks_to_ms = (f32)(1.0 / (performance_frequency.QuadPart / 1000.0));
        ticks_to_us = ticks_to_ms * 1000.0f;
        ticks_to_ns = ticks_to_us * 1000.0f;
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
        QueryPerformanceCounter(&t->last_start);
    }

    f32 timer_elapsed_ms(timer* t)
    {
        LARGE_INTEGER end_time;
        QueryPerformanceCounter(&end_time);
        f32 last_duration = (f32)(end_time.QuadPart - t->last_start.QuadPart);

        return last_duration * ticks_to_ms;
    }

    f32 timer_elapsed_us(timer* t)
    {
        LARGE_INTEGER end_time;
        QueryPerformanceCounter(&end_time);
        f32 last_duration = (f32)(end_time.QuadPart - t->last_start.QuadPart);

        return last_duration * ticks_to_us;
    }

    f32 timer_elapsed_ns(timer* t)
    {
        LARGE_INTEGER end_time;
        QueryPerformanceCounter(&end_time);
        f32 last_duration = (f32)(end_time.QuadPart - t->last_start.QuadPart);

        return last_duration * ticks_to_ns;
    }

    f32 get_time_ms()
    {
        LARGE_INTEGER perf;
        QueryPerformanceCounter(&perf);

        return (f32)(perf.QuadPart) * ticks_to_ms;
    }

    f32 get_time_us()
    {
        LARGE_INTEGER perf;
        QueryPerformanceCounter(&perf);

        return (f32)(perf.QuadPart) * ticks_to_us;
    }

    f32 get_time_ns()
    {
        LARGE_INTEGER perf;
        QueryPerformanceCounter(&perf);

        return (f32)(perf.QuadPart) * ticks_to_ns;
    }
} // namespace pen
