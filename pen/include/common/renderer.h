#ifndef _renderer_h
#define _renderer_h

#include "definitions.h"
#include "renderer_definitions.h"

//--------------------------------------------------------------------------------------
//  TOKENS
//--------------------------------------------------------------------------------------
enum special_values
{
    PEN_CLEAR_COLOUR_BUFFER		=   0x01,
    PEN_CLEAR_DEPTH_BUFFER		=   0x02,
    PEN_SHADER_NULL             =   0xffffff
};

enum resource_types
{
    DIRECT_RESOURCE             = 0x01,
    DEFER_RESOURCE              = 0x02,
    MAX_RESOURCES			    = 10000, 
};

namespace pen
{
	//--------------------------------------------------------------------------------------
	//  PUBLIC API STRUCTS
	//--------------------------------------------------------------------------------------
	typedef struct clear_state
	{
		f32 r, g, b, a, depth;
		u32 flags;

	} clear_state;

	typedef struct stream_out_decl_entry
	{
		u32			stream;
		const c8*	semantic_name;
		u32			semantic_index;
		u8			start_component;
		u8			component_count;
		u8			output_slot;
	} stream_out_decl_entry;

	typedef struct shader_load_params
	{
		void*					byte_code;
		u32						byte_code_size;
		u32						type;
		stream_out_decl_entry*	so_decl_entries;
		u32						so_num_entries;
	} shader_load_params;

	typedef struct buffer_creation_params
	{
		u32	usage_flags;
		u32 bind_flags;
		u32 cpu_access_flags;
		u32 buffer_size;

		void* data;

	} buffer_creation_params;

	typedef struct input_layout_desc
	{
		c8* semantic_name;
		u32 semantic_index;
		u32 format;
		u32 input_slot;
		u32 aligned_byte_offset;
		u32 input_slot_class;
		u32 instance_data_step_rate;

	} 	input_layout_desc;

	typedef struct input_layout_creation_params
	{
		input_layout_desc*	input_layout;
		u32					num_elements;
		void*				vs_byte_code;
		u32					vs_byte_code_size;

	}	input_layout_creation_params;

