#include <stdlib.h>

#include "renderer.h"
#include "memory.h"
#include "pen_string.h"
#include "threads.h"
#include "timer.h"
#include "pen.h"
#include <vector>

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
    
    typedef struct vertex_attribute
    {
        u32     location;
        u32     type;
        u32     stride;
        size_t  offset;
        u32     num_elements;
    } vertex_attribute;
    
    typedef struct input_layout
    {
        std::vector<vertex_attribute> attributes;
    } input_layout;

	typedef struct resource_allocation
	{
		u8 asigned_flag;

		union 
		{
			clear_state_internal*			clear_state;
            input_layout*                   input_layout;
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
    
    typedef struct shader_program
    {
        u32 vs;
        u32 ps;
        u32 gs;
        GLuint program;
    }shader_program;
    
    std::vector<shader_program> shader_programs;
    
    typedef struct active_state
    {
        u32 vertex_buffer;
        u32 index_buffer;
        u32 input_layout;
        u32 vertex_shader;
        u32 pixel_shader;
    }active_state;
    
    active_state g_bound_state;
    active_state g_current_state;

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
		//u32 resoruce_index = get_next_query_index(DIRECT_RESOURCE);
	}

	void direct::renderer_set_query(u32 query_index, u32 action)
	{

	}

	u32 direct::renderer_load_shader(const pen::shader_load_params &params)
	{
		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );
        
        resource_allocation& res = resource_pool[ resource_index ];
        
        res.handle = glCreateShader(params.type);
        
        glShaderSource(res.handle, 1, (c8**)&params.byte_code, (s32*)&params.byte_code_size);
        glCompileShader(res.handle);
        
        // Check compilation status
        GLint result = GL_FALSE;
        int info_log_length;
        
        glGetShaderiv(res.handle, GL_COMPILE_STATUS, &result);
        glGetShaderiv(res.handle, GL_INFO_LOG_LENGTH, &info_log_length);
        
        if ( info_log_length > 0 )
        {
            char* info_log_buf = (char*)pen::memory_alloc(info_log_length + 1);
            
            glGetShaderInfoLog(res.handle, info_log_length, NULL, &info_log_buf[0]);
            
            pen::string_output_debug(info_log_buf);
        }

		return resource_index;
	}

	void direct::renderer_set_shader( u32 shader_index, u32 shader_type )
	{
        if( shader_type == GL_VERTEX_SHADER )
            g_current_state.vertex_shader = shader_index;
        else if( shader_type == GL_FRAGMENT_SHADER )
            g_current_state.pixel_shader = shader_index;
	}

	u32 direct::renderer_create_buffer( const buffer_creation_params &params )
	{
		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );
        
        resource_allocation& res = resource_pool[resource_index];
        
        glGenBuffers(1, &res.handle);
        
        glBindBuffer(params.bind_flags, res.handle);
        
        glBufferData(params.bind_flags, params.buffer_size, params.data, params.usage_flags );
        
		return resource_index;
	}

	u32 direct::renderer_create_input_layout( const input_layout_creation_params &params )
	{
		u32 resource_index = get_next_resource_index( DIRECT_RESOURCE );
        
        resource_allocation& res = resource_pool[ resource_index ];
        
        res.input_layout = new input_layout;
    
        auto& attributes = res.input_layout->attributes;
        
        attributes.resize(params.num_elements);
        
        for( u32 i = 0; i < params.num_elements; ++i )
        {
            attributes[ i ].location        = params.input_layout[ i ].semantic_index;
            attributes[ i ].type            = UNPACK_FORMAT(params.input_layout[ i ].format);
            attributes[ i ].num_elements    = UNPACK_NUM_ELEMENTS(params.input_layout[ i ].format);
            attributes[ i ].offset          = params.input_layout[ i ].aligned_byte_offset;
            attributes[ i ].stride          = 0;
        }

		return resource_index;
	}

	void direct::renderer_set_vertex_buffer( u32 buffer_index, u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets )
	{
        g_current_state.vertex_buffer = buffer_index;
	}

	void direct::renderer_set_input_layout( u32 layout_index )
	{
        g_current_state.input_layout = layout_index;
	}

	void direct::renderer_set_index_buffer( u32 buffer_index, u32 format, u32 offset )
	{
        g_current_state.index_buffer = buffer_index;
	}
    
    bool create_vao = true;
    
    void bind_state()
    {
        if( create_vao )
        {
            GLuint VertexArrayID;
            glGenVertexArrays(1, &VertexArrayID);
            glBindVertexArray(VertexArrayID);
            create_vao = false;
        }
        
        glEnableVertexAttribArray(0);
        
        //bind vertex buffer
        if( g_current_state.vertex_buffer != g_bound_state.vertex_buffer )
        {
            g_bound_state.vertex_buffer = g_current_state.vertex_buffer;
            
            auto& res = resource_pool[g_bound_state.vertex_buffer].handle;
            glBindBuffer(GL_ARRAY_BUFFER, res);
        }
        
        //bind index buffer
        if( g_current_state.index_buffer != g_bound_state.index_buffer )
        {
            g_bound_state.index_buffer = g_current_state.index_buffer;
            
            auto& res = resource_pool[g_bound_state.index_buffer].handle;
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res);
        }
        
        //bind input layout
        if( g_current_state.input_layout != g_bound_state.input_layout )
        {
            g_bound_state.input_layout = g_current_state.input_layout;
            
            auto& res = resource_pool[g_bound_state.input_layout].input_layout;
            
            for( auto& attribute : res->attributes )
            {
                glVertexAttribPointer(
                                      attribute.location,
                                      attribute.num_elements,
                                      attribute.type,
                                      false,
                                      attribute.stride,
                                      (void*)attribute.offset);
            }
        }
        
        //bind shaders
        if( g_current_state.vertex_shader != g_bound_state.vertex_shader ||
            g_current_state.pixel_shader != g_bound_state.pixel_shader )
        {
            g_bound_state.vertex_shader = g_current_state.vertex_shader;
            g_bound_state.pixel_shader = g_current_state.pixel_shader;
            
            shader_program* linked_program = nullptr;
            
            for( auto program : shader_programs )
            {
                if( program.vs == g_bound_state.vertex_shader && program.vs == g_bound_state.vertex_shader )
                {
                    linked_program = &program;
                    break;
                }
            }
            
            if( linked_program == nullptr )
            {
                auto vs_handle = resource_pool[g_bound_state.vertex_shader].handle;
                auto ps_handle = resource_pool[g_bound_state.pixel_shader].handle;
                
                //link the shaders
                GLuint program_id = glCreateProgram();
                
                glAttachShader(program_id, vs_handle);
                glAttachShader(program_id, ps_handle);
                glLinkProgram(program_id);
                
                // Check the program
                GLint result = GL_FALSE;
                int info_log_length;
                
                glGetShaderiv(program_id, GL_LINK_STATUS, &result);
                glGetShaderiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
                
                if ( info_log_length > 0 )
                {
                    char* info_log_buf = (char*)pen::memory_alloc(info_log_length + 1);
                    
                    glGetShaderInfoLog(program_id, info_log_length, NULL, &info_log_buf[0]);
                    
                    pen::string_output_debug(info_log_buf);
                }
                
                shader_program program;
                program.vs = g_bound_state.vertex_shader;
                program.ps = g_bound_state.pixel_shader;
                program.program = program_id;
                
                shader_programs.push_back(program);
                
                linked_program = &shader_programs.back();
            }
            
            glUseProgram( linked_program->program );
        }
    }

	void direct::renderer_draw( u32 vertex_count, u32 start_vertex, u32 primitive_topology )
	{
        bind_state();
        
        //glDisable( GL_CULL_FACE );
        //glDepthFunc( GL_ALWAYS );
        //glDisable( GL_BLEND );
        
        glDrawArrays(primitive_topology, start_vertex, vertex_count);
        
        glDisableVertexAttribArray(0);
        
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
        //glDepthRangef( vp.min_depth, vp.max_depth );
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
        //const GLubyte* version = glGetString(GL_SHADING_LANGUAGE_VERSION);
    }
    
	void renderer_destroy()
	{
	
	}
}

