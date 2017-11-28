#ifndef _DEV_UI_H
#define _DEV_UI_H

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "pen.h"
#include "dev_ui_icons.h"
#include "pen_json.h"

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
        
        enum e_file_browser_flags : u32
        {
            FB_OPEN = 0,
            FB_SAVE = 1,
        };
        
		//imgui_renderer
		bool init();
		void shutdown();
		void new_frame();
		void render();
        
        void util_init();
        void set_program_preference( const c8* name, Str val );
        pen::json get_program_preference( const c8* name );
        
        u32 want_capture();

		//ui utilities
		const c8* file_browser(bool& dialog_open, u32 flags, s32 num_filetypes = 0, ...);
	}
}

#endif
