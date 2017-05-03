#ifndef _DEV_UI_H
#define _DEV_UI_H

#include "imgui.h"
#include "imgui_internal.h"
#include "pen.h"

namespace dev_ui
{
    bool init();
    void shutdown();
    void new_frame();
}

namespace put
{
    const c8* file_browser( bool& dialog_open, s32 num_filetypes = 0, ... );
}

#endif
