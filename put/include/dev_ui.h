#ifndef _DEV_UI_H
#define _DEV_UI_H

#include <imgui.h>

namespace dev_ui
{
    bool init();
    void shutdown();

    void new_frame();
}

#endif