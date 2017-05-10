#ifndef _DEV_UI_H
#define _DEV_UI_H

#include "imgui.h"
#include "imgui_internal.h"
#include "pen.h"

namespace put
{
	namespace dev_ui
	{
		//imgui_renderer
		bool init();
		void shutdown();
		void new_frame();
		void render();

		//ui utilities
		const c8* file_browser(bool& dialog_open, s32 num_filetypes = 0, ...);
	}
}

#endif
