// timer.h
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

// Barebones high resolution c-style timer api, plus a scope timer for profiling.

#pragma once

#include "console.h"
#include "pen.h"

namespace pen
{
    struct timer;

    void   timer_system_intialise(); // query performance frequency, setup ticks to ms, us conversions.
    timer* timer_create();
    void   timer_destroy(timer* t);
    void   timer_start(timer* t);      // set timer start, elapsed = 0
    f64    timer_elapsed_ms(timer* t); // ms since last call to start
    f64    timer_elapsed_us(timer* t); // us since last call to start
    f64    timer_elapsed_ns(timer* t); // ns since last call to start
    f64    get_time_ms();
    f64    get_time_us();
    f64    get_time_ns();

    class scope_timer
    {
      public:
        scope_timer(const c8* name, bool print)
        {
            this->name = name;
            this->print = print;
            start = get_time_ns();
        }

        ~scope_timer()
        {
            f64 end = get_time_ns();
            if (print)
                PEN_LOG("%s: %f(us)", this->name, (end - start) / 1000.0);
        }

      private:
        const c8* name;
        f64       start;
        bool      print;
    };

} // namespace pen

#define PEN_PERF_SCOPE_PRINT(name) pen::scope_timer name = pen::scope_timer(#name, true)
