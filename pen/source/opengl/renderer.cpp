#include <stdlib.h>

#include "renderer.h"
#include "memory.h"
#include "pen_string.h"
#include "threads.h"
#include "timer.h"
#include "pen.h"

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------

extern pen::window_creation_params pen_window;

extern void pen_make_gl_context_current( );
extern void pen_gl_swap_buffers( );

namespace pen
{
	//--------------------------------------------------------------------------------------
	//  COMMON API
	//--------------------------------------------------------------------------------------
	#define NUM_QUERY_BUFFERS		4
	#define MAX_QUERIES				64 
	#define NUM_CUBEMAP_FACES		6
 
	#define QUERY_DISJOINT			1
	#define QUERY_ISSUED			(1<<1)
	#define QUERY_SO_STATS			(1<<2)

	typedef struct context_state
	{
		context_state()
		{
			active_query_index = 0;
		}

		u32 backbuffer_colour;
		u32 backbuffer_depth;

		u32 active_colour_target;
		u32 active_depth_target;

		u32	active_query_index;

	} context_state;

	typedef struct clear_state_internal
	{
		f32 rgba[ 4 ];
		f32 depth;
		u32 flags;
	} clear_state_internal;

	typedef struct resource_allocation
	{
		u8 asigned_flag;

		union 
		{
			clear_state_internal*			clear_state;
            GLuint                          handle;
		};
	} resource_allocation;

	typedef struct query_allocation
	{
		u8 asigned_flag;
		GLuint       query			[NUM_QUERY_BUFFERS];
		u32			 flags			[NUM_QUERY_BUFFERS];
		a_u64		 last_result;
	}query_allocation;

	resource_allocation		 resource_pool	[MAX_RESOURCES];
	query_allocation	     query_pool		[MAX_QUERIES];

	void clear_resource_table( )
	{
		pen::memory_zero( &resource_pool[ 0 ], sizeof( resource_allocation ) * MAX_RESOURCES );
		
		//reserve resource 0 for NULL binding.
		resource_pool[0].asigned_flag |= 0xff;
	}

	void clear_query_table()
	{
		pen::memory_zero(&query_pool[0], sizeof(query_allocation) * MAX_QUERIES);
	}

	u32 get_next_resource_index( u32 domain )
	{
		u32 i = 0;
		while( resource_pool[ i ].asigned_flag & domain )
		{
			++i;
		}

		resource_pool[ i ].asigned_flag |= domain;

		return i;
	};

	u32 get_next_query_index(u32 domain)
	{
		u32 i = 0;
		while (query_pool[i].asigned_flag & domain)
		{
			++i;
		}

		query_pool[i].asigned_flag |= domain;

		return i;
	};

	void free_resource_index( u32 index )
	{
		pen::memory_zero( &resource_pool[ index ], sizeof( resource_allocation ) );
	}

	context_state			 g_context;

	u32 renderer_create_clear_state( const clear_state &cs )
	{
		u32 resoruce_index = get_next_resource_index( DIRECT_RESOURCE | DEFER_RESOURCE );

		resource_pool[ resoruce_index ].clear_state = (pen::clear_state_internal*)pen::memory_alloc( sizeof( clear_state_internal ) );

		resource_pool[ resoruce_index ].clear_state->rgba[ 0 ] = cs.r;
		resource_pool[ resoruce_index ].clear_state->rgba[ 1 ] = cs.g;
		resource_pool[ resoruce_index ].clear_state->rgba[ 2 ] = cs.b;
		resource_pool[ resoruce_index ].clear_state->rgba[ 3 ] = cs.a;
		resource_pool[ resoruce_index ].clear_state->depth = cs.depth;
		resource_pool[ resoruce_index ].clear_state->flags = cs.flags;

		return  resoruce_index;
	}

	f64 renderer_get_last_query(u32 query_index)
	{
		f64 res;
		pen::memory_cpy(&res, &query_pool[query_index].last_result, sizeof(f64));

		return res;
	}

	//--------------------------------------------------------------------------------------
	//  DIRECT API
	//--------------------------------------------------------------------------------------
	void direct::renderer_set_colour_buffer( u32 buffer_index )
	{

	}

	void direct::renderer_set_depth_buffer( u32 buffer_index )
	{
        
	}
    
    void direct::renderer_make_context_current( )
    {
        pen_make_gl_context_current();
    }

	void direct::renderer_clear( u32 clear_state_index, u32 colour_face, u32 depth_face )
	{
        resource_allocation& rc = resource_pool[ clear_state_index ];
        
        glClearColor( rc.clear_state->rgba[ 0 ], rc.clear_state->rgba[ 1 ], rc.clear_state->rgba[ 2 ], rc.clear_state->rgba[ 3 ] );
        glClearDepth( rc.clear_state->depth );
        
        glClear( rc.clear_state->flags );
	}

	void direct::renderer_present( )
	{
        pen_gl_swap_buffers();
	}

	void direct::renderer_create_query( u32 query_type, u32 flags )
	{
		u32 resoruce_index = get_next_query_index(DIRECT_RESOURCE);
	}

