#ifndef _pen_h
#define _pen_h

#include "types.h"

namespace pen
{
    // Currently fixed buffer sizes for the number of multi threaded resources

    enum resource_types
    {
        MAX_RENDERER_RESOURCES = 10000,
        MAX_AUDIO_RESOURCES    = 100
    };

    // Structs for user to setup, make sure to define:
    // window_creation_params pen_window
    // and user_entry function somwhere and then you are good to go

    struct window_creation_params
    {
        u32       width;
        u32       height;
        u32       sample_count;
        const c8* window_title;
    };

    struct user_info
    {
        const c8* user_name;
        const c8* full_user_name;
    };

    extern PEN_TRV user_entry(void* params);
} // namespace pen

#endif
