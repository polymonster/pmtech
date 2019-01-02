#include "renderer.h"

// globals
a_u8 g_window_resize;

namespace pen
{
    a_u64 g_gpu_total;
    
    bool renderer_viewport_vup()
    {
        return false;
    }
    
    namespace direct
    {
        u32 renderer_initialise(void* params, u32 bb_res, u32 bb_depth_res) 
        {
            return u32();
        }
        
        void renderer_shutdown() 
        {
        
        }
        
        void renderer_make_context_current() 
        {
        
        }
        
        void renderer_create_clear_state(const clear_state& cs, u32 resource_slot) 
        {
        
        }
        
        void renderer_clear(u32 clear_state_index, u32 colour_face, u32 depth_face)
        {
        
        }
        
        void renderer_load_shader(const pen::shader_load_params& params, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_shader(u32 shader_index, u32 shader_type) 
        {
        
        }
        
        void renderer_create_input_layout(const input_layout_creation_params& params, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_input_layout(u32 layout_index) 
        {
        
        }
        
        void renderer_link_shader_program(const shader_link_params& params, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_shader_program(u32 program_index) 
        {
        
        }
        
        void renderer_create_buffer(const buffer_creation_params& params, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_vertex_buffer(u32 buffer_index, u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets) 
        {
        
        }
        
        void renderer_set_vertex_buffers(u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides, const u32* offsets) 
        {
        
        }
        
        void renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset) 
        {
        
        }
        
        void renderer_set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 shader_type) 
        {
        
        }
        
        void renderer_update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset) 
        {
        
        }
        
        void renderer_create_texture(const texture_creation_params& tcp, u32 resource_slot) 
        {
        
        }
        
        void renderer_create_sampler(const sampler_creation_params& scp, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_texture(u32 texture_index, u32 sampler_index, u32 resource_slot, u32 bind_flags) 
        {
        
        }
        
        void renderer_create_rasterizer_state(const rasteriser_state_creation_params& rscp, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_rasterizer_state(u32 rasterizer_state_index) 
        {
        
        }
        
        void renderer_set_viewport(const viewport& vp) 
        {
        
        }
        
        void renderer_set_scissor_rect(const rect& r) 
        {
        
        }
        
        void renderer_create_blend_state(const blend_creation_params& bcp, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_blend_state(u32 blend_state_index) 
        {
        
        }
        
        void renderer_create_depth_stencil_state(const depth_stencil_creation_params& dscp, u32 resource_slot) 
        {
        
        }
        
        void renderer_set_depth_stencil_state(u32 depth_stencil_state) 
        {
        
        }
        
        void renderer_draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology) 
        {
        
        }
        
        void renderer_draw_indexed(u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology) 
        {
        
        }
        
        void renderer_draw_indexed_instanced(u32 instance_count, u32 start_instance, u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology) 
        {
        
        }
        
        void renderer_draw_auto() 
        {
        
        }
        
        void renderer_create_render_target(const texture_creation_params& tcp, u32 resource_slot, bool track)
        {
        
        }
        
        void renderer_set_targets(const u32* const colour_targets, u32 num_colour_targets, u32 depth_target, u32 colour_face, u32 depth_face)
        {
        
        }
        
        void renderer_set_resolve_targets(u32 colour_target, u32 depth_target) 
        {
        
        }
        
        void renderer_set_stream_out_target(u32 buffer_index) 
        {
        
        }
        
        void renderer_resolve_target(u32 target, e_msaa_resolve_type type) 
        {
        
        }
        
        void renderer_read_back_resource(const resource_read_back_params& rrbp) 
        {
        
        }
        
        void renderer_present() 
        {
        
        }
        
        void renderer_push_perf_marker(const c8* name) 
        {
        
        }
        
        void renderer_pop_perf_marker() 
        {
        
        }
        
        void renderer_replace_resource(u32 dest, u32 src, e_renderer_resource type) 
        {
        
        }
        
        void renderer_release_shader(u32 shader_index, u32 shader_type) 
        {
        
        }
        
        void renderer_release_program(u32 program) 
        {
        
        }
        
        void renderer_release_clear_state(u32 clear_state) 
        {
        
        }
        
        void renderer_release_buffer(u32 buffer_index) 
        {
        
        }
        
        void renderer_release_texture(u32 texture_index) 
        {
        
        }
        
        void renderer_release_sampler(u32 sampler) 
        {
        
        }
        
        void renderer_release_raster_state(u32 raster_state_index) 
        {
        
        }
        
        void renderer_release_blend_state(u32 blend_state) 
        {
        
        }
        
        void renderer_release_render_target(u32 render_target) 
        {
        
        }
        
        void renderer_release_input_layout(u32 input_layout) 
        {
        
        }
        
        void renderer_release_depth_stencil_state(u32 depth_stencil_state) 
        {
        
        }
        
    }

}