	void direct::renderer_set_query(u32 query_index, u32 action)
	{

	}

	u32 direct::renderer_load_shader(const pen::shader_load_params &params)
	{
		u32 handle_out = (u32)-1;

		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );

		return resource_index;
	}

	void direct::renderer_set_shader( u32 shader_index, u32 shader_type )
	{

	}

	u32 direct::renderer_create_buffer( const buffer_creation_params &params )
	{
		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );
        
        resource_allocation& res = resource_pool[resource_index];
        
        glGenBuffers(1, &res.handle);
        
        glBindBuffer(params.bind_flags, res.handle);
        glBufferData(GL_ARRAY_BUFFER, params.buffer_size, params.data, params.usage_flags );
        
		return resource_index;
	}

	u32 direct::renderer_create_input_layout( const input_layout_creation_params &params )
	{
		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );

		return resource_index;
	}

	void direct::renderer_set_vertex_buffer( u32 buffer_index, u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets )
	{
        resource_allocation& res = resource_pool[buffer_index];
        
        glBindBuffer( GL_ARRAY_BUFFER, res.handle );
	}

	void direct::renderer_set_input_layout( u32 layout_index )
	{

	}

	void direct::renderer_set_index_buffer( u32 buffer_index, u32 format, u32 offset )
	{
        resource_allocation& res = resource_pool[buffer_index];
        
        glBindBuffer( GL_ARRAY_BUFFER, res.handle );
	}

	void direct::renderer_draw( u32 vertex_count, u32 start_vertex, u32 primitive_topology )
	{

	}

	void direct::renderer_draw_indexed( u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology )
	{

	}

	u32 depth_texture_format_to_dsv_format( u32 tex_format )
	{
		return 0;
	}

	u32 depth_texture_format_to_srv_format( u32 tex_format )
	{
		return 0;
	}

	u32 direct::renderer_create_render_target(const texture_creation_params& tcp)
	{
		u32 resource_index = get_next_resource_index(DIRECT_RESOURCE);

		return resource_index;
	}

	void direct::renderer_set_targets( u32 colour_target, u32 depth_target, u32 colour_face, u32 depth_face )
	{

	}

	u32 direct::renderer_create_texture2d(const texture_creation_params& tcp)
	{
		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );

		return resource_index;
	}

	u32 direct::renderer_create_sampler( const sampler_creation_params& scp )
	{
		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );

		return resource_index;
	}

	void direct::renderer_set_texture( u32 texture_index, u32 sampler_index, u32 resource_slot, u32 shader_type )
	{

	}

	u32 direct::renderer_create_rasterizer_state( const rasteriser_state_creation_params &rscp )
	{
		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );

		return resource_index;
	}

	void direct::renderer_set_rasterizer_state( u32 rasterizer_state_index )
	{

	}
    
	void direct::renderer_set_viewport( const viewport &vp )
	{
        glViewport( vp.x, vp.y, vp.width, vp.height );
        glDepthRangef( vp.min_depth, vp.max_depth );
	}

	u32 direct::renderer_create_blend_state( const blend_creation_params &bcp )
	{
		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );

		return resource_index;
	}

	void direct::renderer_set_blend_state( u32 blend_state_index )
	{
        
	}

	void direct::renderer_set_constant_buffer( u32 buffer_index, u32 resource_slot, u32 shader_type )
	{

	}

	void direct::renderer_update_buffer( u32 buffer_index, const void* data )
	{

	}

	u32 direct::renderer_create_depth_stencil_state( const depth_stencil_creation_params& dscp )
	{
		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );

		return resource_index;
	}

	void direct::renderer_set_depth_stencil_state( u32 depth_stencil_state )
	{

	}

	void direct::renderer_release_shader( u32 shader_index, u32 shader_type )
	{

	}

	void direct::renderer_release_buffer( u32 buffer_index )
	{

	}

	void direct::renderer_release_texture2d( u32 texture_index )
	{

	}

	void direct::renderer_release_raster_state( u32 raster_state_index )
	{

	}

	void direct::renderer_release_blend_state( u32 blend_state )
	{

	}

	void direct::renderer_release_render_target( u32 render_target )
	{

	}

	void direct::renderer_release_input_layout( u32 input_layout )
	{

	}

	void direct::renderer_release_sampler( u32 sampler )
	{

	}

	void direct::renderer_release_depth_stencil_state( u32 depth_stencil_state )
	{

	}

	void direct::renderer_release_query( u32 query )
	{

	}

	void direct::renderer_set_so_target( u32 buffer_index )
	{

	}

	void direct::renderer_create_so_shader( const pen::shader_load_params &params )
	{

	}

	void direct::renderer_draw_auto()
	{

	}

	void renderer_update_queries()
	{

	}

	//--------------------------------------------------------------------------------------
	// Clean up the objects we've created
	//--------------------------------------------------------------------------------------
    u32 renderer_init_from_window( void* )
    {
        
    }
    
	void renderer_destroy()
	{
	
	}
}

