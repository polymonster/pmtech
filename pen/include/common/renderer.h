#ifndef _renderer_h
#define _renderer_h

#include "definitions.h"
#include "renderer_definitions.h"

//--------------------------------------------------------------------------------------
//  TOKENS
//--------------------------------------------------------------------------------------
enum special_values
{
    PEN_SHADER_NULL             =   0xffffff
};

namespace pen
{
	//--------------------------------------------------------------------------------------
	//  PUBLIC API STRUCTS
	//--------------------------------------------------------------------------------------
	struct clear_state
	{
		f32 r, g, b, a, depth;
		u32 flags;

	};

	struct stream_out_decl_entry
	{
		u32			stream;
		const c8*	semantic_name;
		u32			semantic_index;
		u8			start_component;
		u8			component_count;
		u8			output_slot;
	};

	struct shader_load_params
	{
		void*					byte_code;
		u32						byte_code_size;
		u32						type;
		stream_out_decl_entry*	so_decl_entries;
		u32						so_num_entries;
	};
	
    struct buffer_creation_params
	{
		u32	usage_flags;
		u32 bind_flags;
		u32 cpu_access_flags;
		u32 buffer_size;

		void* data;

	};

	struct input_layout_desc
	{
		c8* semantic_name;
		u32 semantic_index;
		u32 format;
		u32 input_slot;
		u32 aligned_byte_offset;
		u32 input_slot_class;
		u32 instance_data_step_rate;
	};

	struct input_layout_creation_params
	{
		input_layout_desc*	input_layout;
		u32					num_elements;
		void*				vs_byte_code;
		u32					vs_byte_code_size;
	};
    
    enum constant_type
    {
        CT_SAMPLER_2D = 0,
        CT_SAMPLER_3D = 1,
        CT_SAMPLER_CUBE = 2,
        CT_CBUFFER = 3,
        CT_CONSTANT = 4
    };
    
    struct constant_layout_desc
    {
        c8*           name;
        u32           location;
        constant_type type; 
    };
    
    struct shader_link_params
    {
        u32 vertex_shader;
        u32 pixel_shader;
        u32 input_layout;
        constant_layout_desc* constants;
        u32 num_constants;
    };

	struct texture_creation_params
	{
		u32		width;
		u32		height;
		u32		num_mips;
		u32		num_arrays;
		u32		format;
		u32		sample_count;
		u32		sample_quality;
		u32		usage;
		u32		bind_flags;
		u32		cpu_access_flags;
		u32		flags;
		void*	data;
		u32		data_size;
		u32		block_size;
		u32		pixels_per_block;

	};

	struct sampler_creation_params
	{
		u32 filter = PEN_FILTER_MIN_MAG_MIP_LINEAR;
		u32 address_u = PEN_TEXTURE_ADDRESS_WRAP;
		u32 address_v = PEN_TEXTURE_ADDRESS_WRAP;
		u32 address_w = PEN_TEXTURE_ADDRESS_WRAP;
		f32 mip_lod_bias = 0.0f;
        u32	max_anisotropy = 0.0f;
        u32 comparison_func = PEN_COMPARISON_ALWAYS;
        f32 border_color[ 4 ] = { 0.0f };
        f32 min_lod = -1.0f;
        f32 max_lod = -1.0f;

        sampler_creation_params(){};
	};

	struct rasteriser_state_creation_params
	{
        u32		fill_mode = PEN_FILL_SOLID;
		u32		cull_mode = PEN_CULL_BACK;
		s32		front_ccw = 0;
		s32		depth_bias = 0.0f;
		f32		depth_bias_clamp = 0.0f;
		f32		sloped_scale_depth_bias = 0.0f;
		s32		depth_clip_enable = 1;
		s32		scissor_enable = 0;
		s32		multisample = 0;
        s32		aa_lines = 0;
        
        rasteriser_state_creation_params(){};
	};

	struct viewport
	{
		f32 x, y, width, height;
		f32 min_depth, max_depth;

	};

    struct rect
    {
        f32 left, top, right, bottom;
    };

	struct render_target_blend
	{
        s32	blend_enable = 0;
        u32	src_blend = PEN_BLEND_ONE;
        u32	dest_blend = PEN_BLEND_ZERO;
        u32	blend_op = PEN_BLEND_OP_ADD;
        u32	src_blend_alpha = PEN_BLEND_ONE;
        u32	dest_blend_alpha = PEN_BLEND_ZERO;
        u32	blend_op_alpha = PEN_BLEND_OP_ADD;
        u8	render_target_write_mask = 0xff;
        
