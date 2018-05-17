#include "timer.h"
#include "console.h"
#include "data_struct.h"
#include "pen_string.h"

#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

namespace pen
{
#define MAX_TIMERS 100
    struct timer
    {
        uint64_t last_start;

        f32 accumulated;
        f32 longest;
        f32 shortest;

        u32 hit_count;

        const c8* name;
    };

    f32 ticks_to_ms;
    f32 ticks_to_us;
    f32 ticks_to_ns;

    // fixed array.. todo sort this out
    timer* timers = nullptr;
    int* test;

    u32 next_free = 0;

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

        next_free = 0;
    }

    u32 timer_create(const c8* name)
    {
        timer t;
        t.name = name;

        u32 index = sb_count(timers);
        sb_push(timers, t);

        return index;
    }

    void timer_start(u32 index)
    {
        timers[index].last_start = get_absolute_time();
    }

    f32 timer_elapsed_ms(u32 timer_index)
    {
        uint64_t t = get_absolute_time() - timers[timer_index].last_start;
        return t * ticks_to_ms;
    }

    f32 timer_elapsed_us(u32 timer_index)
    {
        uint64_t t = get_absolute_time() - timers[timer_index].last_start;
        return t * ticks_to_us;
    }

    f32 timer_elapsed_ns(u32 timer_index)
    {
        uint64_t t = get_absolute_time() - timers[timer_index].last_start;
        return t * ticks_to_ns;
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
