// renderer_vulkan.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

// API for shared renderer functionality across multiple rendering backends.
// most of these functions must be called from the render thread and are not intended for public us,
// there are a few exceptions to this rule.. commented as "thread safe"

#pragma once

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
            backbuffer_resize = 1,
            realloc_managed_render_targets = 1 << 1
        };
    }
    typedef u32 shared_flags;

    void                    _renderer_shared_init();
    void                    _renderer_new_frame();
    void                    _renderer_end_frame();
    bool                    _renderer_resized();
    texture_creation_params _renderer_tcp_resolve_ratio(const texture_creation_params& tcp);
    void         _renderer_track_managed_render_target(const texture_creation_params& tcp, u32 render_target_handle);
    void         _renderer_untrack_managed_render_target(u32 render_target_handle);
    void         _renderer_resize_backbuffer(u32 width, u32 height);
    void         _renderer_resize_managed_targets();
    u64          _renderer_frame_index();
    u64          _renderer_resize_index();
    shared_flags _renderer_flags();
    void         _renderer_set_viewport_ratio(const viewport& v);
    void         _renderer_set_scissor_ratio(const rect& r);
    void         _renderer_commit_stretchy_dynamic_buffers();
    size_t       _renderer_buffer_multi_update(stretchy_dynamic_buffer* buf, const void* data, size_t size);
    stretchy_dynamic_buffer* _renderer_get_stretchy_dynamic_buffer(u32 bind_flags);

    // thread safe utilities
    viewport _renderer_resolve_viewport_ratio(const viewport& v);
    rect     _renderer_resolve_scissor_ratio(const rect& r);

    // virtual interface for render backends
    class render_backend
    {
      public:
        virtual u32  initialise(void* params, u32 bb_res, u32 bb_depth_res) = 0;
        virtual void shutdown() = 0;
        virtual void sync() = 0;
        virtual void new_frame() = 0;
        virtual void end_frame() = 0;
        virtual bool frame_valid() = 0; // invalid if we resize while we are building cmdbuf on the user thread
        virtual void create_clear_state(const clear_state& cs, u32 resource_slot) = 0;
        virtual void clear(u32 clear_state_index, u32 colour_slice = 0, u32 depth_slice = 0) = 0;
        virtual void load_shader(const pen::shader_load_params& params, u32 resource_slot) = 0;
        virtual void set_shader(u32 shader_index, u32 shader_type) = 0;
        virtual void create_input_layout(const input_layout_creation_params& params, u32 resource_slot) = 0;
        virtual void set_input_layout(u32 layout_index) = 0;
        virtual void link_shader_program(const shader_link_params& params, u32 resource_slot) = 0;
        virtual void create_buffer(const buffer_creation_params& params, u32 resource_slot) = 0;
        virtual void set_vertex_buffers(u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides,
                                        const u32* offsets) = 0;
        virtual void set_index_buffer(u32 buffer_index, u32 format, u32 offset) = 0;
        virtual void set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 flags) = 0;
        virtual void set_structured_buffer(u32 buffer_index, u32 resource_slot, u32 flags) = 0;
        virtual void update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset) = 0;
        virtual void create_texture(const texture_creation_params& tcp, u32 resource_slot) = 0;
        virtual void create_sampler(const sampler_creation_params& scp, u32 resource_slot) = 0;
        virtual void set_texture(u32 texture_index, u32 sampler_index, u32 resource_slot, u32 bind_flags) = 0;
        virtual void create_rasterizer_state(const raster_state_creation_params& rscp, u32 resource_slot) = 0;
        virtual void set_rasterizer_state(u32 rasterizer_state_index) = 0;
        virtual void set_viewport(const viewport& vp) = 0;
        virtual void set_scissor_rect(const rect& r) = 0;
        virtual void create_blend_state(const blend_creation_params& bcp, u32 resource_slot) = 0;
        virtual void set_blend_state(u32 blend_state_index) = 0;
        virtual void create_depth_stencil_state(const depth_stencil_creation_params& dscp, u32 resource_slot) = 0;
        virtual void set_depth_stencil_state(u32 depth_stencil_state) = 0;
        virtual void set_stencil_ref(u8 ref) = 0;
        virtual void draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology) = 0;
        virtual void draw_indexed(u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology) = 0;
        virtual void draw_indexed_instanced(u32 instance_count, u32 start_instance, u32 index_count, u32 start_index,
                                            u32 base_vertex, u32 primitive_topology) = 0;
        virtual void draw_auto() = 0;
        virtual void dispatch_compute(uint3 grid, uint3 num_threads) = 0;
        virtual void create_render_target(const texture_creation_params& tcp, u32 resource_slot, bool track = true) = 0;
        virtual void set_targets(const u32* const colour_targets, u32 num_colour_targets, u32 depth_target,
                                 u32 colour_slice = 0, u32 depth_slice = 0) = 0;
        virtual void set_resolve_targets(u32 colour_target, u32 depth_target) = 0;
        virtual void set_stream_out_target(u32 buffer_index) = 0;
        virtual void resolve_target(u32 target, e_msaa_resolve_type type, resolve_resources res) = 0;
        virtual void read_back_resource(const resource_read_back_params& rrbp) = 0;
        virtual void present() = 0;
        virtual void push_perf_marker(const c8* name) = 0;
        virtual void pop_perf_marker() = 0;
        virtual void replace_resource(u32 dest, u32 src, e_renderer_resource type) = 0;
        virtual void release_shader(u32 shader_index, u32 shader_type) = 0;
        virtual void release_clear_state(u32 clear_state) = 0;
        virtual void release_buffer(u32 buffer_index) = 0;
        virtual void release_texture(u32 texture_index) = 0;
        virtual void release_sampler(u32 sampler) = 0;
        virtual void release_raster_state(u32 raster_state_index) = 0;
        virtual void release_blend_state(u32 blend_state) = 0;
        virtual void release_render_target(u32 render_target) = 0;
        virtual void release_input_layout(u32 input_layout) = 0;
        virtual void release_depth_stencil_state(u32 depth_stencil_state) = 0;
    };
} // namespace pen
