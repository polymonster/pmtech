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

namespace pen
{
    extern void                renderer_update_queries();
	extern u32                 get_next_query_index(u32 domain);

	//--------------------------------------------------------------------------------------
	//  MULTITHREADED SYNCRONISED RESOURCE HANDLES
	//--------------------------------------------------------------------------------------
	enum renderer_resource_status_flags
	{
		RENDERER_RESOURCES_TO_RECLAIM = 1<<0,
		RENDERER_RESOURCES_ALLOCATED_THIS_FRAME = 1<<1,
		RENDERER_RESOURCES_FULL = 1<<2
	};

	u8	renderer_resource_status[MAX_RENDERER_RESOURCES] = { 0 };
	u8	renderer_resource_flags = 0;

	void renderer_reclaim_resource_indices()
	{
		if ( !(renderer_resource_flags & RENDERER_RESOURCES_TO_RECLAIM) || (renderer_resource_flags & RENDERER_RESOURCES_ALLOCATED_THIS_FRAME) )
		{
			renderer_resource_flags &= ~(RENDERER_RESOURCES_ALLOCATED_THIS_FRAME);
			return;
		}

		for (s32 i = 0; i < MAX_RENDERER_RESOURCES; ++i)
		{
			if ( renderer_resource_status[i] & MARK_DELETE)
			{
				renderer_resource_status[i] = RECLAIMED;
			}
		}

		renderer_resource_flags &= ~(RENDERER_RESOURCES_TO_RECLAIM);
	}

	void renderer_mark_resource_deleted(u32 i)
	{
		renderer_resource_status[i] |= MARK_DELETE;
	}

	u32 renderer_get_next_resource_index(u32 domain)
	{
		u32 i = 0;
		while (renderer_resource_status[i] & domain)
		{
			++i;

			if (i >= MAX_RENDERER_RESOURCES)
			{
				renderer_resource_flags |= RENDERER_RESOURCES_FULL;
				return 0;
			}
		}

		renderer_resource_flags |= RENDERER_RESOURCES_ALLOCATED_THIS_FRAME;
		renderer_resource_status[i] |= domain;

		return i;
	};

    //--------------------------------------------------------------------------------------
    //  COMMAND BUFFER API
    //--------------------------------------------------------------------------------------
#define MAX_COMMANDS 10000
#define INC_WRAP( V ) V = (V+1)%MAX_COMMANDS
    
    enum commands : u32
    {
        CMD_NONE = 0,
        CMD_CLEAR,
        CMD_CLEAR_CUBE,
        CMD_PRESENT,
        CMD_LOAD_SHADER,
        CMD_SET_SHADER,
        CMD_LINK_SHADER,
        CMD_SET_SHADER_PROGRAM,
        CMD_CREATE_INPUT_LAYOUT,
        CMD_SET_INPUT_LAYOUT,
        CMD_CREATE_BUFFER,
        CMD_SET_VERTEX_BUFFER,
        CMD_SET_INDEX_BUFFER,
        CMD_DRAW,
        CMD_DRAW_INDEXED,
        CMD_CREATE_TEXTURE_2D,
        CMD_RELEASE_SHADER,
        CMD_RELEASE_BUFFER,
        CMD_RELEASE_TEXTURE_2D,
        CMD_CREATE_SAMPLER,
        CMD_SET_TEXTURE,
        CMD_CREATE_RASTER_STATE,
        CMD_SET_RASTER_STATE,
        CMD_SET_VIEWPORT,
        CMD_SET_SCISSOR_RECT,
        CMD_RELEASE_RASTER_STATE,
        CMD_CREATE_BLEND_STATE,
        CMD_SET_BLEND_STATE,
        CMD_SET_CONSTANT_BUFFER,
        CMD_UPDATE_BUFFER,
        CMD_CREATE_DEPTH_STENCIL_STATE,
        CMD_SET_DEPTH_STENCIL_STATE,
        CMD_CREATE_QUERY,
        CMD_SET_QUERY,
        CMD_UPDATE_QUERIES,
        CMD_CREATE_RENDER_TARGET,
        CMD_SET_TARGETS,
        CMD_SET_TARGETS_CUBE,
        CMD_RELEASE_BLEND_STATE,
        CMD_RELEASE_RENDER_TARGET,
        CMD_RELEASE_INPUT_LAYOUT,
        CMD_RELEASE_SAMPLER,
        CMD_RELEASE_PROGRAM,
        CMD_RELEASE_CLEAR_STATE,
        CMD_RELEASE_DEPTH_STENCIL_STATE,
        CMD_RELEASE_QUERY,
        CMD_CREATE_SO_SHADER,
        CMD_SET_SO_TARGET,
        CMD_DRAW_AUTO,
    };
    
    struct set_shader_cmd
    {
        u32 shader_index;
        u32 shader_type;
        
    };
    
    struct set_target_cmd
    {
        u32 colour;
        u32 depth;
    };
    
