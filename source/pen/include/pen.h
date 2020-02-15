// pen.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#ifndef _pen_h
#define _pen_h

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

    struct user_info
    {
        const c8* user_name = nullptr;
        const c8* full_user_name = nullptr;
        const c8* working_directory = nullptr;
    };
    
    namespace e_pmtech_create_flags
    {
        enum pmtech_create_flags_t
        {
            renderer = 1<<1,
            physics = 1<<2,
            audio = 1<<3
        };
    }
    typedef e_pmtech_create_flags::pmtech_create_flags_t pmtech_create_flags;
    
    struct pen_creation_params
    {
        volatile u32        window_width;
        volatile u32        window_height;
        u32                 window_sample_count;
        const c8*           window_title;
        pmtech_create_flags flags;
        PEN_TRV             (*user_thread_function)(void* params);
    };

#if PEN_ENTRY_FUNCTION
    extern pen_creation_params pen_entry(int argc, char** argv);
#else
    extern PEN_TRV user_entry(void* params);
#endif
    
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
#endif