	typedef struct texture_creation_params
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

	} texture_creation_params;

	typedef struct sampler_creation_params
	{
		u32 filter;
		u32 address_u;
		u32 address_v;
		u32 address_w;
		f32 mip_lod_bias;
		u32	max_anisotropy;
		u32 comparison_func;
		f32 border_color[ 4 ];
		f32 min_lod;
		f32 max_lod;

	} sampler_creation_params;

	typedef struct rasteriser_state_creation_params
	{
		u32		fill_mode;
		u32		cull_mode;
		s32		front_ccw;
		s32		depth_bias;
		f32		depth_bias_clamp;
		f32		sloped_scale_depth_bias;
		s32		depth_clip_enable;
		s32		scissor_enable;
		s32		multisample;
		s32		aa_lines;

	} rasteriser_state_creation_params;

	typedef struct viewport
	{
		f32 x, y, width, height;
		f32 min_depth, max_depth;

	} viewport;

	typedef struct render_target_blend
	{
		s32	blend_enable;
		u32	src_blend;
		u32	dest_blend;
		u32	blend_op;
		u32	src_blend_alpha;
		u32	dest_blend_alpha;
		u32	blend_op_alpha;
		u8	render_target_write_mask;
	} render_target_blend;

	typedef struct stencil_op
	{
		u32 stencil_failop;
		u32 stencil_depth_failop;
		u32 stencil_passop;
		u32 stencil_func;
	} 	stencil_op;

	typedef struct depth_stencil_creation_params
	{
		u32	                    depth_enable;
		u32						depth_write_mask;
		u32						depth_func;
		u32                     stencil_enable;
		u8                      stencil_read_mask;
		u8                      stencil_write_mask;
		stencil_op				front_face;
		stencil_op				back_face;
	} depth_stencil_creation_params;

	typedef struct blend_creation_params
	{
		s32						alpha_to_coverage_enable;
		s32						independent_blend_enable;
		u32						num_render_targets;
		render_target_blend*	render_targets;
	
	} blend_creation_params;

	//--------------------------------------------------------------------------------------
	//  COMMON FUNCTIONS
	//--------------------------------------------------------------------------------------
	
    //runs on its own thread - will wait for jobs flagged by semaphone
    void				renderer_thread_init();
    void				renderer_wait_init();
	PEN_THREAD_RETURN	renderer_init_thread( void* params );

    //initialised to run from another thread - call renderer_poll_for_jobs 
    void                renderer_init( void* params );
	
    void				renderer_destroy( );

    //poll can be used to render on the main thread
    void                renderer_poll_for_jobs();

	u32					renderer_create_clear_state( const clear_state &cs );

	f64					renderer_get_last_query(u32 query_index);

    const c8*           renderer_get_shader_platform();
    
	//--------------------------------------------------------------------------------------
	//  DIRECT API
	//--------------------------------------------------------------------------------------
	namespace direct
	{
        void    renderer_make_context_current();

        
		//clears
		void	renderer_clear( u32 clear_state_index, u32 colour_face = 0, u32 depth_face = 0 );

		//shaders
		u32		renderer_load_shader( const pen::shader_load_params &params );
		void	renderer_create_so_shader( const pen::shader_load_params &params );
		void	renderer_set_shader( u32 shader_index, u32 shader_type );
		u32		renderer_create_input_layout( const input_layout_creation_params &params );
		void	renderer_set_input_layout( u32 layout_index );

		//buffers
		u32		renderer_create_buffer( const buffer_creation_params &params );
		void	renderer_set_vertex_buffer( u32 buffer_index, u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets );
		void	renderer_set_index_buffer( u32 buffer_index, u32 format, u32 offset );
		void	renderer_set_constant_buffer( u32 buffer_index, u32 resource_slot, u32 shader_type );
		void	renderer_update_buffer( u32 buffer_index, const void* data );

		//textures
		u32		renderer_create_texture2d( const texture_creation_params& tcp );
		u32		renderer_create_sampler( const sampler_creation_params& scp );
		void	renderer_set_texture( u32 texture_index, u32 sampler_index, u32 resource_slot, u32 shader_type );

		//rasterizer
		u32		renderer_create_rasterizer_state( const rasteriser_state_creation_params &rscp );
		void	renderer_set_rasterizer_state( u32 rasterizer_state_index );
		void	renderer_set_viewport( const viewport &vp );

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
		void	renderer_set_colour_buffer( u32 buffer_index );
		void	renderer_set_depth_buffer( u32 buffer_index );
		void	renderer_set_so_target( u32 buffer_index );

		//swap / present / vsync
		void	renderer_present( );
		void	renderer_create_query(u32 query_type, u32 flags);
		void	renderer_set_query(u32 query_index, u32 action);

		//cleanup
		void	renderer_release_shader( u32 shader_index, u32 shader_type );
		void	renderer_release_buffer( u32 buffer_index );
		void	renderer_release_texture2d( u32 texture_index );
		void	renderer_release_raster_state( u32 raster_state_index );

		void	renderer_release_blend_state( u32 blend_state );
		void	renderer_release_render_target( u32 render_target );
		void	renderer_release_input_layout( u32 input_layout );
		void	renderer_release_sampler( u32 sampler );
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

		//buffers
		u32		renderer_create_buffer( const buffer_creation_params &params );
		void	renderer_set_vertex_buffer( u32 buffer_index, u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets );
		void	renderer_set_index_buffer( u32 buffer_index, u32 format, u32 offset );
		void	renderer_set_constant_buffer( u32 buffer_index, u32 resource_slot, u32 shader_type );
		void	renderer_update_buffer( u32 buffer_index, const void* data, u32 data_size );

		//textures
		u32		renderer_create_texture2d( const texture_creation_params& tcp );
		u32		renderer_create_sampler( const sampler_creation_params& scp );
		void	renderer_set_texture( u32 texture_index, u32 sampler_index, u32 resource_slot, u32 shader_type );

		//rasterizer
		u32		renderer_create_rasterizer_state( const rasteriser_state_creation_params &rscp );
		void	renderer_set_rasterizer_state( u32 rasterizer_state_index );
		void	renderer_set_viewport( const viewport &vp );

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
		void	renderer_set_colour_buffer( u32 buffer_index );
		void	renderer_set_depth_buffer( u32 buffer_index );
		void	renderer_set_targets(u32 colour_target, u32 depth_target);
		void	renderer_set_targets_cube(u32 colour_target, u32 colour_face, u32 depth_target, u32 depth_face );
		void	renderer_set_so_target( u32 buffer_index );

		//swap / present / vsync
		void	renderer_present( );
		u32		renderer_create_query( u32 query_type, u32 flags );
		void	renderer_set_query(u32 query_index, u32 action);

		//cleanup
		void	renderer_release_shader( u32 shader_index, u32 shader_type );
		void	renderer_release_buffer( u32 buffer_index );
		void	renderer_release_texture2d( u32 texture_index );
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

