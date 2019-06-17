// timer.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

// Barebones high resolution c-style timer api.

#include "pen.h"

namespace pen
{
    struct timer;

    void    timer_system_intialise();       // query performance frequency, setup ticks to ms, us conversions.
    timer*  timer_create();
    void    timer_destroy(timer* t);
    void    timer_start(timer* t);          // set timer start, elapsed = 0
    f32     timer_elapsed_ms(timer* t);     // ms since last call to start
    f32     timer_elapsed_us(timer* t);     // us since last call to start
    f32     timer_elapsed_ns(timer* t);     // ns since last call to start
    f32     get_time_ms();
    f32     get_time_us();
    f32     get_time_ns();
} // namespace pen