    struct set_target_cube_cmd
    {
        u32 colour;
        u32 colour_face;
        u32 depth;
        u32 depth_face;
    };
    
    struct clear_cube_cmd
    {
        u32 clear_state;
        u32 colour_face;
        u32 depth_face;
    };
    
    struct set_vertex_buffer_cmd
    {
        u32 buffer_index;
        u32 start_slot;
        u32 num_buffers;
        u32* strides;
        u32* offsets;
        
    };
    
    struct set_index_buffer_cmd
    {
        u32 buffer_index;
        u32 format;
        u32 offset;
        
    };
    
    struct draw_cmd
    {
        u32 vertex_count;
        u32 start_vertex;
        u32 primitive_topology;
        
    };
    
    struct draw_indexed_cmd
    {
        u32 index_count;
        u32 start_index;
        u32 base_vertex;
        u32 primitive_topology;
        
    };
    
    struct set_texture_cmd
    {
        u32 texture_index;
        u32 sampler_index;
        u32 resource_slot;
        u32 shader_type;
        
    };
    
    struct set_constant_buffer_cmd
    {
        u32 buffer_index;
        u32 resource_slot;
        u32 shader_type;
        
    };
    
    struct update_buffer_cmd
    {
        u32		buffer_index;
        void*   data;
        u32     data_size;
        u32     offset;
        
    } ;
    
    struct query_params
    {
        u32 action;
        u32 query_index;
    };
    
    struct query_create_params
    {
        u32 query_type;
        u32 query_flags;
    };
    
    typedef struct  deferred_cmd
    {
        u32		command_index;
        
        union
        {
            u32                                 command_data_index;
            shader_load_params                  shader_load;
            set_shader_cmd                      set_shader;
            input_layout_creation_params        create_input_layout;
            buffer_creation_params              create_buffer;
            set_vertex_buffer_cmd               set_vertex_buffer;
            set_index_buffer_cmd                set_index_buffer;
            draw_cmd                            draw;
            draw_indexed_cmd                    draw_indexed;
            texture_creation_params             create_texture2d;
            sampler_creation_params             create_sampler;
            set_texture_cmd                     set_texture;
            rasteriser_state_creation_params    create_raster_state;
            viewport                            set_viewport;
            rect                                set_rect;
            blend_creation_params               create_blend_state;
            set_constant_buffer_cmd             set_constant_buffer;
            update_buffer_cmd                   update_buffer;
            depth_stencil_creation_params*      p_create_depth_stencil_state;
            query_params                        set_query;
            query_create_params                 create_query;
            texture_creation_params             create_render_target;
            set_target_cmd                      set_targets;
            set_target_cube_cmd                 set_targets_cube;
            clear_cube_cmd                      clear_cube;
            shader_link_params                  link_params;
        };
        
        deferred_cmd() {};
        
    } deferred_cmd;
    
    deferred_cmd cmd_buffer[ MAX_COMMANDS ];
    u32 put_pos = 0;
    u32 get_pos = 0;
    
