#ifndef _DEV_UI_H
#define _DEV_UI_H

#include "imgui.h"
#include "imgui_internal.h"
#include "pen.h"

namespace put
{
	namespace dev_ui
	{
        enum io_capture : u32
        {
            MOUSE      = 1<<0,
            KAYBOARD   = 1<<1,
            TEXT       = 1<<2
        };
        
		//imgui_renderer
		bool init();
		void shutdown();
		void new_frame();
		void render();
        
        u32 want_capture();

		//ui utilities
		const c8* file_browser(bool& dialog_open, s32 num_filetypes = 0, ...);
	}
}

#endif
