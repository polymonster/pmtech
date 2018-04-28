#include <stdlib.h>

#include "renderer.h"
#include "memory.h"
#include "pen_string.h"
#include "threads.h"
#include "timer.h"
#include "pen.h"
#include "hash.h"
#include "str/Str.h"
#include <vector>

#include "console.h"
#include "data_struct.h"


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
extern pen::window_creation_params pen_window;

extern void pen_make_gl_context_current( );
extern void pen_gl_swap_buffers( );
extern void pen_window_resize( );

a_u8 g_window_resize( 0 );

namespace
{
    static u64 k_frame = 0;

#define GL_DEBUG_LEVEL 2

#if GL_DEBUG_LEVEL > 1
#define GL_ASSERT(V) PEN_ASSERT(V)
#else
#define GL_ASSERT(V) (void)V
#endif

	void gl_error_break(GLenum err)
	{
		switch (err)
		{
		case GL_INVALID_ENUM:
			printf("invalid enum\n");
			GL_ASSERT(0);
			break;
		case GL_INVALID_VALUE:
			printf("gl invalid value\n");
			GL_ASSERT(0);
			break;
		case GL_INVALID_OPERATION:
			printf("gl invalid operation\n");
			GL_ASSERT(0);
			break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			printf("gl invalid frame buffer operation\n");
			GL_ASSERT(0);
			break;
		case GL_OUT_OF_MEMORY:
			printf("gl out of memory\n");
			GL_ASSERT(0);
			break;
		default:
			break;
		}
	}

#if GL_DEBUG_LEVEL > 0
#define CHECK_GL_ERROR { GLenum err = glGetError( ); if( err != GL_NO_ERROR ) gl_error_break( err ); }
#define CHECK_CALL( C ) C; CHECK_GL_ERROR
#else
#define CHECK_GL_ERROR
#endif
}

namespace pen
{
    //--------------------------------------------------------------------------------------
    //  PERF MARKER API
    //--------------------------------------------------------------------------------------
    struct gpu_perf_result
    {
        u64         elapsed;
        u32         depth;
		u64         frame;
        Str         name;
    };
    gpu_perf_result* k_perf_results = nullptr;
    
    struct perf_marker
    {
        u32         query   = 0;
		u64         frame   = 0;
        const c8*   name    = nullptr;
        bool        issued  = false;
        u32         depth   = 0;
        bool        pad     = false;
        GLuint64    result  = 0;
    };
    
    struct perf_marker_set
    {
        static const u32    num_marker_buffers = 2;
        perf_marker*        markers[num_marker_buffers] = { 0 };
        u32                 pos[num_marker_buffers] = { 0 };
        
        u32                 buf = 0;
        u32                 depth = 0;
    };
    static perf_marker_set k_perf;

    void insert_marker( const c8* name, bool pad = false )
    {
        u32& buf = k_perf.buf;
        u32& pos = k_perf.pos[buf];
        u32& depth = k_perf.depth;
        
        if( pos >= sb_count(k_perf.markers[buf]) )
        {
            //push a new marker
            perf_marker m;
            glGenQueries(1, &m.query);
            
            sb_push( k_perf.markers[buf], m );
        }
        
        //queries have taken longer than 1 frame to obtain results
        //increase num_marker_buffers to avoid losing data
        PEN_ASSERT( !k_perf.markers[buf][pos].issued );
        
        CHECK_CALL( glBeginQuery(GL_TIME_ELAPSED, k_perf.markers[buf][pos].query) );
        k_perf.markers[buf][pos].issued = true;
        k_perf.markers[buf][pos].depth = depth;
        k_perf.markers[buf][pos].frame = k_frame;
        k_perf.markers[buf][pos].pad = pad;
        k_perf.markers[buf][pos].name = name;
        
        ++pos;
    }
    
    void direct::renderer_push_perf_marker( const c8* name )
    {
        u32& depth = k_perf.depth;
        
        if(depth > 0)
        {
            CHECK_CALL( glEndQuery(GL_TIME_ELAPSED) );
        }
        
        
        insert_marker( name );
        
        ++depth;
    }
    
    void direct::renderer_pop_perf_marker( )
    {
        u32& depth = k_perf.depth;
        --depth;
        
        CHECK_CALL( glEndQuery(GL_TIME_ELAPSED) );

        if(depth > 0)
        {
            insert_marker( "pad_marker", true );
        }
    }
    
    void gather_perf_markers( )
    {
        //unbalance push pop in perf markers
        PEN_ASSERT( k_perf.depth == 0 );
        
        //swap buffers
        u32 bb = (k_perf.buf + 1) % k_perf.num_marker_buffers;
        
        u32 complete_count = 0;
        for( u32 i = 0; i < k_perf.pos[bb]; ++i )
        {
            perf_marker& m = k_perf.markers[bb][i];
            if( m.issued )
            {
                s32 avail = 0;
                CHECK_CALL( glGetQueryObjectiv(m.query, GL_QUERY_RESULT_AVAILABLE, &avail) );
                
                if(avail)
                {
                    complete_count++;
                    CHECK_CALL( glGetQueryObjectui64v(m.query, GL_QUERY_RESULT, &m.result) );
                }
            }
        }
        
        if(complete_count == k_perf.pos[bb])
        {
            //gather results into a better view
            sb_free(k_perf_results);
            
            u32 num_timers = k_perf.pos[bb];
            for( u32 i = 0; i < num_timers; ++i )
            {
                perf_marker& m = k_perf.markers[bb][i];
                
                if(!m.issued)
                    continue;
                
                //ready for the next frame
                m.issued = false;
                
                if( m.pad )
                    continue;
                    
                gpu_perf_result p;
                p.name = m.name;
                p.depth = m.depth;
                p.frame = m.frame;
                
                if(m.name)
                {
                    //release mem that was allocated by the command buffer
                    memory_free((void*)m.name);
                    m.name = nullptr;
                }
                
                //gather up times from nested calls
                p.elapsed = 0;
                u32 nest_iter = 0;
                for(;;)
                {
                    perf_marker& n = k_perf.markers[bb][i+nest_iter];

                    if( n.depth < p.depth )
                        break;
                    
                    if( n.pad && p.depth == n.depth)
                        break;
                    
                    p.elapsed += n.result;
                    
                    ++nest_iter;
                    if( nest_iter >= num_timers)
                        break;
                }
                
                Str desc;
                for( u32 j = 0; j < p.depth; ++j )
                    desc.append('\t');
                
                desc.append(p.name.c_str());
                
                desc.append(" : ");
                desc.appendf("%llu", p.elapsed);
                
                //PEN_PRINTF("%s", desc.c_str());
            }
            
            k_perf.pos[bb] = 0;
        }
        
        k_perf.buf = bb;
    }
    
