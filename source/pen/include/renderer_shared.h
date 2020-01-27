// renderer_vulkan.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

// API for shared renderer functionality across multiple rendering backends.
// all functions to be called from the render thread

#include "renderer.h"

namespace pen
{
    texture_creation_params renderer_tcp_resolve_ratio(const texture_creation_params& tcp);
    void                    renderer_track_managed_render_target(const texture_creation_params& tcp, u32 texture_handle);
    
    // void renderer_create_resolve_resources();
    // dynamic buffer
}
