#include <stdlib.h>

#include "renderer.h"
#include "memory.h"
#include "pen_string.h"
#include "threads.h"
#include "timer.h"
#include "pen.h"
#include "hash.h"
#include <vector>

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------

extern pen::window_creation_params pen_window;

extern void pen_make_gl_context_current( );
extern void pen_gl_swap_buffers( );
extern void pen_window_resize( );

a_u8 g_window_resize( 0 );

void gl_error_break( GLenum err )
{
    u32 a = 0;
    switch (err)
    {
        case GL_INVALID_ENUM:
            a = 0;
            break;
        case GL_INVALID_VALUE:
            a = 0;
            break;
        case GL_INVALID_OPERATION:
            a = 0;
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            a = 0;
            break;
        case GL_OUT_OF_MEMORY:
            a = 0;
            break;
        default:
            break;
    }
}

#ifdef GL_DEBUG
#define CHECK_GL_ERROR { GLenum err = glGetError( ); if( err != GL_NO_ERROR ) gl_error_break( err ); }
#else
#define CHECK_GL_ERROR
#endif

namespace pen
{
	//--------------------------------------------------------------------------------------
	//  COMMON API
	//--------------------------------------------------------------------------------------
	#define MAX_VERTEX_BUFFERS      4
    #define NUM_QUERY_BUFFERS		4
	#define MAX_QUERIES				64 
	#define NUM_CUBEMAP_FACES		6
    #define MAX_VERTEX_ATTRIBUTES   16
    #define MAX_UNIFORM_BUFFERS     16
 
	#define QUERY_DISJOINT			1
	#define QUERY_ISSUED			(1<<1)
	#define QUERY_SO_STATS			(1<<2)
    
    struct context_state
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

	};

    struct clear_state_internal
	{
		f32 rgba[ 4 ];
		f32 depth;
        u8 stencil;
		u32 flags;
        
        pen::mrt_clear mrt[PEN_MAX_MRT];
        u32 num_colour_targets;
	};
    
    struct vertex_attribute
    {
        u32     location;
        u32     type;
        u32     stride;
        size_t  offset;
        u32     num_elements;
        u32     input_slot;
        u32     step_rate;
    };
    
    struct input_layout
    {
        std::vector<vertex_attribute> attributes;
        GLuint vertex_array_handle = 0;
        GLuint vb_handle = 0;
    };

    struct raster_state
    {
        GLenum cull_face;
        GLenum polygon_mode;
        bool front_ccw;
        bool culling_enabled;
        bool depth_clip_enabled;
        bool scissor_enabled;
    };
    
    struct texture_info
    {
        GLuint handle;
        u32 max_mip_level;
    };
    
    struct render_target
    {
        texture_info texture;
        texture_info texture_msaa;
        GLuint w, h;
        u32 uid;
        
        texture_creation_params* tcp;
    };
    
    struct framebuffer
    {
        hash_id hash;
        GLuint  framebuffer;
    };
    static std::vector<framebuffer> k_framebuffers;
    
    enum resource_type : s32
    {
        RES_TEXTURE = 0,
        RES_RENDER_TARGET,
        RES_RENDER_TARGET_MSAA
    };
    