    void exec_cmd( const deferred_cmd &cmd )
    {        
        switch( cmd.command_index )
        {
            case CMD_CLEAR:
                direct::renderer_clear( cmd.command_data_index );
                break;
                
            case CMD_PRESENT:
                direct::renderer_present();
                break;
                
            case CMD_LOAD_SHADER:
                direct::renderer_load_shader( cmd.shader_load );
                pen::memory_free( cmd.shader_load.byte_code );
                break;
                
            case CMD_SET_SHADER:
                direct::renderer_set_shader( cmd.set_shader.shader_index, cmd.set_shader.shader_type );
                break;
                
            case CMD_LINK_SHADER:
                direct::renderer_link_shader_program(cmd.link_params);
                for( u32 i = 0; i < cmd.link_params.num_constants; ++i )
                    pen::memory_free( cmd.link_params.constants[i].name );
                pen::memory_free( cmd.link_params.constants );
                break;
                
            case CMD_CREATE_INPUT_LAYOUT:
                direct::renderer_create_input_layout( cmd.create_input_layout );
                pen::memory_free( cmd.create_input_layout.vs_byte_code );
                for( u32 i = 0; i < cmd.create_input_layout.num_elements; ++i )
                    pen::memory_free( cmd.create_input_layout.input_layout[ i ].semantic_name );
                pen::memory_free( cmd.create_input_layout.input_layout );
                break;
                
            case CMD_SET_INPUT_LAYOUT:
                direct::renderer_set_input_layout( cmd.command_data_index );
                break;
                
            case CMD_CREATE_BUFFER:
                direct::renderer_create_buffer( cmd.create_buffer );
                pen::memory_free( cmd.create_buffer.data );
                break;
                
            case CMD_SET_VERTEX_BUFFER:
                direct::renderer_set_vertex_buffer(
                                                   cmd.set_vertex_buffer.buffer_index,
                                                   cmd.set_vertex_buffer.start_slot,
                                                   cmd.set_vertex_buffer.num_buffers,
                                                   cmd.set_vertex_buffer.strides,
                                                   cmd.set_vertex_buffer.offsets );
                pen::memory_free( cmd.set_vertex_buffer.strides );
                pen::memory_free( cmd.set_vertex_buffer.offsets );
                break;
                
            case CMD_SET_INDEX_BUFFER:
                direct::renderer_set_index_buffer(
                                                  cmd.set_index_buffer.buffer_index,
                                                  cmd.set_index_buffer.format,
                                                  cmd.set_index_buffer.offset );
                break;
                
            case CMD_DRAW:
                direct::renderer_draw( cmd.draw.vertex_count, cmd.draw.start_vertex, cmd.draw.primitive_topology );
                break;
                
            case CMD_DRAW_INDEXED:
                direct::renderer_draw_indexed(
                                              cmd.draw_indexed.index_count,
                                              cmd.draw_indexed.start_index,
                                              cmd.draw_indexed.base_vertex,
                                              cmd.draw_indexed.primitive_topology );
                break;
                
            case CMD_CREATE_TEXTURE_2D:
                direct::renderer_create_texture2d( cmd.create_texture2d );
                pen::memory_free( cmd.create_texture2d.data );
                break;
                
            case CMD_CREATE_SAMPLER:
                direct::renderer_create_sampler( cmd.create_sampler );
                break;
                
            case CMD_SET_TEXTURE:
                direct::renderer_set_texture(
                                             cmd.set_texture.texture_index,
                                             cmd.set_texture.sampler_index,
                                             cmd.set_texture.resource_slot,
                                             cmd.set_texture.shader_type );
                break;
                
            case CMD_CREATE_RASTER_STATE:
                direct::renderer_create_rasterizer_state( cmd.create_raster_state );
                break;
                
            case CMD_SET_RASTER_STATE:
                direct::renderer_set_rasterizer_state( cmd.command_data_index );
                break;
                
            case CMD_SET_VIEWPORT:
                direct::renderer_set_viewport( cmd.set_viewport );
                break;

            case CMD_SET_SCISSOR_RECT:
                direct::renderer_set_scissor_rect( cmd.set_rect );
                break;
                
            case CMD_RELEASE_SHADER:
                direct::renderer_release_shader( cmd.set_shader.shader_index, cmd.set_shader.shader_type );
                break;
                
            case CMD_RELEASE_BUFFER:
                direct::renderer_release_buffer( cmd.command_data_index );
                break;
                
            case CMD_RELEASE_TEXTURE_2D:
                direct::renderer_release_texture2d( cmd.command_data_index );
                break;
                
            case CMD_RELEASE_RASTER_STATE:
                direct::renderer_release_raster_state( cmd.command_data_index );
                break;
                
            case CMD_CREATE_BLEND_STATE:
                direct::renderer_create_blend_state( cmd.create_blend_state );
                pen::memory_free( cmd_buffer[ put_pos ].create_blend_state.render_targets );
                break;
                
            case CMD_SET_BLEND_STATE:
                direct::renderer_set_blend_state( cmd.command_data_index );
                break;
                
            case CMD_SET_CONSTANT_BUFFER:
                direct::renderer_set_constant_buffer( cmd.set_constant_buffer.buffer_index, cmd.set_constant_buffer.resource_slot, cmd.set_constant_buffer.shader_type );
                break;
                
            case CMD_UPDATE_BUFFER:
                direct::renderer_update_buffer( cmd.update_buffer.buffer_index, cmd.update_buffer.data, cmd.update_buffer.data_size, cmd.update_buffer.offset );
                pen::memory_free( cmd.update_buffer.data );
                break;
                
            case CMD_CREATE_DEPTH_STENCIL_STATE:
                direct::renderer_create_depth_stencil_state( *cmd.p_create_depth_stencil_state );
                pen::memory_free( cmd.p_create_depth_stencil_state );
                break;
                
            case CMD_SET_DEPTH_STENCIL_STATE:
                direct::renderer_set_depth_stencil_state( cmd.command_data_index );
                break;
                
            case CMD_CREATE_QUERY:
                direct::renderer_create_query( cmd.create_query.query_type, cmd.create_query.query_flags );
                break;
                
            case CMD_SET_QUERY:
                direct::renderer_set_query( cmd.set_query.query_index, cmd.set_query.action );
                break;
                
            case CMD_UPDATE_QUERIES:
                renderer_update_queries();
                break;
                
            case CMD_CREATE_RENDER_TARGET:
                direct::renderer_create_render_target( cmd.create_render_target );
                break;
                
            case CMD_SET_TARGETS:
                direct::renderer_set_targets( cmd.set_targets.colour, cmd.set_targets.depth );
                break;
                
            case CMD_SET_TARGETS_CUBE:
                direct::renderer_set_targets( cmd.set_targets_cube.colour, cmd.set_targets_cube.depth, cmd.set_targets_cube.colour_face, cmd.set_targets_cube.depth_face );
                break;
                
            case CMD_CLEAR_CUBE:
                direct::renderer_clear( cmd.clear_cube.clear_state, cmd.clear_cube.colour_face, cmd.clear_cube.depth_face );
                break;
                
            case CMD_RELEASE_BLEND_STATE:
                direct::renderer_release_blend_state( cmd.command_data_index );
                break;
                
            case CMD_RELEASE_PROGRAM:
                direct::renderer_release_program( cmd.command_data_index );
                break;
                
            case CMD_RELEASE_CLEAR_STATE:
                direct::renderer_release_clear_state( cmd.command_data_index );
                break;
                
            case CMD_RELEASE_RENDER_TARGET:
                direct::renderer_release_render_target( cmd.command_data_index );
                break;
                
            case CMD_RELEASE_INPUT_LAYOUT:
                direct::renderer_release_input_layout( cmd.command_data_index );
                break;
                
            case CMD_RELEASE_SAMPLER:
                direct::renderer_release_sampler( cmd.command_data_index );
                break;
                
            case CMD_RELEASE_DEPTH_STENCIL_STATE:
                direct::renderer_release_depth_stencil_state( cmd.command_data_index );
                break;
                
            case CMD_RELEASE_QUERY:
                direct::renderer_release_query( cmd.command_data_index );
                break;
                
            case CMD_CREATE_SO_SHADER:
                direct::renderer_create_so_shader( cmd.shader_load );
                break;
                
            case CMD_SET_SO_TARGET:
                direct::renderer_set_so_target( cmd.command_data_index );
                break;
                
            case CMD_DRAW_AUTO:
                direct::renderer_draw_auto();
                break;
        }
    }
    
