#ifndef _os_h
#define _os_h

// Tiny api with some window and os specific abstractions.

#include "pen.h"

namespace pen
{
    struct window_frame
    {
        u32 x, y, width, height;
    };

    // Window

    u32   window_init(void* params);
    void* window_get_primary_display_handle();
    void  window_get_frame(window_frame& f);
    void  window_set_frame(const window_frame& f);
    void  window_get_size(s32& width, s32& height);
    void  window_set_size(s32 width, s32 height);

    // OS

    bool      os_update();
    void      os_set_cursor_pos(u32 client_x, u32 client_y);
    void      os_show_cursor(bool show);
    const c8* os_path_for_resource(const c8* filename);

} // namespace pen

#endif
