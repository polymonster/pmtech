#include "timer.h"
#include "console.h"
#include "data_struct.h"
#include "pen_string.h"

namespace pen
{
    struct timer
    {
        LARGE_INTEGER last_start;

        f32 accumulated;
        f32 longest;
        f32 shortest;

        u32 hit_count;

        const c8* name;
    };

    LARGE_INTEGER performance_frequency;
    f32           ticks_to_ms;
    f32           ticks_to_us;
    f32           ticks_to_ns;
    timer*        timers;

    u32 next_free = 0;

    void timer_system_intialise()
    {
        QueryPerformanceFrequency(&performance_frequency);

        ticks_to_ms = (f32)(1.0 / (performance_frequency.QuadPart / 1000.0));
        ticks_to_us = ticks_to_ms * 1000.0f;
        ticks_to_ns = ticks_to_us * 1000.0f;

        next_free = 0;
    }

    u32 timer_create(const c8* name)
    {
        u32 index = sb_count(timers);

        timer t;
        t.name = name;

        sb_push(timers, t);

        return index;
    }

    void timer_start(u32 index)
    {
        QueryPerformanceCounter(&timers[index].last_start);
    }

    f32 timer_elapsed_ms(u32 timer_index)
    {
        LARGE_INTEGER end_time;
        QueryPerformanceCounter(&end_time);
        f32 last_duration = (f32)(end_time.QuadPart - timers[timer_index].last_start.QuadPart);

        return last_duration * ticks_to_ms;
    }

    f32 timer_elapsed_us(u32 timer_index)
    {
        LARGE_INTEGER end_time;
        QueryPerformanceCounter(&end_time);
        f32 last_duration = (f32)(end_time.QuadPart - timers[timer_index].last_start.QuadPart);

        return last_duration * ticks_to_us;
    }

    f32 timer_elapsed_ns(u32 timer_index)
    {
        LARGE_INTEGER end_time;
        QueryPerformanceCounter(&end_time);
        f32 last_duration = (f32)(end_time.QuadPart - timers[timer_index].last_start.QuadPart);

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