    //--------------------------------------------------------------------------------------
    //  THREAD SYNCRONISATION
    //--------------------------------------------------------------------------------------

    void				renderer_wait_for_jobs();
    u32					renderer_init_from_window( void* window );

    pen::job_thread*    p_job_thread_info;
    pen::semaphore*     p_consume_semaphore;
    pen::semaphore*		p_continue_semaphore;
    
    bool                     consume_flag = false;
    
    void renderer_wait_init()
    {
        pen::threads_semaphore_wait( p_continue_semaphore );
    }
    
    void defer::renderer_consume_cmd_buffer()
    {
        if( p_consume_semaphore )
        {
            pen::threads_semaphore_signal( p_consume_semaphore, 1 );
            pen::threads_semaphore_wait( p_continue_semaphore );
        }
    }
    
    void renderer_wait_for_jobs()
    {
        //this is a dedicated thread which stays for the duration of the program
        pen::threads_semaphore_signal( p_continue_semaphore, 1 );
        
        while( 1 )
        {
            PEN_TIMER_START( CMD_BUFFER );
            
            if( pen::threads_semaphore_try_wait( p_consume_semaphore ) )
            {
                //put_pos might change on the producer thread.
                u32 end_pos = put_pos;
                
                //reclaim deleted resource handles while we can syncronise it.
                pen::renderer_reclaim_resource_indices();
                
                pen::threads_semaphore_signal( p_continue_semaphore, 1 );
                
                //some api's need to set the current context on the caller thread.
                direct::renderer_make_context_current();
                
                while( get_pos != end_pos )
                {
                    exec_cmd( cmd_buffer[ get_pos ] );
                    
                    INC_WRAP( get_pos );
                }
            }
            else
            {
                pen::threads_sleep_ms(1);
            }
            
            PEN_TIMER_END( CMD_BUFFER );
            PEN_TIMER_RESET( CMD_BUFFER );
            
            if( pen::threads_semaphore_try_wait( p_job_thread_info->p_sem_exit ) )
            {
                //exit
                break;
            }
        }
        
        pen::threads_semaphore_signal( p_continue_semaphore, 1 );
        pen::threads_semaphore_signal( p_job_thread_info->p_sem_terminated, 1 );
    }
    
    PEN_THREAD_RETURN renderer_thread_function( void* params )
    {
        job_thread_params* job_params = (job_thread_params*)params;
        
        p_job_thread_info = job_params->job_thread_info;
        
        p_consume_semaphore = p_job_thread_info->p_sem_consume;
        p_continue_semaphore = p_job_thread_info->p_sem_continue;
        
		//clear command buffer
		pen::memory_set(cmd_buffer, 0x0, sizeof(deferred_cmd) * MAX_COMMANDS);

		//reserve resource index 0 to be used as a null handle
		renderer_resource_status[0] |= (DIRECT_RESOURCE | DEFER_RESOURCE);

		//initialise renderer
        direct::renderer_initialise(job_params->user_data);
       
        renderer_wait_for_jobs();
        
        return PEN_THREAD_OK;
    }
    
    
    //--------------------------------------------------------------------------------------
    //  DEFERRED API
    //--------------------------------------------------------------------------------------
    void defer::renderer_update_queries()
    {
        cmd_buffer[ put_pos ].command_index = CMD_UPDATE_QUERIES;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_clear( u32 clear_state_index )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CLEAR;
        cmd_buffer[ put_pos ].command_data_index = clear_state_index;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_clear_cube( u32 clear_state_index, u32 colour_face, u32 depth_face )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CLEAR_CUBE;
        cmd_buffer[ put_pos ].clear_cube.clear_state = clear_state_index;
        cmd_buffer[ put_pos ].clear_cube.colour_face = colour_face;
        cmd_buffer[ put_pos ].clear_cube.depth_face = depth_face;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_present()
    {
        cmd_buffer[ put_pos ].command_index = CMD_PRESENT;
        
        INC_WRAP( put_pos );
    }
    
    u32 defer::renderer_create_query( u32 query_type, u32 flags )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CREATE_QUERY;
        
        cmd_buffer[ put_pos ].create_query.query_flags = flags;
        cmd_buffer[ put_pos ].create_query.query_type = query_type;
        
        INC_WRAP( put_pos );
        
        return get_next_query_index( DEFER_RESOURCE );
    }
    