#define INVALID_LOC 255
    struct shader_program
    {
        u32 vs;
        u32 ps;
        u32 gs;
        GLuint program;
        u8 uniform_block_location[MAX_UNIFORM_BUFFERS];
        u8 texture_location[MAX_UNIFORM_BUFFERS];
    };
    static std::vector<shader_program> k_shader_programs;
    
    struct managed_render_target
    {
        texture_creation_params tcp;
        u32 render_target_handle;
    };
    static std::vector<managed_render_target> k_managed_render_targets;
    
	struct resource_allocation
	{
		u8      asigned_flag;
        GLuint  type;
        
		union 
		{
			clear_state_internal			clear_state;
            input_layout*                   input_layout;
            raster_state                    raster_state;
            depth_stencil_creation_params*  depth_stencil;
            blend_creation_params*          blend_state;
            GLuint                          handle;
            texture_info                    texture;
            render_target                   render_target;
            sampler_creation_params*        sampler_state;
            shader_program*                 shader_program;
		};
	};
    resource_allocation		 resource_pool	[MAX_RENDERER_RESOURCES];

    struct query_allocation
	{
		u8              asigned_flag;
		GLuint          query           [NUM_QUERY_BUFFERS];
		u32             flags           [NUM_QUERY_BUFFERS];
		a_u64           last_result;
	};
	query_allocation	query_pool		[MAX_QUERIES];
    
    
    struct active_state
    {
        u32 vertex_buffer[MAX_VERTEX_BUFFERS] = { 0 };
        u32 vertex_buffer_stride[MAX_VERTEX_BUFFERS] = { 0 };
        u32 vertex_buffer_offset[MAX_VERTEX_BUFFERS] = { 0 };
        u32 num_bound_vertex_buffers = 0;
        u32 index_buffer;
        u32 input_layout;
        u32 vertex_shader;
        u32 pixel_shader;
        u32 raster_state;
        bool backbuffer_bound;
        u8 constant_buffer_bindings[MAX_UNIFORM_BUFFERS] = { 0 };
    };
    
    active_state g_bound_state;
    active_state g_current_state;

	void clear_resource_table( )
	{
		pen::memory_zero( &resource_pool[ 0 ], sizeof( resource_allocation ) * MAX_RENDERER_RESOURCES );
		
		//reserve resource 0 for NULL binding.
		resource_pool[0].asigned_flag |= 0xff;
	}

	void clear_query_table()
	{
		pen::memory_zero(&query_pool[0], sizeof(query_allocation) * MAX_QUERIES);
	}
    
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

	context_state			 g_context;

    void direct::renderer_create_clear_state( const clear_state &cs, u32 resource_slot )
	{
		resource_pool[ resource_slot ].clear_state.rgba[ 0 ] = cs.r;
		resource_pool[ resource_slot ].clear_state.rgba[ 1 ] = cs.g;
		resource_pool[ resource_slot ].clear_state.rgba[ 2 ] = cs.b;
		resource_pool[ resource_slot ].clear_state.rgba[ 3 ] = cs.a;
		resource_pool[ resource_slot ].clear_state.depth = cs.depth;
        resource_pool[ resource_slot ].clear_state.stencil = cs.stencil;
		resource_pool[ resource_slot ].clear_state.flags = cs.flags;
        
        resource_pool[ resource_slot ].clear_state.num_colour_targets = cs.num_colour_targets;
        pen::memory_cpy(&resource_pool[ resource_slot ].clear_state.mrt, cs.mrt, sizeof(pen::mrt_clear)*PEN_MAX_MRT);
	}

	f64 renderer_get_last_query(u32 query_index)
	{
		f64 res;
		pen::memory_cpy(&res, &query_pool[query_index].last_result, sizeof(f64));

		return res;
	}
    
    shader_program* link_program_internal( GLuint vs, GLuint ps )
    {
        //link the shaders
        GLuint program_id = glCreateProgram();
        
        glAttachShader(program_id, vs);
        glAttachShader(program_id, ps);
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
            info_log_buf[info_log_length] = '\0';
            
            PEN_PRINTF(info_log_buf);
        }
        
        shader_program program;
        program.vs = vs;
        program.ps = ps;
        program.program = program_id;
        
        for( s32 i = 0; i < MAX_UNIFORM_BUFFERS; ++i )
        {
            program.texture_location[ i ] = INVALID_LOC;
            program.uniform_block_location[ i ] = INVALID_LOC;
        }
        
        k_shader_programs.push_back(program);
        
        CHECK_GL_ERROR;
        
        return &k_shader_programs.back();
    }

	//--------------------------------------------------------------------------------------
	//  DIRECT API
	//--------------------------------------------------------------------------------------    
    void direct::renderer_make_context_current( )
    {
        pen_make_gl_context_current();
    }

	void direct::renderer_clear( u32 clear_state_index, u32 colour_face, u32 depth_face )
	{
        resource_allocation& rc = resource_pool[ clear_state_index ];
        clear_state_internal& cs = rc.clear_state;
        
        glClearDepth( cs.depth );
        glClearStencil( cs.stencil );
        
        if( cs.num_colour_targets == 0 )
        {
            glClearColor( rc.clear_state.rgba[ 0 ], rc.clear_state.rgba[ 1 ], rc.clear_state.rgba[ 2 ], rc.clear_state.rgba[ 3 ] );
            glClear( rc.clear_state.flags );
            return;
        }
        
        u32 masked = rc.clear_state.flags &= ~GL_COLOR_BUFFER_BIT;
        
        glClear( masked );
        
        for( s32 i = 0; i < cs.num_colour_targets; ++i )
        {
            if( cs.mrt[i].type == pen::CLEAR_F32 )
            {
                glClearBufferfv( GL_COLOR, i, cs.mrt[i].f );
                continue;
            }
            
            if( cs.mrt[i].type == pen::CLEAR_U32 )
            {
                glClearBufferuiv( GL_COLOR,  i, cs.mrt[i].u );
                continue;
            }
        }
	}

    static u32 k_resize_counter = 0;
    static bool k_needs_resize = false;
    
    void renderer_resize_managed_targets( )
    {
        for( auto& managed_rt : k_managed_render_targets )
        {
            resource_allocation& res = resource_pool[ managed_rt.render_target_handle ];
            glDeleteTextures( 1, &res.render_target.texture.handle );
            
            if( managed_rt.tcp.sample_count > 1 )
            {
                glDeleteTextures( 1, &res.render_target.texture_msaa.handle );
                res.render_target.texture_msaa.handle = 0;
            }
            
            direct::renderer_create_render_target(managed_rt.tcp, managed_rt.render_target_handle, false );
        }
        
        k_framebuffers.clear();
    }
    
	void direct::renderer_present( )
	{
        pen_gl_swap_buffers();

        if( g_window_resize )
        {
            k_needs_resize = true;
            k_resize_counter = 0;
            g_window_resize = 0;
        }
        else
        {
            k_resize_counter++;
        }
        
        if( k_resize_counter > 5 && k_needs_resize )
        {
            k_resize_counter = 0;
            renderer_resize_managed_targets( );
            k_needs_resize = false;
        }
	}
    
	void direct::renderer_create_query( u32 query_type, u32 flags )
	{
	}

	void direct::renderer_set_query(u32 query_index, u32 action)
	{

	}

	void direct::renderer_load_shader(const pen::shader_load_params &params, u32 resource_slot)
	{
        resource_allocation& res = resource_pool[ resource_slot ];
        
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
            PEN_PRINTF( "%s", params.byte_code );
            
            char* info_log_buf = (char*)pen::memory_alloc(info_log_length + 1);
            
            glGetShaderInfoLog(res.handle, info_log_length, NULL, &info_log_buf[0]);
            
            PEN_PRINTF(info_log_buf);
        }

        CHECK_GL_ERROR;
	}

	void direct::renderer_set_shader( u32 shader_index, u32 shader_type )
	{
        if( shader_type == GL_VERTEX_SHADER )
            g_current_state.vertex_shader = shader_index;
        else if( shader_type == GL_FRAGMENT_SHADER )
            g_current_state.pixel_shader = shader_index;
	}

	void direct::renderer_create_buffer( const buffer_creation_params &params, u32 resource_slot )
	{
        resource_allocation& res = resource_pool[resource_slot];
        
        CHECK_GL_ERROR;
        
        glGenBuffers(1, &res.handle);
        
        glBindBuffer(params.bind_flags, res.handle);
        
        glBufferData(params.bind_flags, params.buffer_size, params.data, params.usage_flags );
        
        CHECK_GL_ERROR;
        
        res.type = params.bind_flags;
	}
    
    void direct::renderer_link_shader_program(const pen::shader_link_params &params, u32 resource_slot )
    {
        GLuint vs = resource_pool[ params.vertex_shader ].handle;
        GLuint ps = resource_pool[ params.pixel_shader ].handle;
        
        shader_program* linked_program = link_program_internal( vs, ps );
        
        GLuint prog = linked_program->program;
    
        //build lookup tables for uniform buffers and texture samplers
        for( u32 i = 0; i < params.num_constants; ++i )
        {
            constant_layout_desc& constant = params.constants[i];
            GLint loc;
            
            switch( constant.type )
            {
                case pen::CT_CBUFFER:
                {
                    loc = glGetUniformBlockIndex(prog, constant.name);
                    
                    PEN_ASSERT( loc < MAX_UNIFORM_BUFFERS );
                    
                    linked_program->uniform_block_location[constant.location] = loc;
                    
                    glUniformBlockBinding( prog, loc, constant.location );
                }
                break;
                    
                case pen::CT_SAMPLER_2D:
                case pen::CT_SAMPLER_2DMS:
                {
                    loc = glGetUniformLocation(prog, constant.name);
                    
                    linked_program->texture_location[constant.location] = loc;
                    
                    glUniform1i( loc, constant.location );
                }
                break;
                default:
                    break;
            }
        }
        
        resource_pool[ resource_slot ].shader_program = linked_program;
    }
    
    void direct::renderer_set_stream_out_target( u32 buffer_index )
    {
        
    }
    
    void direct::renderer_create_stream_out_shader( const pen::shader_load_params &params, u32 resource_slot )
    {
        
    }
    
    void direct::renderer_draw_auto( )
    {
        
    }

	void direct::renderer_create_input_layout( const input_layout_creation_params &params, u32 resource_slot )
	{
        resource_allocation& res = resource_pool[ resource_slot ];
        
        res.input_layout = new input_layout;
    
        auto& attributes = res.input_layout->attributes;
        
        attributes.resize(params.num_elements);
        
        for( u32 i = 0; i < params.num_elements; ++i )
        {
            attributes[ i ].location        = i;
            attributes[ i ].type            = UNPACK_FORMAT(params.input_layout[ i ].format);
            attributes[ i ].num_elements    = UNPACK_NUM_ELEMENTS(params.input_layout[ i ].format);
            attributes[ i ].offset          = params.input_layout[ i ].aligned_byte_offset;
            attributes[ i ].stride          = 0;
            attributes[ i ].input_slot      = params.input_layout[ i ].input_slot;
            attributes[ i ].step_rate       = params.input_layout[ i ].instance_data_step_rate;
        }
	}

	void direct::renderer_set_vertex_buffer( u32 buffer_index, u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets )
	{
        g_current_state.vertex_buffer[0] = buffer_index;
        g_current_state.vertex_buffer_stride[0] = strides[ 0 ];
        g_current_state.vertex_buffer_offset[0] = offsets[0];
        g_current_state.num_bound_vertex_buffers = 1;
	}
    
    void direct::renderer_set_vertex_buffers( u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides, const u32* offsets )
    {
        for( s32 i = 0; i < num_buffers; ++i )
        {
            g_current_state.vertex_buffer[start_slot + i] = buffer_indices[i];
            g_current_state.vertex_buffer_stride[start_slot + i] = strides[i];
            g_current_state.vertex_buffer_offset[start_slot + i] = offsets[i];
        }
        
        g_current_state.num_bound_vertex_buffers = num_buffers;
    }

	void direct::renderer_set_input_layout( u32 layout_index )
	{
        g_current_state.input_layout = layout_index;
	}

	void direct::renderer_set_index_buffer( u32 buffer_index, u32 format, u32 offset )
	{
        g_bound_state.index_buffer = buffer_index;
	}
    
    void bind_state()
    {
        //bind shaders
        if( g_current_state.vertex_shader != g_bound_state.vertex_shader ||
            g_current_state.pixel_shader != g_bound_state.pixel_shader )
        {
            g_bound_state.vertex_shader = g_current_state.vertex_shader;
            g_bound_state.pixel_shader = g_current_state.pixel_shader;
            
            shader_program* linked_program = nullptr;
            
            auto vs_handle = resource_pool[g_bound_state.vertex_shader].handle;
            auto ps_handle = resource_pool[g_bound_state.pixel_shader].handle;
            
            for( s32 i = 0; i < k_shader_programs.size(); ++i )
            {
                if( k_shader_programs[i].vs == vs_handle && k_shader_programs[i].ps == ps_handle )
                {
                    linked_program = &k_shader_programs[i];
                    break;
                }
            }
        
            if( linked_program == nullptr )
            {
                linked_program = link_program_internal(vs_handle, ps_handle);
            }
            
            glUseProgram( linked_program->program );
            
            //constant buffers and texture locations
            for( s32 i = 0; i < MAX_UNIFORM_BUFFERS; ++i )
            {
                if( linked_program->texture_location[ i ] != INVALID_LOC )
                {
                    glUniform1i( linked_program->texture_location[ i ], i );
                }
            }
            
            CHECK_GL_ERROR;
        }
        
#if 1
        g_bound_state.input_layout = g_current_state.input_layout;
        
        auto* input_res = resource_pool[g_current_state.input_layout].input_layout;
        if( input_res->vertex_array_handle == 0 )
        {
            glGenVertexArrays(1, &input_res->vertex_array_handle);
        }
        
        glBindVertexArray(input_res->vertex_array_handle);
        
        for( s32 v = 0; v < g_current_state.num_bound_vertex_buffers; ++v )
        {
            //if( v > 0 )
                //continue;
            
            g_bound_state.vertex_buffer[v] = g_current_state.vertex_buffer[v];
            g_bound_state.vertex_buffer_stride[v] = g_current_state.vertex_buffer_stride[v];
            
            auto& res = resource_pool[g_bound_state.vertex_buffer[v]].handle;
            
            CHECK_GL_ERROR;
            
            for( auto& attribute : input_res->attributes )
            {
                if( attribute.input_slot != v )
                    continue;
                
                glEnableVertexAttribArray(attribute.location);
                glBindBuffer(GL_ARRAY_BUFFER, res);
                
                glVertexAttribPointer(
                                      attribute.location,
                                      attribute.num_elements,
                                      attribute.type,
                                      attribute.type == GL_UNSIGNED_BYTE ? true : false,
                                      g_bound_state.vertex_buffer_stride[v],
                                      (void*)attribute.offset);
                
                glVertexAttribDivisor(attribute.location, attribute.step_rate);
            }
        }
#else
        {
            g_bound_state.vertex_buffer[0] = g_current_state.vertex_buffer[0];
            
            auto& res = resource_pool[g_bound_state.vertex_buffer[0]].handle;
            glBindBuffer(GL_ARRAY_BUFFER, res);
        }
        
        //bind input layout
        auto* input_res = resource_pool[g_current_state.input_layout].input_layout;

        //if input layout has changed, vb has changed or the stride of the vb has changed
        bool invalidate_input_layout = input_res->vb_handle == 0 || input_res->vb_handle != g_bound_state.vertex_buffer[0];
        invalidate_input_layout |= g_current_state.input_layout != g_bound_state.input_layout;
        invalidate_input_layout |= g_current_state.vertex_buffer_stride != g_bound_state.vertex_buffer_stride;
        
        //if( invalidate_input_layout )
        {
            g_bound_state.input_layout = g_current_state.input_layout;
            g_bound_state.vertex_buffer_stride[0] = g_current_state.vertex_buffer_stride[0];
            
            auto* res = input_res;
            
            //if we havent already generated one or we have previously been bound to a different vb layout
            //if( res->vertex_array_handle == 0 || res->vb_handle != g_bound_state.vertex_buffer[0] )
            {
                if( res->vertex_array_handle == 0 )
                {
                    glGenVertexArrays(1, &res->vertex_array_handle);
                }
                
                //res->vb_handle = g_bound_state.vertex_buffer[0];
                
                glBindVertexArray(res->vertex_array_handle);
                
                for( auto& attribute : res->attributes )
                {
                    glVertexAttribPointer(
                                          attribute.location,
                                          attribute.num_elements,
                                          attribute.type,
                                          attribute.type == GL_UNSIGNED_BYTE ? true : false,
                                          g_bound_state.vertex_buffer_stride[0],
                                          (void*)attribute.offset);
                    
                    glEnableVertexAttribArray(attribute.location);
                }
            }
            
            glBindVertexArray( res->vertex_array_handle );
            
            CHECK_GL_ERROR;
        }
#endif

        if( g_bound_state.raster_state != g_current_state.raster_state ||
            g_bound_state.backbuffer_bound != g_current_state.backbuffer_bound )
        {
            g_bound_state.raster_state = g_current_state.raster_state;
            g_bound_state.backbuffer_bound = g_current_state.backbuffer_bound;
            
            auto& rs = resource_pool[ g_bound_state.raster_state ].raster_state;
            
            bool ccw = rs.front_ccw;
            if( !g_current_state.backbuffer_bound )
                ccw = !ccw;
            
            if( ccw )
                glFrontFace(GL_CCW);
            else
                glFrontFace(GL_CW);
            
            if( rs.culling_enabled )
            {
                glEnable( GL_CULL_FACE );
                glCullFace(rs.cull_face);
            }
            else
            {
                glDisable(GL_CULL_FACE);
            }
            
            if( rs.depth_clip_enabled )
            {
                glDisable(GL_DEPTH_CLAMP);
            }
            else
            {
                glEnable(GL_DEPTH_CLAMP);
            }
            
            glPolygonMode(GL_FRONT_AND_BACK, rs.polygon_mode);
            
            if( rs.scissor_enabled )
            {
                glEnable(GL_SCISSOR_TEST);
            }
            else
            {
                glDisable(GL_SCISSOR_TEST);
            }
            
            CHECK_GL_ERROR;
        }
    }

	void direct::renderer_draw( u32 vertex_count, u32 start_vertex, u32 primitive_topology )
	{
        bind_state();
        
        glDrawArrays(primitive_topology, start_vertex, vertex_count);
	}

	void direct::renderer_draw_indexed( u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology )
	{
        bind_state();
        
        //bind index buffer -this must always be re-bound
        GLuint res = resource_pool[g_bound_state.index_buffer].handle;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res);
            
        CHECK_GL_ERROR;

        
        void* offset = (void*)(size_t)(start_index * 2);
        
        glDrawElementsBaseVertex( primitive_topology, index_count, GL_UNSIGNED_SHORT, offset, base_vertex );
	}
    
    void direct::renderer_draw_indexed_instanced(
        u32 instance_count,
        u32 start_instance,
        u32 index_count,
        u32 start_index,
        u32 base_vertex,
        u32 primitive_topology )
    {
        bind_state();
        
        //bind index buffer -this must always be re-bound
        GLuint res = resource_pool[g_bound_state.index_buffer].handle;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res);
        
        //todo this needs to check index size 32 or 16 bit
        void* offset = (void*)(size_t)(start_index * 2);
        
        glDrawElementsInstancedBaseVertex(primitive_topology, index_count, GL_UNSIGNED_SHORT, offset, instance_count, base_vertex);
    }
    
    u32 calc_mip_level_size( u32 w, u32 h, u32 block_size, u32 pixels_per_block )
    {
        u32 num_blocks = (w * h) / pixels_per_block;
        u32 size = num_blocks * block_size;
        
        return size;
    }
    
    struct tex_format_map
    {
        u32 pen_format;
        u32 sized_format;
        u32 format;
        u32 type;
    };
    
    static tex_format_map k_tex_format_map[] =
    {
        { PEN_TEX_FORMAT_BGRA8_UNORM, GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE },
        { PEN_TEX_FORMAT_RGBA8_UNORM, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE },
        { PEN_TEX_FORMAT_D24_UNORM_S8_UINT, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8 },
        { PEN_TEX_FORMAT_R32G32B32A32_FLOAT, GL_RGBA32F, GL_RGBA, GL_FLOAT },
        { PEN_TEX_FORMAT_R16G16B16A16_FLOAT, GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT },
        { PEN_TEX_FORMAT_R32_FLOAT, GL_R32F, GL_RED, GL_FLOAT },
        { PEN_TEX_FORMAT_R16_FLOAT, GL_R16F, GL_RED, GL_HALF_FLOAT },
        { PEN_TEX_FORMAT_R32_UINT, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT }
    };
    static u32 k_num_tex_maps = sizeof(k_tex_format_map) / sizeof(k_tex_format_map[0]);
    
    void get_texture_format( u32 pen_format, u32& sized_format, u32& format, u32& type )
    {
        //todo compressed textures
        //PEN_FORMAT_B8G8R8A8_UNORM       = 0,
        //PEN_FORMAT_BC1_UNORM            = 1,
        //PEN_FORMAT_BC2_UNORM            = 2,
        //PEN_FORMAT_BC3_UNORM            = 3,
        //PEN_FORMAT_BC4_UNORM            = 4,
        //PEN_FORMAT_BC5_UNORM            = 5
        
        for( s32 i = 0; i < k_num_tex_maps; ++i )
        {
            if( k_tex_format_map[i].pen_format == pen_format )
            {
                sized_format = k_tex_format_map[i].sized_format;
                format = k_tex_format_map[i].format;
                type = k_tex_format_map[i].type;
                return;
            }
        }
        
        PEN_PRINTF("unimplemented texture format");
        PEN_ASSERT( 0 );
    }
    
    texture_info create_texture_internal(const texture_creation_params& tcp)
    {
        u32 sized_format, format, type;
        get_texture_format( tcp.format, sized_format, format, type );
        
        u32 mip_w = tcp.width;
        u32 mip_h = tcp.height;
        c8* mip_data = (c8*)tcp.data;
        
        bool is_msaa = tcp.sample_count > 1;
        
        u32 texture_target = is_msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        
        GLuint handle;
        glGenTextures( 1, &handle);
        glBindTexture( texture_target, handle );
        
        for( u32 mip = 0; mip < tcp.num_mips; ++mip )
        {
            if( is_msaa )
                glTexImage2DMultisample( texture_target, tcp.sample_count, sized_format, mip_w, mip_h, GL_TRUE );
            else
                glTexImage2D(GL_TEXTURE_2D, mip, sized_format, mip_w, mip_h, 0, format, type, mip_data);
            
            mip_data += calc_mip_level_size(mip_w, mip_h, tcp.block_size, tcp.pixels_per_block);
            
            mip_w /= 2;
            mip_h /= 2;
        }
        
        glBindTexture(texture_target, 0 );
        
        texture_info ti;
        ti.handle = handle;
        ti.max_mip_level = tcp.num_mips - 1;
        
        if( tcp.width != tcp.height && ti.max_mip_level > 0 )
        {
            ti.max_mip_level = tcp.num_mips - 2;
        }
        
        return ti;
    }
    
	void direct::renderer_create_render_target( const texture_creation_params& tcp, u32 resource_slot, bool track )
	{
        resource_allocation& res = resource_pool[ resource_slot ];
        
        res.type = RES_RENDER_TARGET;
        
        texture_creation_params _tcp = tcp;
        
        res.render_target.uid = (u32)timer_get_time();
        
        if( tcp.width == PEN_BACK_BUFFER_RATIO )
        {
            _tcp.width = pen_window.width / tcp.height;
            _tcp.height = pen_window.height / tcp.height;
            
            if(track)
                k_managed_render_targets.push_back({tcp, resource_slot});
        }
        
        //null handles
        res.render_target.texture_msaa.handle = 0;
        res.render_target.texture.handle = 0;
        
        if( tcp.sample_count > 1 )
        {
            res.type = RES_RENDER_TARGET_MSAA;
            
            res.render_target.texture_msaa = create_texture_internal(_tcp);
            
            res.render_target.tcp = new texture_creation_params;
            *res.render_target.tcp = tcp;
        }
        else
        {
            //non-msaa
            texture_creation_params tcp_no_msaa = _tcp;
            tcp_no_msaa.sample_count = 1;
            
            res.render_target.texture = create_texture_internal(tcp_no_msaa);
        }
	}
    
	void direct::renderer_set_targets( const u32* const colour_targets, u32 num_colour_targets, u32 depth_target, u32 colour_face, u32 depth_face )
	{
        static GLenum k_draw_buffers[PEN_MAX_MRT] =
        {
            GL_COLOR_ATTACHMENT0,
            GL_COLOR_ATTACHMENT1,
            GL_COLOR_ATTACHMENT2,
            GL_COLOR_ATTACHMENT3,
            GL_COLOR_ATTACHMENT4,
            GL_COLOR_ATTACHMENT5,
            GL_COLOR_ATTACHMENT6,
            GL_COLOR_ATTACHMENT7,
        };
        
        bool use_back_buffer = false;
        
        if( depth_target == PEN_BACK_BUFFER_DEPTH )
            use_back_buffer = true;
        
        if( num_colour_targets )
            if( colour_targets[0] == 0 )
                use_back_buffer = true;
        
        if( use_back_buffer )
        {
            g_current_state.backbuffer_bound = true;
            
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDrawBuffers(1, k_draw_buffers);
            
            return;
        }
        
        g_current_state.backbuffer_bound = false;
        
        bool msaa = false;
        
        hash_murmur hh;
        hh.begin();
        
        if( depth_target != PEN_NULL_DEPTH_BUFFER )
        {
            resource_allocation& depth_res = resource_pool[ depth_target ];
            hh.add(depth_res.render_target.uid);
        }
        
        for( s32 i = 0; i < num_colour_targets; ++i )
        {
            resource_allocation& colour_res = resource_pool[ colour_targets[i] ];
            hh.add(colour_res.render_target.uid);
            
            if( colour_res.type == RES_RENDER_TARGET_MSAA )
                msaa = true;
        }
        
        hh.add(colour_face);
        hh.add(depth_face);
        hash_id h = hh.end();
        
        for( auto& fb : k_framebuffers )
        {
            if( fb.hash == h)
            {
                glBindFramebuffer( GL_FRAMEBUFFER, fb.framebuffer );
                glDrawBuffers( num_colour_targets, k_draw_buffers );
                
                return;
            }
        }
        
        GLuint fbh;
        glGenFramebuffers(1, &fbh);
        glBindFramebuffer(GL_FRAMEBUFFER, fbh);
        glDrawBuffers( num_colour_targets, k_draw_buffers );
        
        if( depth_target != PEN_NULL_DEPTH_BUFFER )
        {
            resource_allocation& depth_res = resource_pool[ depth_target ];
            
            if( msaa )
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depth_res.render_target.texture_msaa.handle, 0 );
            else
                glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, depth_res.render_target.texture.handle, 0);
        }

        for( s32 i = 0; i < num_colour_targets; ++i )
        {
            resource_allocation& colour_res = resource_pool[ colour_targets[i] ];
            
            if( msaa )
                glFramebufferTexture2D( GL_FRAMEBUFFER, k_draw_buffers[i], GL_TEXTURE_2D_MULTISAMPLE, colour_res.render_target.texture_msaa.handle, 0 );
            else
                glFramebufferTexture(GL_FRAMEBUFFER, k_draw_buffers[i], colour_res.render_target.texture.handle, 0);
        }
        
        GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
        PEN_ASSERT( status == GL_FRAMEBUFFER_COMPLETE );
        
        framebuffer new_fb;
        new_fb.hash = h;
        new_fb.framebuffer = fbh;
        
        k_framebuffers.push_back(new_fb);
	}
        
    extern resolve_resources g_resolve_resources;
    void direct::renderer_resolve_target( u32 target, e_msaa_resolve_type type )
    {
        resource_allocation& colour_res = resource_pool[ target ];
        
        hash_id hash[2] = { 0, 0 };
        
        f32 w = colour_res.render_target.tcp->width;
        f32 h = colour_res.render_target.tcp->height;
        
        if (colour_res.render_target.tcp->width == -1)
        {
            w = pen_window.width / h;
            h = pen_window.height / h;
        }
        
        if( colour_res.render_target.texture.handle == 0 )
        {
            texture_creation_params& _tcp = *colour_res.render_target.tcp;
            _tcp.sample_count = 0;
            _tcp.width = w;
            _tcp.height = h;
            
            if( _tcp.format == PEN_TEX_FORMAT_D24_UNORM_S8_UINT )
                _tcp.format = PEN_TEX_FORMAT_R32_FLOAT;
            
            colour_res.render_target.texture = create_texture_internal(*colour_res.render_target.tcp);
        }
        
        hash_murmur hh;
        hh.add(target);
        hh.add(colour_res.render_target.texture_msaa.handle);
        hh.add(1);
        hh.add(colour_res.render_target.uid);
        hash[0] = hh.end();
        
        hh.begin();
        hh.add(target);
        hh.add(colour_res.render_target.texture.handle);
        hh.add(1);
        hh.add(colour_res.render_target.uid);
        hash[1] = hh.end();
        
        GLuint fbos[2] = { 0, 0 };
        for( auto& fb : k_framebuffers )
            for( s32 i = 0; i < 2; ++i )
                if( fb.hash == hash[i] )
                    fbos[i] = fb.framebuffer;

        for( s32 i = 0; i < 2; ++i )
        {
            if( fbos[i] == 0 )
            {
                glGenFramebuffers(1, &fbos[i]);
                glBindFramebuffer(GL_FRAMEBUFFER, fbos[i]);
                
                if( i == 0 ) //src msaa
                    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, colour_res.render_target.texture_msaa.handle, 0 );
                else
                    glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colour_res.render_target.texture.handle, 0);
            }
            
            framebuffer fb = {hash[i], fbos[i]};
            k_framebuffers.push_back(fb);
        }
        
        if( type == pen::RESOLVE_CUSTOM )
        {
            resolve_cbuffer cbuf = { w, h, 0.0f, 0.0f };
            
            glBindFramebuffer(GL_FRAMEBUFFER, fbos[1]);
            
            direct::renderer_update_buffer(g_resolve_resources.constant_buffer, &cbuf, sizeof(cbuf), 0);
            direct::renderer_set_constant_buffer(g_resolve_resources.constant_buffer, 0, PEN_SHADER_TYPE_PS);
            
            pen::viewport vp = { 0.0f, 0.0f, w, h, 0.0f, 1.0f };
            direct::renderer_set_viewport(vp);
            
            u32 stride = 24;
            u32 offset = 0;
            direct::renderer_set_vertex_buffer(g_resolve_resources.vertex_buffer, 0, 1, &stride, &offset);
            direct::renderer_set_index_buffer(g_resolve_resources.index_buffer, PEN_FORMAT_R16_UINT, 0);
            
            direct::renderer_set_texture(target, 0, 0, PEN_SHADER_TYPE_PS, pen::TEXTURE_BIND_MSAA);
            
            direct::renderer_draw_indexed(6, 0, 0, PEN_PT_TRIANGLELIST);
        }
        else
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1] );
            glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0] );
            
            glBlitFramebuffer(0, 0, (u32)w, (u32)h, 0, 0, (u32)w, (u32)h, GL_COLOR_BUFFER_BIT, GL_LINEAR );
        }
    }

	void direct::renderer_create_texture(const texture_creation_params& tcp, u32 resource_slot )
	{
        resource_pool[ resource_slot ].type = RES_TEXTURE;
        resource_pool[ resource_slot ].texture = create_texture_internal( tcp );
	}

	void direct::renderer_create_sampler( const sampler_creation_params& scp, u32 resource_slot )
	{
        resource_pool[ resource_slot ].sampler_state = (sampler_creation_params*)pen::memory_alloc(sizeof(scp));
        
        pen::memory_cpy(resource_pool[ resource_slot ].sampler_state, &scp, sizeof(scp));
	}

	void direct::renderer_set_texture( u32 texture_index, u32 sampler_index, u32 resource_slot, u32 shader_type, u32 flags )
	{
        if( texture_index == 0)
            return;
        
        resource_allocation& res = resource_pool[ texture_index ];
        
        glActiveTexture(GL_TEXTURE0 + resource_slot);
        
        u32 max_mip = 0;
        
        u32 target = GL_TEXTURE_2D;
        
        if( res.type == RES_TEXTURE )
        {
            glBindTexture( target, res.texture.handle );
            max_mip = res.texture.max_mip_level;
        }
        else
        {
            if( flags & TEXTURE_BIND_MSAA )
            {
                target = GL_TEXTURE_2D_MULTISAMPLE;
                glBindTexture( target, res.render_target.texture_msaa.handle );
            }
            else
            {
                glBindTexture( target, res.render_target.texture.handle );
            }
            
            max_mip = res.render_target.texture_msaa.max_mip_level;
        }
        
        if(sampler_index == 0)
            return;
        
        auto* sampler_state = resource_pool[sampler_index].sampler_state;
        
        //handle unmipped textures or textures with missisng mips
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, max_mip);
        
        if( !sampler_state )
            return;
        
        // filter
        switch( sampler_state->filter )
        {
            case PEN_FILTER_MIN_MAG_MIP_LINEAR:
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
            case PEN_FILTER_MIN_MAG_MIP_POINT:
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_POINT);
                break;
            case PEN_FILTER_LINEAR:
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                break;
            case PEN_FILTER_POINT:
                glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_POINT);
                glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_POINT);
                break;
        };
        
        //address mode
        glTexParameteri(target, GL_TEXTURE_WRAP_S, sampler_state->address_u );
        glTexParameteri(target, GL_TEXTURE_WRAP_T, sampler_state->address_v );
        glTexParameteri(target, GL_TEXTURE_WRAP_R, sampler_state->address_w );
        
        //mip control
        glTexParameterf(target, GL_TEXTURE_LOD_BIAS, sampler_state->mip_lod_bias );
        
        if( sampler_state->max_lod > -1.0f )
        {
            glTexParameterf(target, GL_TEXTURE_MAX_LOD, sampler_state->max_lod );
        }
        
        if( sampler_state->min_lod > -1.0f )
        {
            glTexParameterf(target, GL_TEXTURE_MIN_LOD, sampler_state->min_lod );
        }
        
        CHECK_GL_ERROR;
	}

	void direct::renderer_create_rasterizer_state( const rasteriser_state_creation_params &rscp, u32 resource_slot )
	{
        auto& rs = resource_pool[resource_slot].raster_state;
        
        rs = { 0 };
        
        if( rscp.cull_mode != PEN_CULL_NONE )
        {
            rs.culling_enabled = true;
            rs.cull_face = rscp.cull_mode;
        }
        
        rs.front_ccw = rscp.front_ccw;
        
        rs.depth_clip_enabled = rscp.depth_clip_enable;
        rs.scissor_enabled = rscp.scissor_enable;
        
        rs.polygon_mode = rscp.fill_mode;
	}

	void direct::renderer_set_rasterizer_state( u32 rasterizer_state_index )
	{
        g_current_state.raster_state = rasterizer_state_index;
	}

    viewport g_current_vp;
	void direct::renderer_set_viewport( const viewport &vp )
	{
        g_current_vp = vp;
        
        glViewport( vp.x, vp.y, vp.width, vp.height );
        glDepthRangef( vp.min_depth, vp.max_depth );
	}
    
    void direct::renderer_set_scissor_rect( const rect &r )
    {
        f32 top = g_current_vp.height - r.bottom;
        glScissor(r.left, top, r.right - r.left, r.bottom - r.top);
    }

	void direct::renderer_create_blend_state( const blend_creation_params &bcp, u32 resource_slot )
	{
        resource_pool[ resource_slot ].blend_state = (blend_creation_params*)pen::memory_alloc( sizeof(blend_creation_params) );
        
        blend_creation_params* blend_state = resource_pool[ resource_slot ].blend_state;
        
        *blend_state = bcp;
        
        blend_state->render_targets = (render_target_blend*)pen::memory_alloc( sizeof(render_target_blend) * bcp.num_render_targets );
        
        for( s32 i = 0; i < bcp.num_render_targets; ++i )
        {
            blend_state->render_targets[i] = bcp.render_targets[i];
        }
	}

	void direct::renderer_set_blend_state( u32 blend_state_index )
	{
        auto* blend_state = resource_pool[ blend_state_index ].blend_state;
        
        for( s32 i = 0; i < blend_state->num_render_targets; ++i )
        {
            auto& rt_blend = blend_state->render_targets[ i ];
            
            if( i == 0 )
            {
                if( rt_blend.blend_enable )
                {
                    glEnable(GL_BLEND);
                    
                    if( blend_state->independent_blend_enable )
                    {
                        glBlendFuncSeparate(rt_blend.src_blend, rt_blend.dest_blend, rt_blend.src_blend_alpha, rt_blend.dest_blend_alpha);
                        glBlendEquationSeparate(rt_blend.blend_op, rt_blend.blend_op_alpha);
                    }
                    else
                    {
                        glBlendFunc(rt_blend.src_blend, rt_blend.dest_blend);
                        glBlendEquation(rt_blend.blend_op);
                    }
                }
                else
                {
                    glDisable(GL_BLEND);
                }
            }
        }
	}

	void direct::renderer_set_constant_buffer( u32 buffer_index, u32 resource_slot, u32 shader_type )
	{
        resource_allocation& res = resource_pool[ buffer_index ];
        
        glBindBufferBase(GL_UNIFORM_BUFFER, resource_slot, res.handle );
        
        //g_current_state.constant_buffer_bindings[ resource_slot ] = res.handle;
	}

	void direct::renderer_update_buffer( u32 buffer_index, const void* data, u32 data_size, u32 offset )
	{
        resource_allocation& res = resource_pool[ buffer_index ];
        
        glBindBuffer( res.type, res.handle );
        
        void* mapped_data = glMapBuffer( res.type, GL_WRITE_ONLY );
        
        if( mapped_data )
        {
            c8* mapped_offset = ((c8*)mapped_data) + offset;
            pen::memory_cpy(mapped_offset, data, data_size);
        }
        
        glUnmapBuffer( res.type );
        
        glBindBuffer( res.type, 0 );
	}
    
    void direct::renderer_read_back_resource( const resource_read_back_params& rrbp )
    {
        resource_allocation& res = resource_pool[ rrbp.resource_index ];
        
        GLuint t = res.type;
        if( t == RES_TEXTURE || t == RES_RENDER_TARGET || t == RES_RENDER_TARGET_MSAA )
        {
            u32 sized_format, format, type;
            get_texture_format( rrbp.format, sized_format, format, type );
            
            s32 target_handle = res.texture.handle ;
            if( t == RES_RENDER_TARGET || t == RES_RENDER_TARGET_MSAA )
                target_handle = res.render_target.texture.handle;

            glBindTexture( GL_TEXTURE_2D, res.texture.handle );
            
            void* data = pen::memory_alloc(rrbp.data_size);
            glGetTexImage( GL_TEXTURE_2D, 0, format, type, data );
            
            rrbp.call_back_function( data, rrbp.row_pitch, rrbp.depth_pitch, rrbp.block_size );
            
            pen::memory_free(data);
        }
        else if( t == GL_ELEMENT_ARRAY_BUFFER || t == GL_UNIFORM_BUFFER || t == GL_ARRAY_BUFFER )
        {
            glBindBuffer(t, res.handle );
            void* map = glMapBuffer(t, GL_READ_ONLY);
            
            rrbp.call_back_function( map, rrbp.row_pitch, rrbp.depth_pitch, rrbp.block_size );
            
            glUnmapBuffer(t);
        }
    }

	void direct::renderer_create_depth_stencil_state( const depth_stencil_creation_params& dscp, u32 resource_slot )
	{
        resource_pool[ resource_slot ].depth_stencil = (depth_stencil_creation_params*)pen::memory_alloc(sizeof(dscp));
        
        pen::memory_cpy( resource_pool[ resource_slot ].depth_stencil, &dscp, sizeof(dscp));
	}

	void direct::renderer_set_depth_stencil_state( u32 depth_stencil_state )
	{
        resource_allocation& res = resource_pool[ depth_stencil_state ];
        
        if( res.depth_stencil->depth_enable )
        {
            glEnable(GL_DEPTH_TEST);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
        
        glDepthFunc(res.depth_stencil->depth_func);
        glDepthMask(res.depth_stencil->depth_write_mask);
	}

	void direct::renderer_release_shader( u32 shader_index, u32 shader_type )
    {
        resource_allocation& res = resource_pool[ shader_index ];
        glDeleteShader( res.handle );
        
        res.handle = 0;
	}

	void direct::renderer_release_buffer( u32 buffer_index )
	{
        resource_allocation& res = resource_pool[ buffer_index ];
        glDeleteBuffers(1, &res.handle);
        
        res.handle = 0;
	}

	void direct::renderer_release_texture( u32 texture_index )
	{
        resource_allocation& res = resource_pool[ texture_index ];
        glDeleteTextures(1, &res.handle);
        
        res.handle = 0;
	}

	void direct::renderer_release_raster_state( u32 raster_state_index )
	{
        
	}

	void direct::renderer_release_blend_state( u32 blend_state )
	{
        resource_allocation& res = resource_pool[ blend_state ];
        
        if(res.blend_state)
        {
            //clear rtb
            pen::memory_free(res.blend_state->render_targets);
            res.blend_state->render_targets = nullptr;
            
            pen::memory_free(res.blend_state);
            
            res.blend_state = nullptr;
        }

	}

	void direct::renderer_release_render_target( u32 render_target )
	{
        //remove from managed rt
        for (s32 i = k_managed_render_targets.size() - 1; i >= 0; --i)
        {
            if (k_managed_render_targets[i].render_target_handle == render_target)
            {
                k_managed_render_targets.erase(k_managed_render_targets.begin() + i);
                break;
            }
        }
        
        resource_allocation& res = resource_pool[ render_target ];
        
        if( res.render_target.texture.handle > 0)
            glDeleteTextures( 1, &res.render_target.texture.handle );
        
        if( res.render_target.texture_msaa.handle > 0)
            glDeleteTextures( 1, &res.render_target.texture_msaa.handle );

        delete res.render_target.tcp;
	}

	void direct::renderer_release_input_layout( u32 input_layout )
	{
        resource_allocation& res = resource_pool[ input_layout ];
        
        pen::memory_free(res.input_layout);
	}

	void direct::renderer_release_sampler( u32 sampler )
	{
        resource_allocation& res = resource_pool[ sampler ];
        
        pen::memory_free(res.sampler_state);
	}

	void direct::renderer_release_depth_stencil_state( u32 depth_stencil_state )
	{
        resource_allocation& res = resource_pool[ depth_stencil_state ];
        
        pen::memory_free( res.depth_stencil );
	}
    
    void direct::renderer_release_clear_state( u32 clear_state )
    {

    }
    
    void direct::renderer_release_program( u32 program )
    {
        resource_allocation& res = resource_pool[ program ];
        
        glDeleteProgram(res.shader_program->program);
    }
    
    void direct::renderer_replace_resource(u32 dest, u32 src, e_renderer_resource type)
    {
        switch (type)
        {
            case RESOURCE_TEXTURE:
                direct::renderer_release_texture(dest);
                break;
            case RESOURCE_BUFFER:
                direct::renderer_release_buffer(dest);
                break;
            case RESOURCE_VERTEX_SHADER:
                direct::renderer_release_shader(dest, PEN_SHADER_TYPE_VS);
                break;
            case RESOURCE_PIXEL_SHADER:
                direct::renderer_release_shader(dest, PEN_SHADER_TYPE_PS);
                break;
            default:
                break;
        }
        
        resource_pool[dest] = resource_pool[src];
    }

	void direct::renderer_release_query( u32 query )
	{

	}
    
    void direct::renderer_update_queries()
	{

	}
    
    u32 direct::renderer_initialise( void*, u32, u32 )
    {
        //todo renderer caps
        //const GLubyte* version = glGetString(GL_SHADING_LANGUAGE_VERSION);
        
        return PEN_ERR_OK;
    }
    
    void direct::renderer_shutdown( )
    {
        //todo device / stray resource shutdown
    }
    
    const c8* renderer_get_shader_platform( )
    {
        return "glsl";
    }
    
    bool renderer_viewport_vup( )
    {
        return true;
    }
}

