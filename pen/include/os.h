#ifndef _os_h
#define _os_h

#include "pen.h"

namespace pen
{
    // Tiny api with some window and os specific interactions

    // Window
    u32   window_init(void* params);
    void* window_get_primary_display_handle();
    void  window_get_size(s32& width, s32& height);

    // os
    bool os_update();
    void os_set_cursor_pos(u32 client_x, u32 client_y);
    void os_show_cursor(bool show);
    const c8* os_path_for_resource(const c8* filename);
    
} // namespace pen

#endif