        render_target_blend(){};
	};

	struct stencil_op
	{
		u32 stencil_failop;
		u32 stencil_depth_failop;
		u32 stencil_passop;
		u32 stencil_func;
	};

	struct depth_stencil_creation_params
	{
		u32                     depth_enable;
		u32						depth_write_mask;
		u32						depth_func;
		u32                     stencil_enable;
		u8                      stencil_read_mask;
		u8                      stencil_write_mask;
		stencil_op				front_face;
		stencil_op				back_face;
	};

	struct blend_creation_params
	{
		s32						alpha_to_coverage_enable;
		s32						independent_blend_enable;
		u32						num_render_targets;
		render_target_blend*	render_targets;
	
	};

	//--------------------------------------------------------------------------------------
	//  COMMON FUNCTIONS
	//--------------------------------------------------------------------------------------
	
    //runs on its own thread - will wait for jobs flagged by semaphone
	PEN_THREAD_RETURN	renderer_thread_function( void* params );
    
	u32					renderer_create_clear_state( const clear_state &cs );
	f64					renderer_get_last_query(u32 query_index);
    const c8*           renderer_get_shader_platform();

	//resource management
	void renderer_reclaim_resource_indices();
	void renderer_mark_resource_deleted(u32 i);
	u32 renderer_get_next_resource_index(u32 domain);

	//--------------------------------------------------------------------------------------
	//  DIRECT API
	//--------------------------------------------------------------------------------------
	namespace direct
	{
        u32		renderer_initialise( void* params );
        void	renderer_shutdown( );
        void    renderer_make_context_current();

		//clears
		void	renderer_clear( u32 clear_state_index, u32 colour_face = 0, u32 depth_face = 0 );

		//shaders
		u32		renderer_load_shader( const pen::shader_load_params &params );
		void	renderer_create_so_shader( const pen::shader_load_params &params );
		void	renderer_set_shader( u32 shader_index, u32 shader_type );
		u32		renderer_create_input_layout( const input_layout_creation_params &params );
		void	renderer_set_input_layout( u32 layout_index );
        u32     renderer_link_shader_program( const shader_link_params &params );
        void    renderer_set_shader_program( u32 program_index );

		//buffers
		u32		renderer_create_buffer( const buffer_creation_params &params );
		void	renderer_set_vertex_buffer( u32 buffer_index, u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets );
		void	renderer_set_index_buffer( u32 buffer_index, u32 format, u32 offset );
		void	renderer_set_constant_buffer( u32 buffer_index, u32 resource_slot, u32 shader_type );
		void	renderer_update_buffer( u32 buffer_index, const void* data, u32 data_size, u32 offset );

		//textures
		u32		renderer_create_texture( const texture_creation_params& tcp );
		u32		renderer_create_sampler( const sampler_creation_params& scp );
		void	renderer_set_texture( u32 texture_index, u32 sampler_index, u32 resource_slot, u32 shader_type );

		//rasterizer
		u32		renderer_create_rasterizer_state( const rasteriser_state_creation_params &rscp );
		void	renderer_set_rasterizer_state( u32 rasterizer_state_index );
		void	renderer_set_viewport( const viewport &vp );
        void    renderer_set_scissor_rect( const rect &r );

		//blending
		u32		renderer_create_blend_state( const blend_creation_params &bcp );
		void    renderer_set_blend_state( u32 blend_state_index );

		//depth state
		u32		renderer_create_depth_stencil_state( const depth_stencil_creation_params& dscp );
		void	renderer_set_depth_stencil_state( u32 depth_stencil_state );

		//draw calls
		void	renderer_draw( u32 vertex_count, u32 start_vertex, u32 primitive_topology );
		void	renderer_draw_indexed( u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology );
		void	renderer_draw_auto();

		//render targets
		u32		renderer_create_render_target( const texture_creation_params& tcp );
		void	renderer_set_targets(u32 colour_target, u32 depth_target, u32 colour_face = 0, u32 depth_face = 0 );
		void	renderer_set_so_target( u32 buffer_index );

		//swap / present / vsync
		void	renderer_present( );
		void	renderer_create_query(u32 query_type, u32 flags);
		void	renderer_set_query(u32 query_index, u32 action);

