// renderer_vulkan.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

// API for shared renderer functionality across multiple rendering backends.
// all functions to be called from the render thread

#include "renderer.h"

namespace pen
{
    void                    _renderer_new_frame();
    void                    _renderer_end_frame();
    texture_creation_params _renderer_tcp_resolve_ratio(const texture_creation_params& tcp);
    void                    _renderer_track_managed_render_target(const texture_creation_params& tcp, u32 texture_handle);
    void                    _renderer_resize_backbuffer(u32 width, u32 height);
    void                    _renderer_resize_managed_targets();
    u64                     _renderer_frame_index();
    u64                     _renderer_resize_index();
    
    // void renderer_create_resolve_resources();
    // dynamic buffer
}
