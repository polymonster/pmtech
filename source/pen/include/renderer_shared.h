// renderer_vulkan.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#pragma once

// API for shared renderer functionality across multiple rendering backends.
// all functions must be called on the render thread and inside the rendering backend implementation.

#include "renderer.h"

namespace pen
{
    struct stretchy_dynamic_buffer
    {
        size_t _cpu_capacity = 0;
        size_t _gpu_capacity = 0;
        size_t _write_offset = 0;
        size_t _read_offset = 0;
        size_t _alignment = 1;
        
        u8* _cpu_data = nullptr;
        u32 _gpu_buffer;
    };

	namespace e_shared_flags
	{
		enum shared_flags_t
		{
			backbuffer_resize = 1
		};
	}
	typedef u32 shared_flags;
    
    void                        _renderer_shared_init();
    void                        _renderer_new_frame();
    void                        _renderer_end_frame();
    texture_creation_params     _renderer_tcp_resolve_ratio(const texture_creation_params& tcp);
    void                        _renderer_track_managed_render_target(const texture_creation_params& tcp, u32 texture_handle);
    void                        _renderer_resize_backbuffer(u32 width, u32 height);
    void                        _renderer_resize_managed_targets();
    u64                         _renderer_frame_index();
    u64                         _renderer_resize_index();
	shared_flags				_renderer_flags();
    
    void                        _renderer_commit_stretchy_dynamic_buffers();
    size_t                      _renderer_buffer_multi_update(stretchy_dynamic_buffer* buf, const void* data, size_t size);
    stretchy_dynamic_buffer*    _renderer_get_stretchy_dynamic_buffer(u32 bind_flags);
}