		//cleanup
		void	renderer_release_shader( u32 shader_index, u32 shader_type );
        void	renderer_release_program( u32 program );
        void	renderer_release_clear_state( u32 clear_state );
		void	renderer_release_buffer( u32 buffer_index );
		void	renderer_release_texture( u32 texture_index );
        void	renderer_release_sampler( u32 sampler );
		void	renderer_release_raster_state( u32 raster_state_index );
		void	renderer_release_blend_state( u32 blend_state );
		void	renderer_release_render_target( u32 render_target );
		void	renderer_release_input_layout( u32 input_layout );
		void	renderer_release_depth_stencil_state( u32 depth_stencil_state );
		void	renderer_release_query( u32 query );
	}

	//--------------------------------------------------------------------------------------
	//  DEFERRED API
	//--------------------------------------------------------------------------------------
	namespace defer
	{
		//clears
		void	renderer_clear( u32 clear_state_index );
		void	renderer_clear_cube( u32 clear_state_index, u32 colour_face, u32 depth_face );

		//shaders
		u32		renderer_load_shader( const pen::shader_load_params &params );
		u32		renderer_create_so_shader( const pen::shader_load_params &params );
		void	renderer_set_shader( u32 shader_index, u32 shader_type );
		u32		renderer_create_input_layout( const input_layout_creation_params &params );
		void	renderer_set_input_layout( u32 layout_index );
        u32     renderer_link_shader_program( const shader_link_params &params );
        void    renderer_set_shader_program( u32 program_index );

		//buffers
		u32		renderer_create_buffer( const buffer_creation_params &params );
        
        void	renderer_set_vertex_buffer( u32 buffer_index, u32 start_slot, u32 stride, u32 offset );
		void	renderer_set_vertex_buffer( u32 buffer_index, u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets );
        
		void	renderer_set_index_buffer( u32 buffer_index, u32 format, u32 offset );
		void	renderer_set_constant_buffer( u32 buffer_index, u32 resource_slot, u32 shader_type );
		void	renderer_update_buffer( u32 buffer_index, const void* data, u32 data_size, u32 offset = 0 );

		//textures
		u32		renderer_create_texture( const texture_creation_params& tcp );
		u32		renderer_create_sampler( const sampler_creation_params& scp );
		void	renderer_set_texture( u32 texture_index, u32 sampler_index, u32 resource_slot, u32 shader_type );

		//rasterizer
		u32		renderer_create_rasterizer_state( const rasteriser_state_creation_params &rscp );
		void	renderer_set_rasterizer_state( u32 rasterizer_state_index );
		void	renderer_set_viewport( const viewport &vp );
        void    renderer_set_scissor_rect( const rect &r );

		//blending
		u32		renderer_create_blend_state( const blend_creation_params &bcp );
		void    renderer_set_blend_state( u32 blend_state_index );

		//depth state
		u32		renderer_create_depth_stencil_state( const depth_stencil_creation_params& dscp );
		void	renderer_set_depth_stencil_state( u32 depth_stencil_state );

		//draw calls
		void	renderer_draw( u32 vertex_count, u32 start_vertex, u32 primitive_topology );
		void	renderer_draw_indexed( u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology );
		void	renderer_draw_auto();

		//render targets
		u32		renderer_create_render_target(const texture_creation_params& tcp);
		void	renderer_set_targets(u32 colour_target, u32 depth_target);
		void	renderer_set_targets_cube(u32 colour_target, u32 colour_face, u32 depth_target, u32 depth_face );
		void	renderer_set_so_target( u32 buffer_index );

		//swap / present / vsync
		void	renderer_present( );
		u32		renderer_create_query( u32 query_type, u32 flags );
		void	renderer_set_query(u32 query_index, u32 action);

		//cleanup
		void	renderer_release_shader( u32 shader_index, u32 shader_type );
        void	renderer_release_program( u32 program );
        void	renderer_release_clear_state( u32 clear_state );
		void	renderer_release_buffer( u32 buffer_index );
		void	renderer_release_texture( u32 texture_index );
		void	renderer_release_raster_state( u32 raster_state_index );
		void	renderer_release_blend_state( u32 blend_state );
		void	renderer_release_render_target( u32 render_target );
		void	renderer_release_input_layout( u32 input_layout );
		void	renderer_release_sampler( u32 sampler );
		void	renderer_release_depth_stencil_state( u32 depth_stencil_state );
		void	renderer_release_query( u32 query );

		//cmd specific
		void	renderer_consume_cmd_buffer();
		void	renderer_update_queries();
	}
}

#endif

