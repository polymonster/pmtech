// dev_ui.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

#include "dev_ui_icons.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "maths/vec.h"
#include "pen.h"
#include "pen_json.h"
#include "renderer.h"

#define IMG(I) (void*)(intptr_t) I

namespace put
{
    namespace dev_ui
    {
        namespace e_io_capture
        {
            enum io_capture_t
            {
                none,
                mouse = 1 << 0,
                keyboard = 1 << 1,
                text = 1 << 2
            };
        }
        typedef u32 io_capture;

        namespace e_file_browser_flags
        {
            enum file_browser_flags_t
            {
                open,
                save
            };
        }
        typedef u32 file_browser_flags;

        namespace e_console_level
        {
            enum console_level_t
            {
                message,
                warning,
                error
            };
        }
        typedef e_console_level::console_level_t console_level;

        namespace e_ui_shader
        {
            enum ui_shader_t
            {
                imgui = -1,
                texture_2d = pen::TEXTURE_COLLECTION_NONE,
                cubemap = pen::TEXTURE_COLLECTION_CUBE,
                volume_texture = pen::TEXTURE_COLLECTION_VOLUME,
                texture_array = pen::TEXTURE_COLLECTION_ARRAY,
                texture_cube_array = pen::TEXTURE_COLLECTION_CUBE_ARRAY
            };
        }
        typedef e_ui_shader::ui_shader_t ui_shader;

        bool        init();
        void        shutdown();
        void        render();
        void        update();
        void        new_frame();
        io_capture  want_capture();
        void        set_shader(ui_shader shader, u32 cbuffer);
        void        util_init();
        void        enable(bool enabled);
    
        // main menu bar
        void        enable_main_menu_bar(bool enable);
        bool        main_menu_bar_enabled();

        // console
        bool        is_console_open();
        void        show_console(bool val);
        void        log(const c8* fmt, ...);
        void        log_level(u32 level, const c8* fmt, ...);
        void        console();

        // imgui extensions
        bool        state_button(const c8* text, bool state_active);
        void        set_tooltip(const c8* fmt, ...);
        const c8*   file_browser(bool& dialog_open, file_browser_flags flags, s32 num_filetypes = 0, ...);
        void        show_platform_info();
        void        image_ex(u32 handle, vec2f size, ui_shader shader);

        // generic program preferences
        void        set_program_preference(const c8* name, f32 val);
        void        set_program_preference(const c8* name, s32 val);
        void        set_program_preference(const c8* name, bool val);
        void        set_program_preference(const c8* name, Str val);
        pen::json   get_program_preference(const c8* name);
        Str         get_program_preference_filename(const c8* name, const c8* default_value = nullptr);
        void        set_program_preference_filename(const c8* name, Str val);
    } // namespace dev_ui
} // namespace put

#define dev_console_log_level(level, fmt, ...) put::dev_ui::log_level(level, fmt, ##__VA_ARGS__)
#define dev_console_log(fmt, ...) put::dev_ui::log(fmt, ##__VA_ARGS__)
