// os.h
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

// Tiny api with some window and os api specific abstractions.

#pragma once

#include "pen.h"
#include "str_utilities.h"

namespace pen
{
    struct window_frame
    {
        u32 x, y, width, height;
    };

    struct user_info
    {
        const c8* user_name = nullptr;
        const c8* full_user_name = nullptr;
        const c8* working_directory = nullptr;
    };

    // window
    u32       window_init(void* params);
    void*     window_get_primary_display_handle();
    void      window_get_frame(window_frame& f);
    void      window_set_frame(const window_frame& f);
    void      window_get_size(s32& width, s32& height);
    void      window_set_size(s32 width, s32 height);
    f32       window_get_aspect();
    const c8* window_get_title();
    hash_id   window_get_id();

    // generic os
    void             os_terminate(u32 return_code);
    bool             os_update();
    void             os_set_cursor_pos(u32 client_x, u32 client_y);
    void             os_show_cursor(bool show);
    const Str        os_path_for_resource(const c8* filename);
    const user_info& os_get_user_info();
    Str              os_get_persistent_data_directory();
    Str              os_get_cache_data_directory();
    void             os_create_directory(const Str& dir);
    void             os_open_url(const Str& url);
    void             os_ignore_slient();
    f32              os_get_status_bar_portrait_height();
    void             os_haptic_selection_feedback();

    // music
    struct music_item
    {
        Str   artist;
        Str   album;
        Str   track;
        f64   duration;
        void* internal;
    };

    struct music_file
    {
        f32*   pcm_data;     // interleaved stereo or mono channel pcm
        size_t len;          // length in bytes
        u32    num_channels; // indicates mono(1) or stereo(2)
        f64    sample_frequency;
    };

    const music_item* music_get_items(); // returns stretchy buffer use sb_count for num items
    music_file        music_open_file(const music_item& item);
    void              music_close_file(const music_file& file);

} // namespace pen
