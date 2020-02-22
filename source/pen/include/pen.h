// pen.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "types.h"

namespace pen
{
    // Structs for user to setup, make sure to define:
    // window_creation_params pen_window
    // and user_entry function somwhere and then you are good to go
    
    struct window_creation_params
    {
        volatile u32    width;
        volatile u32    height;
        u32             sample_count;
        const c8*       window_title;
    };
    
    namespace e_pen_create_flags
    {
        enum pen_create_flags_t
        {
            renderer = 1<<1,
			console_app = 1<<2
        };
    }
    typedef e_pen_create_flags::pen_create_flags_t pen_create_flags;
    
    struct pen_creation_params
    {
		volatile u32        window_width = 1280;
		volatile u32        window_height = 720;
		u32                 window_sample_count = 1;
		const c8*           window_title = "pen_app";
		pen_create_flags	flags = e_pen_create_flags::renderer;
		void*               (*user_thread_function)(void*) = nullptr;
    };

    extern pen_creation_params pen_entry(int argc, char** argv);
    extern void* user_entry(void* params);
    
} // namespace pen

// compiler
#ifdef _MSC_VER
#define pen_inline __forceinline
#define pen_deprecated __declspec(deprecated)
#define pen_debug_break __debug_break()
#else
#define pen_inline inline __attribute__((always_inline))
#define pen_deprecated __attribute__((deprecated))
#define pen_debug_break __builtin_trap()
#endif
#ifdef PEN_PLATFORM_IOS
#define PEN_HOTLOADING_ENABLED return
#else
#define PEN_HOTLOADING_ENABLED
#endif