    void defer::renderer_set_query( u32 query_index, u32 action )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_QUERY;
        
        cmd_buffer[ put_pos ].set_query.query_index = query_index;
        cmd_buffer[ put_pos ].set_query.action = action;
        
        INC_WRAP( put_pos );
    }
    
    u32 defer::renderer_load_shader( const pen::shader_load_params &params )
    {
        cmd_buffer[ put_pos ].command_index = CMD_LOAD_SHADER;
        
        cmd_buffer[ put_pos ].shader_load.byte_code_size = params.byte_code_size;
        cmd_buffer[ put_pos ].shader_load.type = params.type;
        
        if( params.byte_code )
        {
            cmd_buffer[ put_pos ].shader_load.byte_code = pen::memory_alloc( params.byte_code_size );
            pen::memory_cpy( cmd_buffer[ put_pos ].shader_load.byte_code, params.byte_code, params.byte_code_size );
        }
        
        u32 shader_index = renderer_get_next_resource_index( DEFER_RESOURCE );
        
        INC_WRAP( put_pos );
        
        return shader_index;
    }
    
    u32 defer::renderer_link_shader_program( const shader_link_params &params )
    {
        cmd_buffer[ put_pos ].command_index = CMD_LINK_SHADER;
        
        cmd_buffer[ put_pos ].link_params.input_layout = params.input_layout;
        cmd_buffer[ put_pos ].link_params.vertex_shader = params.vertex_shader;
        cmd_buffer[ put_pos ].link_params.pixel_shader = params.pixel_shader;
        
        u32 num = params.num_constants;
        cmd_buffer[ put_pos ].link_params.num_constants = num;
        
        cmd_buffer[ put_pos ].link_params.constants = (pen::constant_layout_desc*)pen::memory_alloc(sizeof(pen::constant_layout_desc) * num);
        
        pen::constant_layout_desc* c = cmd_buffer[ put_pos ].link_params.constants;
        for( u32 i = 0; i < num; ++i )
        {
            c[i].location = params.constants[i].location;
            c[i].type = params.constants[i].type;
            
            u32 len = pen::string_length(params.constants[i].name);
            c[i].name = (c8*)pen::memory_alloc(len+1);
            
            pen::memory_cpy( c[i].name, params.constants[i].name, len);
            c[i].name[len] = '\0';
        }
        
        u32 program_index = renderer_get_next_resource_index( DEFER_RESOURCE );
        
        INC_WRAP( put_pos );
        
        return program_index;
    }
    
    void defer::renderer_set_shader_program( u32 program_index )
    {
        
    }
    
    u32 defer::renderer_create_so_shader( const pen::shader_load_params &params )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CREATE_SO_SHADER;
        
        cmd_buffer[ put_pos ].shader_load.byte_code_size = params.byte_code_size;
        cmd_buffer[ put_pos ].shader_load.type = params.type;
        
        if( params.byte_code )
        {
            cmd_buffer[ put_pos ].shader_load.byte_code = pen::memory_alloc( params.byte_code_size );
            pen::memory_cpy( cmd_buffer[ put_pos ].shader_load.byte_code, params.byte_code, params.byte_code_size );
        }
        
        if( params.so_decl_entries )
        {
            cmd_buffer[ put_pos ].shader_load.so_num_entries = params.so_num_entries;
            
            u32 entries_size = sizeof( stream_out_decl_entry ) * params.so_num_entries;
            cmd_buffer[ put_pos ].shader_load.so_decl_entries = ( stream_out_decl_entry* ) pen::memory_alloc( entries_size );
            
            pen::memory_cpy( cmd_buffer[ put_pos ].shader_load.so_decl_entries, params.so_decl_entries, entries_size );
        }
        
        u32 shader_index = renderer_get_next_resource_index( DEFER_RESOURCE );
        
        INC_WRAP( put_pos );
        
        return shader_index;
    }
    
    void defer::renderer_set_shader( u32 shader_index, u32 shader_type )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_SHADER;
        
        cmd_buffer[ put_pos ].set_shader.shader_index = shader_index;
        cmd_buffer[ put_pos ].set_shader.shader_type = shader_type;
        
        INC_WRAP( put_pos );
    }
    
    u32 defer::renderer_create_input_layout( const input_layout_creation_params &params )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CREATE_INPUT_LAYOUT;
        
        //simple data
        cmd_buffer[ put_pos ].create_input_layout.num_elements = params.num_elements;
        cmd_buffer[ put_pos ].create_input_layout.vs_byte_code_size = params.vs_byte_code_size;
        
        //copy buffer
        cmd_buffer[ put_pos ].create_input_layout.vs_byte_code = pen::memory_alloc( params.vs_byte_code_size );
        pen::memory_cpy( cmd_buffer[ put_pos ].create_input_layout.vs_byte_code, params.vs_byte_code, params.vs_byte_code_size );
        
        //copy array
        u32 input_layouts_size = sizeof( pen::input_layout_desc ) * params.num_elements;
        cmd_buffer[ put_pos ].create_input_layout.input_layout = ( pen::input_layout_desc* )pen::memory_alloc( input_layouts_size );
        
        pen::memory_cpy( cmd_buffer[ put_pos ].create_input_layout.input_layout, params.input_layout, input_layouts_size );
        
        for( u32 i = 0; i < params.num_elements; ++i )
        {
            //we need to also allocate and copy a string
            u32 semantic_len = pen::string_length( params.input_layout[ i ].semantic_name );
            cmd_buffer[ put_pos ].create_input_layout.input_layout[ i ].semantic_name = ( c8* ) pen::memory_alloc( semantic_len + 1 );
            pen::memory_cpy( cmd_buffer[ put_pos ].create_input_layout.input_layout[ i ].semantic_name, params.input_layout[ i ].semantic_name, semantic_len );
            
            //terminate string
            cmd_buffer[ put_pos ].create_input_layout.input_layout[ i ].semantic_name[ semantic_len ] = '\0';
        }
        
        INC_WRAP( put_pos );
        
        return renderer_get_next_resource_index( DEFER_RESOURCE );
    }
    
    void defer::renderer_set_input_layout( u32 layout_index )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_INPUT_LAYOUT;
        
        cmd_buffer[ put_pos ].command_data_index = layout_index;
        
        INC_WRAP( put_pos );
    }
    
    u32 defer::renderer_create_buffer( const buffer_creation_params &params )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CREATE_BUFFER;
        
        pen::memory_cpy( &cmd_buffer[ put_pos ].create_buffer, ( void* ) &params, sizeof( buffer_creation_params ) );
        
        if( params.data )
        {
            //make a copy of the buffers data
            cmd_buffer[ put_pos ].create_buffer.data = pen::memory_alloc( params.buffer_size );
            pen::memory_cpy( cmd_buffer[ put_pos ].create_buffer.data, params.data, params.buffer_size );
        }
        
        INC_WRAP( put_pos );
        
        return renderer_get_next_resource_index( DEFER_RESOURCE );
    }
    
    void defer::renderer_set_vertex_buffer( u32 buffer_index, u32 start_slot, u32 num_buffers, const u32* strides, const u32* offsets )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_VERTEX_BUFFER;
        
        cmd_buffer[ put_pos ].set_vertex_buffer.buffer_index = buffer_index;
        cmd_buffer[ put_pos ].set_vertex_buffer.start_slot = start_slot;
        cmd_buffer[ put_pos ].set_vertex_buffer.num_buffers = num_buffers;
        
        cmd_buffer[ put_pos ].set_vertex_buffer.strides = ( u32* ) pen::memory_alloc( sizeof( u32 ) * num_buffers );
        cmd_buffer[ put_pos ].set_vertex_buffer.offsets = ( u32* ) pen::memory_alloc( sizeof( u32 ) * num_buffers );
        
        for( u32 i = 0; i < num_buffers; ++i )
        {
            cmd_buffer[ put_pos ].set_vertex_buffer.strides[ i ] = strides[ i ];
            cmd_buffer[ put_pos ].set_vertex_buffer.offsets[ i ] = offsets[ i ];
        }
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_set_index_buffer( u32 buffer_index, u32 format, u32 offset )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_INDEX_BUFFER;
        cmd_buffer[ put_pos ].set_index_buffer.buffer_index = buffer_index;
        cmd_buffer[ put_pos ].set_index_buffer.format = format;
        cmd_buffer[ put_pos ].set_index_buffer.offset = offset;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_draw( u32 vertex_count, u32 start_vertex, u32 primitive_topology )
    {
        cmd_buffer[ put_pos ].command_index = CMD_DRAW;
        cmd_buffer[ put_pos ].draw.vertex_count = vertex_count;
        cmd_buffer[ put_pos ].draw.start_vertex = start_vertex;
        cmd_buffer[ put_pos ].draw.primitive_topology = primitive_topology;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_draw_indexed( u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology )
    {
        cmd_buffer[ put_pos ].command_index = CMD_DRAW_INDEXED;
        cmd_buffer[ put_pos ].draw_indexed.index_count = index_count;
        cmd_buffer[ put_pos ].draw_indexed.start_index = start_index;
        cmd_buffer[ put_pos ].draw_indexed.base_vertex = base_vertex;
        cmd_buffer[ put_pos ].draw_indexed.primitive_topology = primitive_topology;
        
        INC_WRAP( put_pos );
    }
    
    u32 defer::renderer_create_render_target( const texture_creation_params& tcp )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CREATE_RENDER_TARGET;
        
        pen::memory_cpy( &cmd_buffer[ put_pos ].create_render_target, ( void* ) &tcp, sizeof( texture_creation_params ) );
        
        INC_WRAP( put_pos );
        
        return renderer_get_next_resource_index( DEFER_RESOURCE );
    }
    
    u32 defer::renderer_create_texture2d( const texture_creation_params& tcp )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CREATE_TEXTURE_2D;
        
        pen::memory_cpy( &cmd_buffer[ put_pos ].create_texture2d, ( void* ) &tcp, sizeof( texture_creation_params ) );
        
        cmd_buffer[ put_pos ].create_texture2d.data = pen::memory_alloc( tcp.data_size );
        
        if( tcp.data )
        {
            pen::memory_cpy( cmd_buffer[ put_pos ].create_texture2d.data, tcp.data, tcp.data_size );
        }
        else
        {
            cmd_buffer[ put_pos ].create_texture2d.data = nullptr;
        }

        INC_WRAP( put_pos );
        
        return renderer_get_next_resource_index( DEFER_RESOURCE );
    }
    
    void defer::renderer_release_shader( u32 shader_index, u32 shader_type )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_SHADER;
        
        cmd_buffer[ put_pos ].set_shader.shader_index = shader_index;
        cmd_buffer[ put_pos ].set_shader.shader_type = shader_type;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_buffer( u32 buffer_index )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_BUFFER;
        
        cmd_buffer[ put_pos ].command_data_index = buffer_index;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_texture2d( u32 texture_index )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_TEXTURE_2D;
        
        cmd_buffer[ put_pos ].command_data_index = texture_index;
        
        INC_WRAP( put_pos );
    }
    
    u32 defer::renderer_create_sampler( const sampler_creation_params& scp )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CREATE_SAMPLER;
        
        pen::memory_cpy( &cmd_buffer[ put_pos ].create_sampler, ( void* ) &scp, sizeof( sampler_creation_params ) );
        
        INC_WRAP( put_pos );
        
        return renderer_get_next_resource_index( DEFER_RESOURCE );
    }
    
    void defer::renderer_set_texture( u32 texture_index, u32 sampler_index, u32 resource_slot, u32 shader_type )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_TEXTURE;
        
        cmd_buffer[ put_pos ].set_texture.texture_index = texture_index;
        cmd_buffer[ put_pos ].set_texture.sampler_index = sampler_index;
        cmd_buffer[ put_pos ].set_texture.resource_slot = resource_slot;
        cmd_buffer[ put_pos ].set_texture.shader_type = shader_type;
        
        INC_WRAP( put_pos );
    }
    
    u32 defer::renderer_create_rasterizer_state( const rasteriser_state_creation_params &rscp )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CREATE_RASTER_STATE;
        
        pen::memory_cpy( &cmd_buffer[ put_pos ].create_raster_state, ( void* ) &rscp, sizeof( rasteriser_state_creation_params ) );
        
        INC_WRAP( put_pos );
        
        return renderer_get_next_resource_index( DEFER_RESOURCE );
    }
    
    void defer::renderer_set_rasterizer_state( u32 rasterizer_state_index )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_RASTER_STATE;
        
        cmd_buffer[ put_pos ].command_data_index = rasterizer_state_index;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_set_viewport( const viewport &vp )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_VIEWPORT;
        
        pen::memory_cpy( &cmd_buffer[ put_pos ].set_viewport, ( void* ) &vp, sizeof( viewport ) );
        
        INC_WRAP( put_pos );
    }

    void defer::renderer_set_scissor_rect( const rect &r )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_SCISSOR_RECT;

        pen::memory_cpy( &cmd_buffer[ put_pos ].set_rect, ( void* ) &r, sizeof( rect ) );

        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_raster_state( u32 raster_state_index )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_RASTER_STATE;
        
        cmd_buffer[ put_pos ].command_data_index = raster_state_index;
        
        INC_WRAP( put_pos );
    }
    
    u32 defer::renderer_create_blend_state( const blend_creation_params &bcp )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CREATE_BLEND_STATE;
        
        pen::memory_cpy( &cmd_buffer[ put_pos ].create_blend_state, ( void* ) &bcp, sizeof( blend_creation_params ) );
        
        //alloc and copy the render targets blend modes. to save space in the cmd buffer
        u32 render_target_modes_size = sizeof( render_target_blend ) * bcp.num_render_targets;
        cmd_buffer[ put_pos ].create_blend_state.render_targets = ( render_target_blend* ) pen::memory_alloc( render_target_modes_size );
        
        pen::memory_cpy( cmd_buffer[ put_pos ].create_blend_state.render_targets, ( void* ) bcp.render_targets, render_target_modes_size );
        
        INC_WRAP( put_pos );
        
        return renderer_get_next_resource_index( DEFER_RESOURCE );
    }
    
    void defer::renderer_set_blend_state( u32 blend_state_index )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_BLEND_STATE;
        
        cmd_buffer[ put_pos ].command_data_index = blend_state_index;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_set_constant_buffer( u32 buffer_index, u32 resource_slot, u32 shader_type )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_CONSTANT_BUFFER;
        
        cmd_buffer[ put_pos ].set_constant_buffer.buffer_index = buffer_index;
        cmd_buffer[ put_pos ].set_constant_buffer.resource_slot = resource_slot;
        cmd_buffer[ put_pos ].set_constant_buffer.shader_type = shader_type;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_update_buffer( u32 buffer_index, const void* data, u32 data_size, u32 offset )
    {
        cmd_buffer[ put_pos ].command_index = CMD_UPDATE_BUFFER;
        
        cmd_buffer[ put_pos ].update_buffer.buffer_index = buffer_index;
        cmd_buffer[ put_pos ].update_buffer.data_size = data_size;
        cmd_buffer[ put_pos ].update_buffer.offset = offset;
        cmd_buffer[ put_pos ].update_buffer.data = pen::memory_alloc( data_size );
        pen::memory_cpy( cmd_buffer[ put_pos ].update_buffer.data, data, data_size );
        
        INC_WRAP( put_pos );
    }
    
    u32 defer::renderer_create_depth_stencil_state( const depth_stencil_creation_params& dscp )
    {
        cmd_buffer[ put_pos ].command_index = CMD_CREATE_DEPTH_STENCIL_STATE;
        
        cmd_buffer[ put_pos ].p_create_depth_stencil_state = ( depth_stencil_creation_params* ) pen::memory_alloc( sizeof( depth_stencil_creation_params ) );
        pen::memory_cpy( cmd_buffer[ put_pos ].p_create_depth_stencil_state, &dscp, sizeof( depth_stencil_creation_params ) );
        
        INC_WRAP( put_pos );
        
        return renderer_get_next_resource_index( DEFER_RESOURCE );
    }
    
    void defer::renderer_set_depth_stencil_state( u32 depth_stencil_state )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_DEPTH_STENCIL_STATE;
        
        cmd_buffer[ put_pos ].command_data_index = depth_stencil_state;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_set_targets( u32 colour_target, u32 depth_target )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_TARGETS;
        cmd_buffer[ put_pos ].set_targets.colour = colour_target;
        cmd_buffer[ put_pos ].set_targets.depth = depth_target;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_set_targets_cube( u32 colour_target, u32 colour_face, u32 depth_target, u32 depth_face )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_TARGETS_CUBE;
        cmd_buffer[ put_pos ].set_targets_cube.colour = colour_target;
        cmd_buffer[ put_pos ].set_targets_cube.colour_face = colour_face;
        cmd_buffer[ put_pos ].set_targets_cube.depth = depth_target;
        cmd_buffer[ put_pos ].set_targets_cube.depth_face = depth_face;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_blend_state( u32 blend_state )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_BLEND_STATE;
        
        cmd_buffer[ put_pos ].command_data_index = blend_state;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_render_target( u32 render_target )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_RENDER_TARGET;
        
        cmd_buffer[ put_pos ].command_data_index = render_target;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_clear_state( u32 clear_state )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_CLEAR_STATE;
        
        cmd_buffer[ put_pos ].command_data_index = clear_state;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_program( u32 program )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_PROGRAM;
        
        cmd_buffer[ put_pos ].command_data_index = program;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_input_layout( u32 input_layout )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_INPUT_LAYOUT;
        
        cmd_buffer[ put_pos ].command_data_index = input_layout;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_sampler( u32 sampler )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_SAMPLER;
        
        cmd_buffer[ put_pos ].command_data_index = sampler;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_depth_stencil_state( u32 depth_stencil_state )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_DEPTH_STENCIL_STATE;
        
        cmd_buffer[ put_pos ].command_data_index = depth_stencil_state;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_release_query( u32 query )
    {
        cmd_buffer[ put_pos ].command_index = CMD_RELEASE_QUERY;
        
        cmd_buffer[ put_pos ].command_data_index = query;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_set_so_target( u32 buffer_index )
    {
        cmd_buffer[ put_pos ].command_index = CMD_SET_SO_TARGET;
        
        cmd_buffer[ put_pos ].command_data_index = buffer_index;
        
        INC_WRAP( put_pos );
    }
    
    void defer::renderer_draw_auto()
    {
        cmd_buffer[ put_pos ].command_index = CMD_DRAW_AUTO;
        
        INC_WRAP( put_pos );
    }
}