	//--------------------------------------------------------------------------------------
	//  COMMON API
	//--------------------------------------------------------------------------------------
	#define MAX_VERTEX_BUFFERS      4
	#define NUM_CUBEMAP_FACES		6
    #define MAX_VERTEX_ATTRIBUTES   16
    #define MAX_UNIFORM_BUFFERS     16
    #define MAX_SHADER_TEXTURES     32
 
    struct context_state
	{
		context_state()
		{
		}

		u32 backbuffer_colour;
		u32 backbuffer_depth;

		u32 active_colour_target;
		u32 active_depth_target;
	};

    struct clear_state_internal
	{
		f32 rgba[ 4 ];
		f32 depth;
        u8 stencil;
		u32 flags;
        
        pen::mrt_clear mrt[MAX_MRT];
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
        vertex_attribute* attributes;
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
        u32 target;
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
    static framebuffer* k_framebuffers;
    
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
        u32 so;
        GLuint program;
        u8 uniform_block_location[MAX_UNIFORM_BUFFERS];
        u8 texture_location[MAX_SHADER_TEXTURES];
    };
    static shader_program* k_shader_programs;
    
    struct managed_render_target
    {
        texture_creation_params tcp;
        u32 render_target_handle;
    };
    static managed_render_target* k_managed_render_targets;
    
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
    
    struct active_state
    {
        u32 vertex_buffer[MAX_VERTEX_BUFFERS] = { 0 };
        u32 vertex_buffer_stride[MAX_VERTEX_BUFFERS] = { 0 };
        u32 vertex_buffer_offset[MAX_VERTEX_BUFFERS] = { 0 };
        u32 num_bound_vertex_buffers = 0;
        u32 index_buffer = 0;
        u32 stream_out_buffer = 0;
        u32 input_layout = 0;
        u32 vertex_shader = 0;
        u32 pixel_shader = 0;
        u32 stream_out_shader = 0;
        u32 raster_state = 0;
        bool backbuffer_bound = false;
        u8 constant_buffer_bindings[MAX_UNIFORM_BUFFERS] = { 0 };
        u32 index_format = GL_UNSIGNED_SHORT;
    };
    
    active_state g_bound_state;
    active_state g_current_state;

	void clear_resource_table( )
	{
		pen::memory_zero( &resource_pool[ 0 ], sizeof( resource_allocation ) * MAX_RENDERER_RESOURCES );
		
		//reserve resource 0 for NULL binding.
		resource_pool[0].asigned_flag |= 0xff;
	}
    
