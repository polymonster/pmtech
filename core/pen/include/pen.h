// pen.h
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "types.h"

#if PEN_SINGLE_THREADED
#define pen_main_loop(function) pen::jobs_create_single_thread_update(function);
#define pen_main_loop_exit()
#define pen_main_loop_continue() return true
typedef bool loop_t;
#else
#define pen_main_loop(function)                                                                                              \
    for (;;)                                                                                                                 \
    {                                                                                                                        \
        if (!function())                                                                                                     \
            break;                                                                                                           \
    }
#define pen_main_loop_exit() return false;
#define pen_main_loop_continue() return true;
typedef bool loop_t;
#endif

namespace pen
{
    // window_creation_params is now being deprecated
    struct window_creation_params
    {
        volatile u32 width;
        volatile u32 height;
        u32          sample_count;
        const c8*    window_title;
    };

    namespace e_pen_create_flags
    {
        enum pen_create_flags_t
        {
            renderer = 1 << 1,
            console_app = 1 << 2
        };
    }
    typedef e_pen_create_flags::pen_create_flags_t pen_create_flags;

    struct pen_creation_params
    {
        volatile u32     window_width = 1280;
        volatile u32     window_height = 720;
        u32              window_sample_count = 1;
        const c8*        window_title = "pen_app";
        pen_create_flags flags = e_pen_create_flags::renderer;
        u32              max_renderer_commands = 1 << 16; // space for max commands in cmd buffer
        void* (*user_thread_function)(void*) = nullptr;
        void* user_data = nullptr;
    };

    extern pen_creation_params pen_entry(int argc, char** argv);
    void*                      user_entry(void* params);
} // namespace pen
