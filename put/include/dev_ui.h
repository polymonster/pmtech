#ifndef _DEV_UI_H
#define _DEV_UI_H

#include "dev_ui_icons.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "pen.h"
#include "renderer.h"
#include "pen_json.h"
#include "vec.h"

#define IMG(I) (void*)(intptr_t)I

namespace put
{
    namespace dev_ui
    {
        enum io_capture : u32
        {
            NO_INPUT = 0,
            MOUSE    = 1 << 0,
            KEYBOARD = 1 << 1,
            TEXT     = 1 << 2
        };

        enum e_file_browser_flags : u32
        {
            FB_OPEN = 0,
            FB_SAVE = 1,
        };

        enum e_console_log_level : u32
        {
            CONSOLE_MESSAGE = 0,
            CONSOLE_WARNING = 1,
            CONSOLE_ERROR   = 2
        };

        enum e_shader : u32
        {
            SHADER_DEFAULT = pen::TEXTURE_COLLECTION_NONE,
            SHADER_CUBEMAP = pen::TEXTURE_COLLECTION_CUBE,
            SHADER_VOLUME_TEXTURE = pen::TEXTURE_COLLECTION_VOLUME,
            SHADER_TEXTURE_ARRAY = pen::TEXTURE_COLLECTION_ARRAY
        };

        // imgui_renderer
        bool init();
        void shutdown();
        void new_frame();
        void render();
        u32  want_capture();
        void set_shader(e_shader shader, u32 cbuffer);

        void util_init();
        void update();

        // console
        void show_console(bool val);
        void log(const c8* fmt, ...);
        void log_level(u32 level, const c8* fmt, ...);
        void console();

        // imgui extensions
        bool      state_button(const c8* text, bool state_active);
        void      set_tooltip(const c8* fmt, ...);
        const c8* file_browser(bool& dialog_open, u32 flags, s32 num_filetypes = 0, ...);
        void      show_platform_info();
        void      image(u32 handle, vec2f size, s32 mip_level = -1);
        void      image_ex(u32 handle, vec2f size, e_shader shader, s32 mip_level = -1);
        
        // generic program preferences
        void      set_program_preference(const c8* name, f32 val);
        void      set_program_preference(const c8* name, s32 val);
        void      set_program_preference(const c8* name, bool val);
        void      set_program_preference(const c8* name, Str val);
        pen::json get_program_preference(const c8* name);
        Str       get_program_preference_filename(const c8* name, const c8* default_value = nullptr);
        void      set_program_preference_filename(const c8* name, Str val);
    } // namespace dev_ui
} // namespace put

#define dev_console_log_level(level, fmt, ...) put::dev_ui::log_level(level, fmt, ##__VA_ARGS__)
#define dev_console_log(fmt, ...) put::dev_ui::log(fmt, ##__VA_ARGS__)

#endif