	context_state g_context;

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
        pen::memory_cpy(&resource_pool[ resource_slot ].clear_state.mrt, cs.mrt, sizeof(pen::mrt_clear)*MAX_MRT);
	}

    u32 link_program_internal( u32 vs, u32 ps, const pen::shader_link_params* params = nullptr )
    {
        //link the shaders
        GLuint program_id = CHECK_CALL( glCreateProgram() );
        
        GLuint so = 0;
        
        if( params )
        {
            //this path is for a proper link with reflection info
            vs = resource_pool[ params->vertex_shader ].handle;
            ps = resource_pool[ params->pixel_shader ].handle;
            so = resource_pool[ params->stream_out_shader ].handle;
            
            if(vs)
            {
                CHECK_CALL( glAttachShader(program_id, vs) );
            }
            
            if(ps)
            {
                CHECK_CALL( glAttachShader(program_id, ps) );
            }
            
            if(so)
            {
                CHECK_CALL( glAttachShader(program_id, so) );
                CHECK_CALL( glTransformFeedbackVaryings(
                                            program_id,
                                            params->num_stream_out_names,
                                            params->stream_out_names,
                                            GL_INTERLEAVED_ATTRIBS ) );
            }
        }
        else
        {
            //on the fly link for bound vs and ps which have not been explicity linked
            //to emulate d3d behaviour of set vs set ps etc
            CHECK_CALL( glAttachShader(program_id, vs) );
            CHECK_CALL( glAttachShader(program_id, ps) );
        }

        CHECK_CALL( glLinkProgram(program_id) );
        
        // Check the program
        GLint result = GL_FALSE;
        int info_log_length = 0;
        
        CHECK_CALL( glGetProgramiv(program_id, GL_LINK_STATUS, &result) );
        CHECK_CALL( glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length) );
        
        if ( info_log_length > 0 )
        {
            char* info_log_buf = (char*)pen::memory_alloc(info_log_length + 1);
            
            CHECK_CALL( glGetShaderInfoLog(program_id, info_log_length, NULL, &info_log_buf[0]) );
            info_log_buf[info_log_length] = '\0';
            
            output_debug("%s", info_log_buf);
            pen::memory_free(info_log_buf);
        }
        
        shader_program program;
        program.vs = vs;
        program.ps = ps;
        program.so = so;
        program.program = program_id;
        
        for( s32 i = 0; i < MAX_UNIFORM_BUFFERS; ++i )
        {
            program.texture_location[ i ] = INVALID_LOC;
            program.uniform_block_location[ i ] = INVALID_LOC;
        }
        
        sb_push(k_shader_programs, program);
        
        return sb_count(k_shader_programs)-1;
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

        CHECK_CALL( glClearDepth( cs.depth ) );
        CHECK_CALL( glClearStencil( cs.stencil ) );
        
        if( cs.num_colour_targets == 0 )
        {
            CHECK_CALL( glClearColor(
                                     rc.clear_state.rgba[ 0 ],
                                     rc.clear_state.rgba[ 1 ],
                                     rc.clear_state.rgba[ 2 ],
                                     rc.clear_state.rgba[ 3 ] ));
            
            CHECK_CALL( glClear( rc.clear_state.flags ) );
            return;
        }
        
        u32 masked = rc.clear_state.flags &= ~GL_COLOR_BUFFER_BIT;
        
        CHECK_CALL( glClear( masked ) );
        
        for( s32 i = 0; i < cs.num_colour_targets; ++i )
        {
            if( cs.mrt[i].type == pen::CLEAR_F32 )
            {
                CHECK_CALL( glClearBufferfv( GL_COLOR, i, cs.mrt[i].f ) );
                continue;
            }
            
            if( cs.mrt[i].type == pen::CLEAR_U32 )
            {
                CHECK_CALL( glClearBufferuiv( GL_COLOR,  i, cs.mrt[i].u ) );
                continue;
            }
        }
	}

    static u32 k_resize_counter = 0;
    static bool k_needs_resize = false;
    
    void renderer_resize_managed_targets( )
    {
        u32 num_man_rt = sb_count(k_managed_render_targets);
        for( u32 i = 0; i < num_man_rt; ++i)
        {
            auto& managed_rt = k_managed_render_targets[i];
            
            resource_allocation& res = resource_pool[ managed_rt.render_target_handle ];
            CHECK_CALL( glDeleteTextures( 1, &res.render_target.texture.handle ) );
            
            if( managed_rt.tcp.sample_count > 1 )
            {
                CHECK_CALL( glDeleteTextures( 1, &res.render_target.texture_msaa.handle ) );
                res.render_target.texture_msaa.handle = 0;
            }
            
            direct::renderer_create_render_target(managed_rt.tcp, managed_rt.render_target_handle, false );
        }
        
        sb_free(k_framebuffers);
        k_framebuffers = nullptr;
    }
    
	void direct::renderer_present( )
	{
        pen_gl_swap_buffers();
        
        if( k_frame > 0 )
            renderer_pop_perf_marker(); //gpu total
        
        gather_perf_markers();
        
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
        
        k_frame++;
        
        //gpu total
        renderer_push_perf_marker(nullptr);
	}
    
	void direct::renderer_load_shader(const pen::shader_load_params &params, u32 resource_slot)
	{
        resource_allocation& res = resource_pool[ resource_slot ];
        
        u32 internal_type = params.type;
        if(params.type == PEN_SHADER_TYPE_SO)
            internal_type = PEN_SHADER_TYPE_VS;
        
        res.handle = CHECK_CALL( glCreateShader(internal_type) );
        
        CHECK_CALL( glShaderSource(res.handle, 1, (c8**)&params.byte_code, (s32*)&params.byte_code_size) );
        CHECK_CALL( glCompileShader(res.handle) );
        
        // Check compilation status
        GLint result = GL_FALSE;
        int info_log_length;
        
        CHECK_CALL( glGetShaderiv(res.handle, GL_COMPILE_STATUS, &result) );
        CHECK_CALL( glGetShaderiv(res.handle, GL_INFO_LOG_LENGTH, &info_log_length) );
        
        if ( info_log_length > 0 )
        {
            PEN_PRINTF( "%s", params.byte_code );
            
            char* info_log_buf = (char*)pen::memory_alloc(info_log_length + 1);
            
            CHECK_CALL( glGetShaderInfoLog(res.handle, info_log_length, NULL, &info_log_buf[0]) );
            
            PEN_PRINTF(info_log_buf);
        }
	}

	void direct::renderer_set_shader( u32 shader_index, u32 shader_type )
	{
        if( shader_type == GL_VERTEX_SHADER )
        {
            g_current_state.vertex_shader = shader_index;
            g_current_state.stream_out_shader = 0;
        }
        else if( shader_type == GL_FRAGMENT_SHADER )
        {
            g_current_state.pixel_shader = shader_index;
        }
        else if( shader_type == PEN_SHADER_TYPE_SO )
        {
            g_current_state.stream_out_shader = shader_index;
            g_current_state.vertex_shader = 0;
            g_current_state.pixel_shader = 0;
        }
	}

	void direct::renderer_create_buffer( const buffer_creation_params &params, u32 resource_slot )
	{
        resource_allocation& res = resource_pool[resource_slot];
        
        CHECK_CALL( glGenBuffers(1, &res.handle) );
        
        CHECK_CALL( glBindBuffer(params.bind_flags, res.handle) );
        
        CHECK_CALL( glBufferData(params.bind_flags, params.buffer_size, params.data, params.usage_flags ) );

        res.type = params.bind_flags;
	}
    
    void direct::renderer_link_shader_program(const pen::shader_link_params &params, u32 resource_slot )
    {
        shader_link_params slp = params;
        u32 program_index = link_program_internal( 0, 0, &slp );
        
        shader_program* linked_program = &k_shader_programs[program_index];
        
        GLuint prog = linked_program->program;
        glUseProgram( linked_program->program );
    
        //build lookup tables for uniform buffers and texture samplers
        for( u32 i = 0; i < params.num_constants; ++i )
        {
            constant_layout_desc& constant = params.constants[i];
            GLint loc;
            
            switch( constant.type )
            {
                case pen::CT_CBUFFER:
                {
                    loc = CHECK_CALL( glGetUniformBlockIndex(prog, constant.name) );
                    PEN_ASSERT( loc < MAX_UNIFORM_BUFFERS );
                    
                    linked_program->uniform_block_location[constant.location] = loc;
                    
                    CHECK_CALL( glUniformBlockBinding( prog, loc, constant.location ) );
                }
                break;
                
                case pen::CT_SAMPLER_3D:
                {
                    u32 a = 0;
                }
                    
                case pen::CT_SAMPLER_2D:
                case pen::CT_SAMPLER_2DMS:
                case pen::CT_SAMPLER_CUBE:
                {
                    loc = CHECK_CALL( glGetUniformLocation(prog, constant.name) );
                    
                    linked_program->texture_location[constant.location] = loc;
                    
                    if(loc != -1 && loc != constant.location )
                        CHECK_CALL( glUniform1i( loc, constant.location ) );
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
        g_current_state.stream_out_buffer = buffer_index;
    }
    
    void direct::renderer_draw_auto( )
    {
        
    }

	void direct::renderer_create_input_layout( const input_layout_creation_params &params, u32 resource_slot )
	{
        resource_allocation& res = resource_pool[ resource_slot ];
        
        res.input_layout = new input_layout;
    
        res.input_layout->attributes = nullptr;
        
        for( u32 i = 0; i < params.num_elements; ++i )
        {
            vertex_attribute new_attrib;
            
            new_attrib.location        = i;
            new_attrib.type            = UNPACK_FORMAT(params.input_layout[ i ].format);
            new_attrib.num_elements    = UNPACK_NUM_ELEMENTS(params.input_layout[ i ].format);
            new_attrib.offset          = params.input_layout[ i ].aligned_byte_offset;
            new_attrib.stride          = 0;
            new_attrib.input_slot      = params.input_layout[ i ].input_slot;
            new_attrib.step_rate       = params.input_layout[ i ].instance_data_step_rate;
            
            sb_push(res.input_layout->attributes, new_attrib);
        }
	}

	void direct::renderer_set_vertex_buffer(u32 buffer_index,
                                            u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets )
	{
        g_current_state.vertex_buffer[0] = buffer_index;
        g_current_state.vertex_buffer_stride[0] = strides[ 0 ];
        g_current_state.vertex_buffer_offset[0] = offsets[0];
        g_current_state.num_bound_vertex_buffers = 1;
	}
    
    void direct::renderer_set_vertex_buffers(u32* buffer_indices,
                                             u32 num_buffers, u32 start_slot, const u32* strides, const u32* offsets )
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
        g_bound_state.index_format = format;
	}
    
    void bind_state( u32 primitive_topology )
    {
        //bind shaders
        if( g_current_state.stream_out_shader  != 0 &&
            (g_current_state.stream_out_shader != g_bound_state.stream_out_shader) )
        {
            //stream out
            g_bound_state.vertex_shader = g_current_state.vertex_shader;
            g_bound_state.pixel_shader = g_current_state.pixel_shader;
            g_bound_state.stream_out_shader = g_current_state.stream_out_shader;
            
            shader_program* linked_program = nullptr;
            
            u32 so_handle = resource_pool[g_bound_state.stream_out_shader].handle;
            
            u32 num_shaders = sb_count(k_shader_programs);
            for( s32 i = 0; i < num_shaders; ++i )
            {
                if( k_shader_programs[i].so == so_handle)
                {
                    linked_program = &k_shader_programs[i];
                    break;
                }
            }
            
            if( linked_program == nullptr )
            {
                //we need to link ahead of time to setup the transform feedback varying
                PEN_ASSERT(0);
            }
            
            CHECK_CALL( glUseProgram( linked_program->program ) );
            
            //constant buffers and texture locations
            for( s32 i = 0; i < MAX_UNIFORM_BUFFERS; ++i )
                if( linked_program->texture_location[ i ] != INVALID_LOC )
                    CHECK_CALL( glUniform1i( linked_program->texture_location[ i ], i ) );
            
            CHECK_CALL( glEnable(GL_RASTERIZER_DISCARD) );
        }
        else
        {
            if( g_current_state.vertex_shader != g_bound_state.vertex_shader ||
               g_current_state.pixel_shader != g_bound_state.pixel_shader )
            {
                g_bound_state.vertex_shader = g_current_state.vertex_shader;
                g_bound_state.pixel_shader = g_current_state.pixel_shader;
                g_bound_state.stream_out_shader = g_current_state.stream_out_shader;
                
                shader_program* linked_program = nullptr;
                
                auto vs_handle = resource_pool[g_bound_state.vertex_shader].handle;
                auto ps_handle = resource_pool[g_bound_state.pixel_shader].handle;
                
                u32 num_shaders = sb_count(k_shader_programs);
                for( s32 i = 0; i < num_shaders; ++i )
                {
                    if( k_shader_programs[i].vs == vs_handle && k_shader_programs[i].ps == ps_handle )
                    {
                        linked_program = &k_shader_programs[i];
                        break;
                    }
                }
                
                if( linked_program == nullptr )
                {
                    u32 index = link_program_internal(vs_handle, ps_handle);
                    linked_program = &k_shader_programs[index];
                }
                
                CHECK_CALL( glUseProgram( linked_program->program ) );
                
                //constant buffers and texture locations
                for( s32 i = 0; i < MAX_UNIFORM_BUFFERS; ++i )
                    if( linked_program->texture_location[ i ] != INVALID_LOC )
                        CHECK_CALL( glUniform1i( linked_program->texture_location[ i ], i ) );
            }
        }
        
        g_bound_state.input_layout = g_current_state.input_layout;
        
        auto* input_res = resource_pool[g_current_state.input_layout].input_layout;
        if( input_res->vertex_array_handle == 0 )
        {
            CHECK_CALL( glGenVertexArrays(1, &input_res->vertex_array_handle) );
        }
        
        CHECK_CALL( glBindVertexArray(input_res->vertex_array_handle) );
        
        for( s32 v = 0; v < g_current_state.num_bound_vertex_buffers; ++v )
        {
            g_bound_state.vertex_buffer[v] = g_current_state.vertex_buffer[v];
            g_bound_state.vertex_buffer_stride[v] = g_current_state.vertex_buffer_stride[v];
            
            auto& res = resource_pool[g_bound_state.vertex_buffer[v]].handle;
            
            u32 num_attribs = sb_count(input_res->attributes);
            for(u32 a = 0; a < num_attribs; ++a )
            {
                auto& attribute = input_res->attributes[a];
                
                if( attribute.input_slot != v )
                    continue;
                
                CHECK_CALL( glEnableVertexAttribArray(attribute.location) );
                CHECK_CALL( glBindBuffer(GL_ARRAY_BUFFER, res) );
                
                CHECK_CALL( glVertexAttribPointer(
                                      attribute.location,
                                      attribute.num_elements,
                                      attribute.type,
                                      attribute.type == GL_UNSIGNED_BYTE ? true : false,
                                      g_bound_state.vertex_buffer_stride[v],
                                      (void*)attribute.offset));
                
                CHECK_CALL( glVertexAttribDivisor(attribute.location, attribute.step_rate) );
            }
        }

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
            {
                CHECK_CALL( glFrontFace(GL_CCW) );
            }
            else
            {
                CHECK_CALL( glFrontFace(GL_CW) );
            }
            
            if( rs.culling_enabled )
            {
                CHECK_CALL( glEnable( GL_CULL_FACE ) );
                CHECK_CALL( glCullFace(rs.cull_face) );
            }
            else
            {
                CHECK_CALL( glDisable(GL_CULL_FACE) );
            }
            
            if( rs.depth_clip_enabled )
            {
                CHECK_CALL( glDisable(GL_DEPTH_CLAMP) );
            }
            else
            {
                CHECK_CALL( glEnable(GL_DEPTH_CLAMP) );
            }
            
            CHECK_CALL( glPolygonMode(GL_FRONT_AND_BACK, rs.polygon_mode) );
            
            if( rs.scissor_enabled )
            {
                CHECK_CALL( glEnable(GL_SCISSOR_TEST) );
            }
            else
            {
                CHECK_CALL( glDisable(GL_SCISSOR_TEST) );
            }
        }
        
        if( g_current_state.stream_out_buffer != g_bound_state.stream_out_buffer )
        {
            g_bound_state.stream_out_buffer = g_current_state.stream_out_buffer;
            
            if( g_bound_state.stream_out_buffer )
            {
                u32 so_buffer = resource_pool[g_bound_state.stream_out_buffer].handle;
                
                CHECK_CALL( glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, so_buffer) );
                CHECK_CALL( glBeginTransformFeedback(primitive_topology) );
            }
        }
    }

	void direct::renderer_draw( u32 vertex_count, u32 start_vertex, u32 primitive_topology )
	{
        bind_state( primitive_topology );
        
        CHECK_CALL( glDrawArrays(primitive_topology, start_vertex, vertex_count) );
        
        if(g_bound_state.stream_out_buffer)
        {
            g_current_state.stream_out_buffer = 0;
            g_bound_state.stream_out_buffer = 0;
            
            CHECK_CALL( glEndTransformFeedback() );
            CHECK_CALL( glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0) );
            CHECK_CALL( glDisable(GL_RASTERIZER_DISCARD) );
        }
	}

	void direct::renderer_draw_indexed( u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology )
	{
        bind_state( primitive_topology );
        
        //bind index buffer -this must always be re-bound
        GLuint res = resource_pool[g_bound_state.index_buffer].handle;
        CHECK_CALL( glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res) );

        void* offset = (void*)(size_t)(start_index * 2);
        
        CHECK_CALL( glDrawElementsBaseVertex( primitive_topology, index_count, g_bound_state.index_format, offset, base_vertex ) );
	}
    
    void direct::renderer_draw_indexed_instanced(
        u32 instance_count,
        u32 start_instance,
        u32 index_count,
        u32 start_index,
        u32 base_vertex,
        u32 primitive_topology )
    {
        bind_state( primitive_topology );
        
        //bind index buffer -this must always be re-bound
        GLuint res = resource_pool[g_bound_state.index_buffer].handle;
        CHECK_CALL( glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res) );
        
        //todo this needs to check index size 32 or 16 bit
        void* offset = (void*)(size_t)(start_index * 2);
        
        CHECK_CALL( glDrawElementsInstancedBaseVertex(primitive_topology,
                                                      index_count,
                                                      g_bound_state.index_format,
                                                      offset,
                                                      instance_count,
                                                      base_vertex) );
    }
    
    u32 calc_mip_level_size( u32 w, u32 h, u32 block_size, u32 pixels_per_block )
    {
        if(block_size != 1)
        {
            u32 block_width = max<u32>(1, ((w + (pixels_per_block-1))/ pixels_per_block));
            u32 block_height = max<u32>(1, ((h + (pixels_per_block-1)) / pixels_per_block));
            return  block_width * block_height * block_size;
        }
        
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
        { PEN_TEX_FORMAT_R32_UINT, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT },
        { PEN_TEX_FORMAT_R8_UNORM, GL_R8, GL_RED, GL_UNSIGNED_BYTE },
        { PEN_TEX_FORMAT_BC1_UNORM, 0, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, GL_TEXTURE_COMPRESSED },
        { PEN_TEX_FORMAT_BC2_UNORM, 0, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, GL_TEXTURE_COMPRESSED },
        { PEN_TEX_FORMAT_BC3_UNORM, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, GL_TEXTURE_COMPRESSED }
    };
    static u32 k_num_tex_maps = sizeof(k_tex_format_map) / sizeof(k_tex_format_map[0]);
    
    void get_texture_format( u32 pen_format, u32& sized_format, u32& format, u32& type )
    {
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
        
        PEN_PRINTF("unimplemented / unsupported texture format");
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
        u32 base_texture_target = texture_target;
        
        u32 num_slices = 1;
        u32 num_arrays = tcp.num_arrays;
        
        switch((pen::texture_collection_type)tcp.collection_type)
        {
            case TEXTURE_COLLECTION_NONE:
                texture_target = is_msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
                base_texture_target = texture_target;
                break;
            case TEXTURE_COLLECTION_CUBE:
                is_msaa = false;
                texture_target = GL_TEXTURE_CUBE_MAP;
                base_texture_target = GL_TEXTURE_CUBE_MAP_POSITIVE_X;
                break;
            case TEXTURE_COLLECTION_VOLUME:
                is_msaa = false;
                num_slices = tcp.num_arrays;
                num_arrays = 1;
                texture_target = GL_TEXTURE_3D;
                base_texture_target = texture_target;
                break;
            default:
                texture_target = is_msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
                base_texture_target = texture_target;
                break;
                
        }

        GLuint handle;
        CHECK_CALL( glGenTextures( 1, &handle) );
        CHECK_CALL( glBindTexture( texture_target, handle ) );
        
        for( u32 a = 0; a < num_arrays; ++a )
        {
            mip_w = tcp.width;
            mip_h = tcp.height;
            
            for( u32 mip = 0; mip < tcp.num_mips; ++mip )
            {
                u32 mip_size = calc_mip_level_size(mip_w, mip_h, tcp.block_size, tcp.pixels_per_block);
                
                if( is_msaa )
                {
                    CHECK_CALL( glTexImage2DMultisample(base_texture_target,
                                                        tcp.sample_count, sized_format, mip_w, mip_h, GL_TRUE ) );
                }
                else
                {
                    if( type == GL_TEXTURE_COMPRESSED )
                    {
                        CHECK_CALL( glCompressedTexImage2D(base_texture_target + a,
                                                           mip, format, mip_w, mip_h, 0, mip_size, mip_data ));
                        
                    }
                    else
                    {
                        if( base_texture_target == GL_TEXTURE_3D )
                        {
                            //3d textures rgba / bgra are reversed?
                            if( format == GL_RGBA )
                            {
                                format = GL_BGRA;
                            }
                            else if( format == GL_BGRA )
                            {
                                format = GL_RGBA;
                            }
                            
                            CHECK_CALL( glTexImage3D(base_texture_target,
                                                     mip, sized_format, mip_w, mip_h, num_slices, 0, format,
                                                     type, mip_data) );
                        }
                        else
                        {
                            CHECK_CALL( glTexImage2D(base_texture_target + a,
                                                     mip, sized_format, mip_w, mip_h, 0, format, type, mip_data));
                        }
                    }
                }
                
                mip_data += mip_size;
                
                mip_w /= 2;
                mip_h /= 2;
                                               
                mip_w = max<u32>(1, mip_w);
                mip_h = max<u32>(1, mip_h);
            }
        }

        CHECK_CALL( glBindTexture(texture_target, 0 ) );
        
        texture_info ti;
        ti.handle = handle;
        ti.max_mip_level = tcp.num_mips - 1;
        ti.target = texture_target;
        
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
        
        res.render_target.uid = (u32)pen::get_time_ms();
        
        if( tcp.width == BACK_BUFFER_RATIO )
        {
            _tcp.width = pen_window.width / tcp.height;
            _tcp.height = pen_window.height / tcp.height;
            
            if(track)
            {
                managed_render_target man_rt = {tcp, resource_slot};
                sb_push(k_managed_render_targets, man_rt);
            }
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
    
	void direct::renderer_set_targets(const u32* const colour_targets,
                                      u32 num_colour_targets, u32 depth_target, u32 colour_face, u32 depth_face )
	{
        static GLenum k_draw_buffers[MAX_MRT] =
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
            
            CHECK_CALL( glBindFramebuffer(GL_FRAMEBUFFER, 0) );
            CHECK_CALL( glDrawBuffer(GL_BACK) );
            
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
            hh.add(colour_targets[i]);
            
            if( colour_res.type == RES_RENDER_TARGET_MSAA )
                msaa = true;
        }
        
        hh.add(colour_face);
        hh.add(depth_face);
        hash_id h = hh.end();
        
        u32 num_fb = sb_count(k_framebuffers);
        for( u32 i = 0; i < num_fb; ++i )
        {
            auto& fb = k_framebuffers[i];
            
            if( fb.hash == h)
            {
                CHECK_CALL( glBindFramebuffer( GL_FRAMEBUFFER, fb.framebuffer ) );
                CHECK_CALL( glDrawBuffers( num_colour_targets, k_draw_buffers ) );
                
                return;
            }
        }
        
        GLuint fbh;
        CHECK_CALL( glGenFramebuffers(1, &fbh) );
        CHECK_CALL( glBindFramebuffer(GL_FRAMEBUFFER, fbh) );
        CHECK_CALL( glDrawBuffers( num_colour_targets, k_draw_buffers ) );
        
        if( depth_target != PEN_NULL_DEPTH_BUFFER )
        {
            resource_allocation& depth_res = resource_pool[ depth_target ];
            
            if( msaa )
            {
                CHECK_CALL( glFramebufferTexture2D(GL_FRAMEBUFFER,
                                                   GL_DEPTH_STENCIL_ATTACHMENT,
                                                   GL_TEXTURE_2D_MULTISAMPLE,
                                                   depth_res.render_target.texture_msaa.handle, 0 ));
            }
            else
            {
                CHECK_CALL( glFramebufferTexture(GL_FRAMEBUFFER,
                                                 GL_DEPTH_STENCIL_ATTACHMENT, depth_res.render_target.texture.handle, 0));
            }
        }

        for( s32 i = 0; i < num_colour_targets; ++i )
        {
            resource_allocation& colour_res = resource_pool[ colour_targets[i] ];
            
            if( msaa )
            {
                CHECK_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER,
                                                  k_draw_buffers[i],
                                                  GL_TEXTURE_2D_MULTISAMPLE,
                                                  colour_res.render_target.texture_msaa.handle, 0 ));
            }
            else
            {
                CHECK_CALL(glFramebufferTexture(GL_FRAMEBUFFER,
                                                k_draw_buffers[i], colour_res.render_target.texture.handle, 0));
            }
        }
        
        GLenum status = CHECK_CALL(glCheckFramebufferStatus( GL_FRAMEBUFFER ));
        PEN_ASSERT( status == GL_FRAMEBUFFER_COMPLETE );
        
        framebuffer new_fb;
        new_fb.hash = h;
        new_fb.framebuffer = fbh;
        
        sb_push(k_framebuffers, new_fb);
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
        u32 num_fb = sb_count(k_framebuffers);
        for( u32 i = 0; i < num_fb; ++i )
        {
            auto& fb = k_framebuffers[i];
            for( s32 i = 0; i < 2; ++i )
                if( fb.hash == hash[i] )
                    fbos[i] = fb.framebuffer;
        }

        for( s32 i = 0; i < 2; ++i )
        {
            if( fbos[i] == 0 )
            {
                CHECK_CALL( glGenFramebuffers(1, &fbos[i]));
                CHECK_CALL( glBindFramebuffer(GL_FRAMEBUFFER, fbos[i]));
                
                if( i == 0 ) //src msaa
                {
                    CHECK_CALL( glFramebufferTexture2D(GL_FRAMEBUFFER,
                                                       GL_COLOR_ATTACHMENT0,
                                                       GL_TEXTURE_2D_MULTISAMPLE,
                                                       colour_res.render_target.texture_msaa.handle, 0 ));
                }
                else
                {
                    CHECK_CALL( glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colour_res.render_target.texture.handle, 0));
                }
                
                framebuffer fb = {hash[i], fbos[i]};
                sb_push(k_framebuffers, fb);
            }
        }
        
        if( type == pen::RESOLVE_CUSTOM )
        {
            resolve_cbuffer cbuf = { w, h, 0.0f, 0.0f };
            
            CHECK_CALL( glBindFramebuffer(GL_FRAMEBUFFER, fbos[1]));
            
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
            CHECK_CALL( glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbos[1] ));
            CHECK_CALL( glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos[0] ));
            
            CHECK_CALL( glBlitFramebuffer(0, 0, (u32)w, (u32)h, 0, 0, (u32)w, (u32)h, GL_COLOR_BUFFER_BIT, GL_LINEAR ));
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
        
        CHECK_CALL( glActiveTexture(GL_TEXTURE0 + resource_slot));
        
        u32 max_mip = 0;
        
        u32 target = resource_pool[ texture_index ].texture.target;
        
        if( res.type == RES_TEXTURE )
        {
            CHECK_CALL( glBindTexture( target, res.texture.handle ));
            max_mip = res.texture.max_mip_level;
        }
        else
        {
            if( flags & TEXTURE_BIND_MSAA )
            {
                target = GL_TEXTURE_2D_MULTISAMPLE;
                CHECK_CALL( glBindTexture( target, res.render_target.texture_msaa.handle ));
            }
            else
            {
                CHECK_CALL( glBindTexture( target, res.render_target.texture.handle ));
            }
            
            max_mip = res.render_target.texture_msaa.max_mip_level;
        }
        
        if(sampler_index == 0)
            return;
        
        auto* sampler_state = resource_pool[sampler_index].sampler_state;
        
        //handle unmipped textures or textures with missisng mips
        CHECK_CALL( glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, max_mip));
        
        if( !sampler_state )
            return;
        
        // filter
        switch( sampler_state->filter )
        {
            case PEN_FILTER_MIN_MAG_MIP_LINEAR:
            {
                CHECK_CALL( glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
                CHECK_CALL( glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            }
            break;
            case PEN_FILTER_MIN_MAG_MIP_POINT:
            {
                CHECK_CALL( glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST));
                CHECK_CALL( glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
            }
            break;
            case PEN_FILTER_LINEAR:
            {
                CHECK_CALL( glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
                CHECK_CALL( glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            }
            break;
            case PEN_FILTER_POINT:
            {
                CHECK_CALL( glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_POINT));
                CHECK_CALL( glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_POINT));
            }
            break;
        };
        
        //address mode
        CHECK_CALL( glTexParameteri(target, GL_TEXTURE_WRAP_S, sampler_state->address_u ));
        CHECK_CALL( glTexParameteri(target, GL_TEXTURE_WRAP_T, sampler_state->address_v ));
        CHECK_CALL( glTexParameteri(target, GL_TEXTURE_WRAP_R, sampler_state->address_w ));
        
        //mip control
        CHECK_CALL( glTexParameterf(target, GL_TEXTURE_LOD_BIAS, sampler_state->mip_lod_bias ));
        
        if( sampler_state->max_lod > -1.0f )
        {
            CHECK_CALL( glTexParameterf(target, GL_TEXTURE_MAX_LOD, sampler_state->max_lod ));
        }
        
        if( sampler_state->min_lod > -1.0f )
        {
            CHECK_CALL( glTexParameterf(target, GL_TEXTURE_MIN_LOD, sampler_state->min_lod ));
        }
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
        
        CHECK_CALL( glViewport( vp.x, vp.y, vp.width, vp.height ));
        CHECK_CALL( glDepthRangef( vp.min_depth, vp.max_depth ));
	}
    
    void direct::renderer_set_scissor_rect( const rect &r )
    {
        f32 top = g_current_vp.height - r.bottom;
        CHECK_CALL( glScissor(r.left, top, r.right - r.left, r.bottom - r.top));
    }

	void direct::renderer_create_blend_state( const blend_creation_params &bcp, u32 resource_slot )
	{
        resource_pool[ resource_slot ].blend_state = (blend_creation_params*)pen::memory_alloc( sizeof(blend_creation_params) );
        
        blend_creation_params* blend_state = resource_pool[ resource_slot ].blend_state;
        
        *blend_state = bcp;
        
        blend_state->render_targets = (render_target_blend*)pen::memory_alloc(sizeof(render_target_blend) *
                                                                              bcp.num_render_targets );
        
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
                    CHECK_CALL( glEnable(GL_BLEND) );
                    
                    if( blend_state->independent_blend_enable )
                    {
                        CHECK_CALL( glBlendFuncSeparate(rt_blend.src_blend,
                                                        rt_blend.dest_blend,
                                                        rt_blend.src_blend_alpha, rt_blend.dest_blend_alpha) );
                        CHECK_CALL( glBlendEquationSeparate(rt_blend.blend_op, rt_blend.blend_op_alpha));
                    }
                    else
                    {
                        CHECK_CALL( glBlendFunc(rt_blend.src_blend, rt_blend.dest_blend) );
                        CHECK_CALL( glBlendEquation(rt_blend.blend_op) );
                    }
                }
                else
                {
                    CHECK_CALL( glDisable(GL_BLEND) );
                }
            }
        }
	}

	void direct::renderer_set_constant_buffer( u32 buffer_index, u32 resource_slot, u32 shader_type )
	{
        resource_allocation& res = resource_pool[ buffer_index ];
        
        CHECK_CALL( glBindBufferBase(GL_UNIFORM_BUFFER, resource_slot, res.handle ) );
	}

	void direct::renderer_update_buffer( u32 buffer_index, const void* data, u32 data_size, u32 offset )
	{
        resource_allocation& res = resource_pool[ buffer_index ];
        
        CHECK_CALL( glBindBuffer( res.type, res.handle ) );
        
        void* mapped_data = CHECK_CALL( glMapBuffer( res.type, GL_WRITE_ONLY ) );
        
        if( mapped_data )
        {
            c8* mapped_offset = ((c8*)mapped_data) + offset;
            pen::memory_cpy(mapped_offset, data, data_size);
        }
        
        CHECK_CALL( glUnmapBuffer( res.type ) );
        
        CHECK_CALL( glBindBuffer( res.type, 0 ) );
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

            CHECK_CALL( glBindTexture( GL_TEXTURE_2D, res.texture.handle ) );
            
            void* data = pen::memory_alloc(rrbp.data_size);
            CHECK_CALL( glGetTexImage( GL_TEXTURE_2D, 0, format, type, data ) );
            
            rrbp.call_back_function( data, rrbp.row_pitch, rrbp.depth_pitch, rrbp.block_size );
            
            pen::memory_free(data);
        }
        else if( t == GL_ELEMENT_ARRAY_BUFFER || t == GL_UNIFORM_BUFFER || t == GL_ARRAY_BUFFER )
        {
            CHECK_CALL( glBindBuffer(t, res.handle ) );
            void* map = glMapBuffer(t, GL_READ_ONLY);
            
            rrbp.call_back_function( map, rrbp.row_pitch, rrbp.depth_pitch, rrbp.block_size );
            
            CHECK_CALL( glUnmapBuffer(t) );
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
            CHECK_CALL( glEnable(GL_DEPTH_TEST) );
        }
        else
        {
            CHECK_CALL( glDisable(GL_DEPTH_TEST) );
        }
        
        CHECK_CALL( glDepthFunc(res.depth_stencil->depth_func) );
        CHECK_CALL( glDepthMask(res.depth_stencil->depth_write_mask) );
	}

	void direct::renderer_release_shader( u32 shader_index, u32 shader_type )
    {
        resource_allocation& res = resource_pool[ shader_index ];
        CHECK_CALL( glDeleteShader( res.handle ) );
        
        res.handle = 0;
	}

	void direct::renderer_release_buffer( u32 buffer_index )
	{
        resource_allocation& res = resource_pool[ buffer_index ];
        CHECK_CALL( glDeleteBuffers(1, &res.handle) );
        
        res.handle = 0;
	}

	void direct::renderer_release_texture( u32 texture_index )
	{
        resource_allocation& res = resource_pool[ texture_index ];
        CHECK_CALL( glDeleteTextures(1, &res.handle) );
        
        res.handle = 0;
	}

	void direct::renderer_release_raster_state( u32 raster_state_index )
	{
        //no gl objects associated with raster state
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
        managed_render_target* erased = nullptr;
        
        u32 num_man_rt = sb_count(k_managed_render_targets);
        for (s32 i = num_man_rt - 1; i >= 0; --i)
        {
            if (k_managed_render_targets[i].render_target_handle == render_target)
                continue;
            
            sb_push(erased, k_managed_render_targets[i]);
        }
        
        sb_free(k_managed_render_targets);
        k_managed_render_targets = erased;
        
        resource_allocation& res = resource_pool[ render_target ];
        
        if( res.render_target.texture.handle > 0)
        {
            CHECK_CALL( glDeleteTextures( 1, &res.render_target.texture.handle ) );
        }
        
        if( res.render_target.texture_msaa.handle > 0)
        {
            CHECK_CALL( glDeleteTextures( 1, &res.render_target.texture_msaa.handle ) );
        }

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
        
        CHECK_CALL( glDeleteProgram(res.shader_program->program) );
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
			case RESOURCE_RENDER_TARGET:
				direct::renderer_release_render_target(dest);
				break;
            default:
                break;
        }
        
        resource_pool[dest] = resource_pool[src];
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

