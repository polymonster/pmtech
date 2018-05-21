#include "console.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_string.h"
#include "renderer.h"
#include "slot_resource.h"
#include "threads.h"
#include "timer.h"

//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------

namespace pen
{
    //--------------------------------------------------------------------------------------
    //  COMMAND BUFFER API
    //--------------------------------------------------------------------------------------
    u32 put_pos             = 0;
    u32 get_pos             = 0;
    u32 commands_this_frame = 0;

#define MAX_COMMANDS (1 << 18)
#define INC_WRAP(V)                                                                                                          \
    V = (V + 1) & (MAX_COMMANDS - 1);                                                                                        \
    commands_this_frame++

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
        CMD_DRAW_INDEXED_INSTANCED,
        CMD_CREATE_TEXTURE,
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
        CMD_CREATE_SO_SHADER,
        CMD_SET_SO_TARGET,
        CMD_RESOLVE_TARGET,
        CMD_DRAW_AUTO,
        CMD_MAP_RESOURCE,
        CMD_REPLACE_RESOURCE,
        CMD_CREATE_CLEAR_STATE,
        CMD_PUSH_PERF_MARKER,
        CMD_POP_PERF_MARKER
    };

    struct set_shader_cmd
    {
        u32 shader_index;
        u32 shader_type;
    };

    static const u32 k_max_colour_targets = 8;
    struct set_target_cmd
    {
        u32 num_colour;
        u32 colour[k_max_colour_targets];
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
        u32  buffer_index;
        u32  start_slot;
        u32* buffer_indices;
        u32  num_buffers;
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

    struct draw_indexed_instanced_cmd
    {
        u32 instance_count;
        u32 start_instance;
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
        u32 flags;
    };

    struct set_constant_buffer_cmd
    {
        u32 buffer_index;
        u32 resource_slot;
        u32 shader_type;
    };

    struct update_buffer_cmd
    {
        u32   buffer_index;
        void* data;
        u32   data_size;
        u32   offset;
    };

    struct msaa_resolve_params
    {
        u32                 render_target;
        e_msaa_resolve_type resolve_type;
    };

    struct replace_resource
    {
        u32                 dest_handle;
        u32                 src_handle;
        e_renderer_resource type;
    };

    typedef struct deferred_cmd
    {
        u32 command_index;
        u32 resource_slot;

        union {
            u32                              command_data_index;
            shader_load_params               shader_load;
            set_shader_cmd                   set_shader;
            input_layout_creation_params     create_input_layout;
            buffer_creation_params           create_buffer;
            set_vertex_buffer_cmd            set_vertex_buffer;
            set_index_buffer_cmd             set_index_buffer;
            draw_cmd                         draw;
            draw_indexed_cmd                 draw_indexed;
            draw_indexed_instanced_cmd       draw_indexed_instanced;
            texture_creation_params          create_texture;
            sampler_creation_params          create_sampler;
            set_texture_cmd                  set_texture;
            rasteriser_state_creation_params create_raster_state;
            viewport                         set_viewport;
            rect                             set_rect;
            blend_creation_params            create_blend_state;
            set_constant_buffer_cmd          set_constant_buffer;
            update_buffer_cmd                update_buffer;
            depth_stencil_creation_params*   p_create_depth_stencil_state;
            texture_creation_params          create_render_target;
            set_target_cmd                   set_targets;
            set_target_cube_cmd              set_targets_cube;
            clear_cube_cmd                   clear_cube;
            shader_link_params               link_params;
            resource_read_back_params        rrb_params;
            msaa_resolve_params              resolve_params;
            replace_resource                 replace_resource_params;
            clear_state                      clear_state_params;
            c8*                              name;
        };

        deferred_cmd(){};

    } deferred_cmd;

    deferred_cmd cmd_buffer[MAX_COMMANDS];

    void exec_cmd(const deferred_cmd& cmd)
    {
        switch (cmd.command_index)
        {
        case CMD_CLEAR:
            direct::renderer_clear(cmd.command_data_index);
            break;

        case CMD_PRESENT:
            direct::renderer_present();
            break;

        case CMD_LOAD_SHADER:
            direct::renderer_load_shader(cmd.shader_load, cmd.resource_slot);
            memory_free(cmd.shader_load.byte_code);
            memory_free(cmd.shader_load.so_decl_entries);
            break;

        case CMD_SET_SHADER:
            direct::renderer_set_shader(cmd.set_shader.shader_index, cmd.set_shader.shader_type);
            break;

        case CMD_LINK_SHADER:
            direct::renderer_link_shader_program(cmd.link_params, cmd.resource_slot);
            for (u32 i = 0; i < cmd.link_params.num_constants; ++i)
                memory_free(cmd.link_params.constants[i].name);
            memory_free(cmd.link_params.constants);
            if (cmd.link_params.stream_out_names)
                for (u32 i = 0; i < cmd.link_params.num_stream_out_names; ++i)
                    memory_free(cmd.link_params.stream_out_names[i]);
            memory_free(cmd.link_params.stream_out_names);
            break;

        case CMD_CREATE_INPUT_LAYOUT:
            direct::renderer_create_input_layout(cmd.create_input_layout, cmd.resource_slot);
            memory_free(cmd.create_input_layout.vs_byte_code);
            memory_free(cmd.create_input_layout.input_layout);
            break;

        case CMD_SET_INPUT_LAYOUT:
            direct::renderer_set_input_layout(cmd.command_data_index);
            break;

        case CMD_CREATE_BUFFER:
            direct::renderer_create_buffer(cmd.create_buffer, cmd.resource_slot);
            memory_free(cmd.create_buffer.data);
            break;

        case CMD_SET_VERTEX_BUFFER:
            direct::renderer_set_vertex_buffers(cmd.set_vertex_buffer.buffer_indices, cmd.set_vertex_buffer.num_buffers,
                                                cmd.set_vertex_buffer.start_slot, cmd.set_vertex_buffer.strides,
                                                cmd.set_vertex_buffer.offsets);
            memory_free(cmd.set_vertex_buffer.buffer_indices);
            memory_free(cmd.set_vertex_buffer.strides);
            memory_free(cmd.set_vertex_buffer.offsets);
            break;

        case CMD_SET_INDEX_BUFFER:
            direct::renderer_set_index_buffer(cmd.set_index_buffer.buffer_index, cmd.set_index_buffer.format,
                                              cmd.set_index_buffer.offset);
            break;

        case CMD_DRAW:
            direct::renderer_draw(cmd.draw.vertex_count, cmd.draw.start_vertex, cmd.draw.primitive_topology);
            break;

        case CMD_DRAW_INDEXED:
            direct::renderer_draw_indexed(cmd.draw_indexed.index_count, cmd.draw_indexed.start_index,
                                          cmd.draw_indexed.base_vertex, cmd.draw_indexed.primitive_topology);
            break;

        case CMD_DRAW_INDEXED_INSTANCED:
            direct::renderer_draw_indexed_instanced(
                cmd.draw_indexed_instanced.instance_count, cmd.draw_indexed_instanced.start_instance,
                cmd.draw_indexed_instanced.index_count, cmd.draw_indexed_instanced.start_index,
                cmd.draw_indexed_instanced.base_vertex, cmd.draw_indexed_instanced.primitive_topology);
            break;

        case CMD_CREATE_TEXTURE:
            direct::renderer_create_texture(cmd.create_texture, cmd.resource_slot);
            memory_free(cmd.create_texture.data);
            break;

        case CMD_CREATE_SAMPLER:
            direct::renderer_create_sampler(cmd.create_sampler, cmd.resource_slot);
            break;

        case CMD_SET_TEXTURE:
            direct::renderer_set_texture(cmd.set_texture.texture_index, cmd.set_texture.sampler_index,
                                         cmd.set_texture.resource_slot, cmd.set_texture.shader_type, cmd.set_texture.flags);
            break;

        case CMD_CREATE_RASTER_STATE:
            direct::renderer_create_rasterizer_state(cmd.create_raster_state, cmd.resource_slot);
            break;

        case CMD_SET_RASTER_STATE:
            direct::renderer_set_rasterizer_state(cmd.command_data_index);
            break;

        case CMD_SET_VIEWPORT:
            direct::renderer_set_viewport(cmd.set_viewport);
            break;

        case CMD_SET_SCISSOR_RECT:
            direct::renderer_set_scissor_rect(cmd.set_rect);
            break;

        case CMD_RELEASE_SHADER:
            direct::renderer_release_shader(cmd.set_shader.shader_index, cmd.set_shader.shader_type);
            break;

        case CMD_RELEASE_BUFFER:
            direct::renderer_release_buffer(cmd.command_data_index);
            break;

        case CMD_RELEASE_TEXTURE_2D:
            direct::renderer_release_texture(cmd.command_data_index);
            break;

        case CMD_RELEASE_RASTER_STATE:
            direct::renderer_release_raster_state(cmd.command_data_index);
            break;

        case CMD_CREATE_BLEND_STATE:
            direct::renderer_create_blend_state(cmd.create_blend_state, cmd.resource_slot);
            memory_free(cmd.create_blend_state.render_targets);
            break;

        case CMD_SET_BLEND_STATE:
            direct::renderer_set_blend_state(cmd.command_data_index);
            break;

        case CMD_SET_CONSTANT_BUFFER:
            direct::renderer_set_constant_buffer(cmd.set_constant_buffer.buffer_index, cmd.set_constant_buffer.resource_slot,
                                                 cmd.set_constant_buffer.shader_type);
            break;

        case CMD_UPDATE_BUFFER:
            direct::renderer_update_buffer(cmd.update_buffer.buffer_index, cmd.update_buffer.data,
                                           cmd.update_buffer.data_size, cmd.update_buffer.offset);
            memory_free(cmd.update_buffer.data);
            break;

        case CMD_CREATE_DEPTH_STENCIL_STATE:
            direct::renderer_create_depth_stencil_state(*cmd.p_create_depth_stencil_state, cmd.resource_slot);
            memory_free(cmd.p_create_depth_stencil_state);
            break;

        case CMD_SET_DEPTH_STENCIL_STATE:
            direct::renderer_set_depth_stencil_state(cmd.command_data_index);
            break;

        case CMD_UPDATE_QUERIES:
            renderer_update_queries();
            break;

        case CMD_CREATE_RENDER_TARGET:
            direct::renderer_create_render_target(cmd.create_render_target, cmd.resource_slot);
            break;

        case CMD_SET_TARGETS:
            direct::renderer_set_targets(cmd.set_targets.colour, cmd.set_targets.num_colour, cmd.set_targets.depth);
            break;

        case CMD_SET_TARGETS_CUBE:
            direct::renderer_set_targets(&cmd.set_targets_cube.colour, 1, cmd.set_targets_cube.depth,
                                         cmd.set_targets_cube.colour_face, cmd.set_targets_cube.depth_face);
            break;

        case CMD_CLEAR_CUBE:
            direct::renderer_clear(cmd.clear_cube.clear_state, cmd.clear_cube.colour_face, cmd.clear_cube.depth_face);
            break;

        case CMD_RELEASE_BLEND_STATE:
            direct::renderer_release_blend_state(cmd.command_data_index);
            break;

        case CMD_RELEASE_PROGRAM:
            direct::renderer_release_program(cmd.command_data_index);
            break;

        case CMD_RELEASE_CLEAR_STATE:
            direct::renderer_release_clear_state(cmd.command_data_index);
            break;

        case CMD_RELEASE_RENDER_TARGET:
            direct::renderer_release_render_target(cmd.command_data_index);
            break;

        case CMD_RELEASE_INPUT_LAYOUT:
            direct::renderer_release_input_layout(cmd.command_data_index);
            break;

        case CMD_RELEASE_SAMPLER:
            direct::renderer_release_sampler(cmd.command_data_index);
            break;

        case CMD_RELEASE_DEPTH_STENCIL_STATE:
            direct::renderer_release_depth_stencil_state(cmd.command_data_index);
            break;

        case CMD_SET_SO_TARGET:
            direct::renderer_set_stream_out_target(cmd.command_data_index);
            break;

        case CMD_RESOLVE_TARGET:
            direct::renderer_resolve_target(cmd.resolve_params.render_target, cmd.resolve_params.resolve_type);
            break;

        case CMD_DRAW_AUTO:
            direct::renderer_draw_auto();
            break;

        case CMD_MAP_RESOURCE:
            direct::renderer_read_back_resource(cmd.rrb_params);
            break;

        case CMD_REPLACE_RESOURCE:
            direct::renderer_replace_resource(cmd.replace_resource_params.dest_handle, cmd.replace_resource_params.src_handle,
                                              cmd.replace_resource_params.type);
            break;

        case CMD_CREATE_CLEAR_STATE:
            direct::renderer_create_clear_state(cmd.clear_state_params, cmd.resource_slot);
            break;

        case CMD_PUSH_PERF_MARKER:
            direct::renderer_push_perf_marker(cmd.name);
            break;

        case CMD_POP_PERF_MARKER:
            direct::renderer_pop_perf_marker();
            break;
        }
    }

    //--------------------------------------------------------------------------------------
    //  THREAD SYNCRONISATION
    //--------------------------------------------------------------------------------------

    void renderer_wait_for_jobs();

    static pen::job*           p_job_thread_info;
    static pen::semaphore*     p_consume_semaphore  = nullptr;
    static pen::semaphore*     p_continue_semaphore = nullptr;
    static pen::slot_resources k_renderer_slot_resources;

    void renderer_wait_init()
    {
        thread_semaphore_wait(p_continue_semaphore);
    }

    void renderer_consume_cmd_buffer()
    {
        if (p_consume_semaphore)
        {
            thread_semaphore_signal(p_consume_semaphore, 1);
            thread_semaphore_wait(p_continue_semaphore);
        }
    }

    bool renderer_dispatch()
    {
        if (thread_semaphore_try_wait(p_consume_semaphore))
        {
            // put_pos might change on the producer thread.
            u32 end_pos = put_pos;
            
            // need more commands
            PEN_ASSERT(commands_this_frame < MAX_COMMANDS);
            
            thread_semaphore_signal(p_continue_semaphore, 1);
            
            // some api's need to set the current context on the caller thread.
            direct::renderer_make_context_current();
            
            while (get_pos != end_pos)
            {
                exec_cmd(cmd_buffer[get_pos]);
                
                INC_WRAP(get_pos);
            }
            
            commands_this_frame = 0;
            return true;
        }
        
        return false;
    }
    
    void renderer_wait_for_jobs()
    {
        // this is a dedicated thread which stays for the duration of the program
        thread_semaphore_signal(p_continue_semaphore, 1);

        for (;;)
        {
            if (!renderer_dispatch())
                pen::thread_sleep_us(100);
            
#ifndef WIN32
            if (!pen::os_update())
                break;
#else
            if (thread_semaphore_try_wait(p_job_thread_info->p_sem_exit))
                break;
#endif
        }

#ifdef WIN32
        thread_semaphore_signal(p_continue_semaphore, 1);
        thread_semaphore_signal(p_job_thread_info->p_sem_terminated, 1);
#endif
    }

    resolve_resources g_resolve_resources;

    struct textured_vertex
    {
        float x, y, z, w;
        float u, v;
    };

    void init_resolve_resources()
    {
        textured_vertex quad_vertices[] = {
            -1.0f, -1.0f, 0.5f, 1.0f, // p1
            0.0f,  1.0f,              // uv1

            -1.0f, 1.0f,  0.5f, 1.0f, // p2
            0.0f,  0.0f,              // uv2

            1.0f,  1.0f,  0.5f, 1.0f, // p3
            1.0f,  0.0f,              // uv3

            1.0f,  -1.0f, 0.5f, 1.0f, // p4
            1.0f,  1.0f,              // uv4
        };

        if (renderer_viewport_vup())
        {
            std::swap<f32>(quad_vertices[0].v, quad_vertices[1].v);
            std::swap<f32>(quad_vertices[2].v, quad_vertices[3].v);
        }

        buffer_creation_params bcp;
        bcp.usage_flags      = PEN_USAGE_DEFAULT;
        bcp.bind_flags       = PEN_BIND_VERTEX_BUFFER;
        bcp.cpu_access_flags = 0;

        bcp.buffer_size = sizeof(textured_vertex) * 4;
        bcp.data        = (void*)&quad_vertices[0];

        g_resolve_resources.vertex_buffer = renderer_create_buffer(bcp);

        // create index buffer
        u16 indices[] = {0, 1, 2, 2, 3, 0};

        bcp.usage_flags      = PEN_USAGE_IMMUTABLE;
        bcp.bind_flags       = PEN_BIND_INDEX_BUFFER;
        bcp.cpu_access_flags = 0;
        bcp.buffer_size      = sizeof(u16) * 6;
        bcp.data             = (void*)&indices[0];

        g_resolve_resources.index_buffer = renderer_create_buffer(bcp);

        // create cbuffer
        bcp.usage_flags      = PEN_USAGE_DYNAMIC;
        bcp.bind_flags       = PEN_BIND_CONSTANT_BUFFER;
        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
        bcp.buffer_size      = sizeof(resolve_cbuffer);
        bcp.data             = nullptr;

        g_resolve_resources.constant_buffer = renderer_create_buffer(bcp);
    }

    void renderer_init(void* user_data)
    {
        if (!p_consume_semaphore)
            p_consume_semaphore = thread_semaphore_create(0, 1);

        if (!p_continue_semaphore)
            p_continue_semaphore = thread_semaphore_create(0, 1);

        // clear command buffer
        memory_set(cmd_buffer, 0x0, sizeof(deferred_cmd) * MAX_COMMANDS);

        slot_resources_init(&k_renderer_slot_resources, MAX_RENDERER_RESOURCES);

        // initialise renderer
        u32 bb_res       = slot_resources_get_next(&k_renderer_slot_resources);
        u32 bb_depth_res = slot_resources_get_next(&k_renderer_slot_resources);

        direct::renderer_initialise(user_data, bb_res, bb_depth_res);

        init_resolve_resources();

        //renderer_wait_for_jobs();
    }

    PEN_TRV renderer_thread_function(void* params)
    {
        job_thread_params* job_params = (job_thread_params*)params;

        p_job_thread_info = job_params->job_info;

        p_consume_semaphore  = p_job_thread_info->p_sem_consume;
        p_continue_semaphore = p_job_thread_info->p_sem_continue;

        renderer_init(job_params->user_data);

        return PEN_THREAD_OK;
    }

    //--------------------------------------------------------------------------------------
    //  COMMAND BUFFER API
    //--------------------------------------------------------------------------------------
    void renderer_update_queries()
    {
        cmd_buffer[put_pos].command_index = CMD_UPDATE_QUERIES;

        INC_WRAP(put_pos);
    }

    void renderer_clear(u32 clear_state_index)
    {
        cmd_buffer[put_pos].command_index      = CMD_CLEAR;
        cmd_buffer[put_pos].command_data_index = clear_state_index;

        INC_WRAP(put_pos);
    }

    void renderer_clear_cube(u32 clear_state_index, u32 colour_face, u32 depth_face)
    {
        cmd_buffer[put_pos].command_index          = CMD_CLEAR_CUBE;
        cmd_buffer[put_pos].clear_cube.clear_state = clear_state_index;
        cmd_buffer[put_pos].clear_cube.colour_face = colour_face;
        cmd_buffer[put_pos].clear_cube.depth_face  = depth_face;

        INC_WRAP(put_pos);
    }

    void renderer_present()
    {
        cmd_buffer[put_pos].command_index = CMD_PRESENT;

        INC_WRAP(put_pos);
    }

    u32 renderer_load_shader(const shader_load_params& params)
    {
        cmd_buffer[put_pos].command_index = CMD_LOAD_SHADER;

        cmd_buffer[put_pos].shader_load.byte_code_size = params.byte_code_size;
        cmd_buffer[put_pos].shader_load.type           = params.type;

        if (params.byte_code)
        {
            cmd_buffer[put_pos].shader_load.byte_code = memory_alloc(params.byte_code_size);
            memory_cpy(cmd_buffer[put_pos].shader_load.byte_code, params.byte_code, params.byte_code_size);
        }

        cmd_buffer[put_pos].shader_load.so_decl_entries = nullptr;
        if (params.so_decl_entries)
        {
            cmd_buffer[put_pos].shader_load.so_num_entries = params.so_num_entries;

            u32 entries_size                                = sizeof(stream_out_decl_entry) * params.so_num_entries;
            cmd_buffer[put_pos].shader_load.so_decl_entries = (stream_out_decl_entry*)memory_alloc(entries_size);

            memory_cpy(cmd_buffer[put_pos].shader_load.so_decl_entries, params.so_decl_entries, entries_size);
        }

        u32 resource_slot                 = slot_resources_get_next(&k_renderer_slot_resources);
        cmd_buffer[put_pos].resource_slot = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    u32 renderer_link_shader_program(const shader_link_params& params)
    {
        cmd_buffer[put_pos].command_index = CMD_LINK_SHADER;

        cmd_buffer[put_pos].link_params = params;

        u32 num                                   = params.num_constants;
        u32 layout_size                           = sizeof(constant_layout_desc) * num;
        cmd_buffer[put_pos].link_params.constants = (constant_layout_desc*)memory_alloc(layout_size);

        constant_layout_desc* c = cmd_buffer[put_pos].link_params.constants;
        for (u32 i = 0; i < num; ++i)
        {
            c[i].location = params.constants[i].location;
            c[i].type     = params.constants[i].type;

            u32 len   = string_length(params.constants[i].name);
            c[i].name = (c8*)memory_alloc(len + 1);

            memory_cpy(c[i].name, params.constants[i].name, len);
            c[i].name[len] = '\0';
        }

        cmd_buffer[put_pos].link_params.stream_out_names = nullptr;
        if (params.stream_out_shader != 0)
        {
            u32 num_so                                       = params.num_stream_out_names;
            cmd_buffer[put_pos].link_params.stream_out_names = (c8**)memory_alloc(sizeof(c8*) * num_so);

            c8** so = cmd_buffer[put_pos].link_params.stream_out_names;
            for (u32 i = 0; i < num_so; ++i)
            {
                u32 len = string_length(params.stream_out_names[i]);
                so[i]   = (c8*)memory_alloc(len + 1);

                memory_cpy(so[i], params.stream_out_names[i], len);
                so[i][len] = '\0';
            }
        }

        u32 resource_slot                 = slot_resources_get_next(&k_renderer_slot_resources);
        cmd_buffer[put_pos].resource_slot = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    void renderer_set_shader_program(u32 program_index)
    {
    }

    void renderer_set_shader(u32 shader_index, u32 shader_type)
    {
        cmd_buffer[put_pos].command_index = CMD_SET_SHADER;

        cmd_buffer[put_pos].set_shader.shader_index = shader_index;
        cmd_buffer[put_pos].set_shader.shader_type  = shader_type;

        INC_WRAP(put_pos);
    }

    u32 renderer_create_input_layout(const input_layout_creation_params& params)
    {
        cmd_buffer[put_pos].command_index = CMD_CREATE_INPUT_LAYOUT;

        // simple data
        cmd_buffer[put_pos].create_input_layout.num_elements      = params.num_elements;
        cmd_buffer[put_pos].create_input_layout.vs_byte_code_size = params.vs_byte_code_size;

        // copy buffer
        cmd_buffer[put_pos].create_input_layout.vs_byte_code = memory_alloc(params.vs_byte_code_size);
        memory_cpy(cmd_buffer[put_pos].create_input_layout.vs_byte_code, params.vs_byte_code, params.vs_byte_code_size);

        // copy array
        u32 input_layouts_size                               = sizeof(input_layout_desc) * params.num_elements;
        cmd_buffer[put_pos].create_input_layout.input_layout = (input_layout_desc*)memory_alloc(input_layouts_size);

        memory_cpy(cmd_buffer[put_pos].create_input_layout.input_layout, params.input_layout, input_layouts_size);

        u32 resource_slot                 = slot_resources_get_next(&k_renderer_slot_resources);
        cmd_buffer[put_pos].resource_slot = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    void renderer_set_input_layout(u32 layout_index)
    {
        cmd_buffer[put_pos].command_index = CMD_SET_INPUT_LAYOUT;

        cmd_buffer[put_pos].command_data_index = layout_index;

        INC_WRAP(put_pos);
    }

    u32 renderer_create_buffer(const buffer_creation_params& params)
    {
        cmd_buffer[put_pos].command_index = CMD_CREATE_BUFFER;

        memory_cpy(&cmd_buffer[put_pos].create_buffer, (void*)&params, sizeof(buffer_creation_params));

        if (params.data)
        {
            // make a copy of the buffers data
            cmd_buffer[put_pos].create_buffer.data = memory_alloc(params.buffer_size);
            memory_cpy(cmd_buffer[put_pos].create_buffer.data, params.data, params.buffer_size);
        }

        u32 resource_slot                 = slot_resources_get_next(&k_renderer_slot_resources);
        cmd_buffer[put_pos].resource_slot = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    void renderer_set_vertex_buffer(u32 buffer_index, u32 start_slot, u32 stride, u32 offset)
    {
        renderer_set_vertex_buffers(&buffer_index, 1, start_slot, &stride, &offset);
    }

    void renderer_set_vertex_buffers(u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides,
                                     const u32* offsets)
    {
        cmd_buffer[put_pos].command_index = CMD_SET_VERTEX_BUFFER;

        cmd_buffer[put_pos].set_vertex_buffer.start_slot  = start_slot;
        cmd_buffer[put_pos].set_vertex_buffer.num_buffers = num_buffers;

        cmd_buffer[put_pos].set_vertex_buffer.buffer_indices = (u32*)memory_alloc(sizeof(u32) * num_buffers);
        cmd_buffer[put_pos].set_vertex_buffer.strides        = (u32*)memory_alloc(sizeof(u32) * num_buffers);
        cmd_buffer[put_pos].set_vertex_buffer.offsets        = (u32*)memory_alloc(sizeof(u32) * num_buffers);

        for (u32 i = 0; i < num_buffers; ++i)
        {
            cmd_buffer[put_pos].set_vertex_buffer.buffer_indices[i] = buffer_indices[i];
            cmd_buffer[put_pos].set_vertex_buffer.strides[i]        = strides[i];
            cmd_buffer[put_pos].set_vertex_buffer.offsets[i]        = offsets[i];
        }

        INC_WRAP(put_pos);
    }

    void renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset)
    {
        cmd_buffer[put_pos].command_index                 = CMD_SET_INDEX_BUFFER;
        cmd_buffer[put_pos].set_index_buffer.buffer_index = buffer_index;
        cmd_buffer[put_pos].set_index_buffer.format       = format;
        cmd_buffer[put_pos].set_index_buffer.offset       = offset;

        INC_WRAP(put_pos);
    }

    void renderer_draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology)
    {
        cmd_buffer[put_pos].command_index           = CMD_DRAW;
        cmd_buffer[put_pos].draw.vertex_count       = vertex_count;
        cmd_buffer[put_pos].draw.start_vertex       = start_vertex;
        cmd_buffer[put_pos].draw.primitive_topology = primitive_topology;

        INC_WRAP(put_pos);
    }

    void renderer_draw_indexed(u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology)
    {
        cmd_buffer[put_pos].command_index                   = CMD_DRAW_INDEXED;
        cmd_buffer[put_pos].draw_indexed.index_count        = index_count;
        cmd_buffer[put_pos].draw_indexed.start_index        = start_index;
        cmd_buffer[put_pos].draw_indexed.base_vertex        = base_vertex;
        cmd_buffer[put_pos].draw_indexed.primitive_topology = primitive_topology;

        INC_WRAP(put_pos);
    }

    void renderer_draw_indexed_instanced(u32 instance_count, u32 start_instance, u32 index_count, u32 start_index,
                                         u32 base_vertex, u32 primitive_topology)
    {
        cmd_buffer[put_pos].command_index                             = CMD_DRAW_INDEXED_INSTANCED;
        cmd_buffer[put_pos].draw_indexed_instanced.instance_count     = instance_count;
        cmd_buffer[put_pos].draw_indexed_instanced.start_instance     = start_instance;
        cmd_buffer[put_pos].draw_indexed_instanced.index_count        = index_count;
        cmd_buffer[put_pos].draw_indexed_instanced.start_index        = start_index;
        cmd_buffer[put_pos].draw_indexed_instanced.base_vertex        = base_vertex;
        cmd_buffer[put_pos].draw_indexed_instanced.primitive_topology = primitive_topology;

        INC_WRAP(put_pos);
    }

    u32 renderer_create_render_target(const texture_creation_params& tcp)
    {
        cmd_buffer[put_pos].command_index = CMD_CREATE_RENDER_TARGET;

        memory_cpy(&cmd_buffer[put_pos].create_render_target, (void*)&tcp, sizeof(texture_creation_params));

        u32 resource_slot                 = slot_resources_get_next(&k_renderer_slot_resources);
        cmd_buffer[put_pos].resource_slot = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    u32 renderer_create_texture(const texture_creation_params& tcp)
    {
        switch ((pen::texture_collection_type)tcp.collection_type)
        {
        case TEXTURE_COLLECTION_NONE:
        case TEXTURE_COLLECTION_CUBE:
        case TEXTURE_COLLECTION_VOLUME:
            break;
        default:
            PEN_ASSERT_MSG(0, "inavlid collection type");
            break;
        }

        cmd_buffer[put_pos].command_index = CMD_CREATE_TEXTURE;

        memory_cpy(&cmd_buffer[put_pos].create_texture, (void*)&tcp, sizeof(texture_creation_params));

        cmd_buffer[put_pos].create_texture.data = memory_alloc(tcp.data_size);

        if (tcp.data)
        {
            memory_cpy(cmd_buffer[put_pos].create_texture.data, tcp.data, tcp.data_size);
        }
        else
        {
            cmd_buffer[put_pos].create_texture.data = nullptr;
        }

        u32 resource_slot                 = slot_resources_get_next(&k_renderer_slot_resources);
        cmd_buffer[put_pos].resource_slot = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    void renderer_release_shader(u32 shader_index, u32 shader_type)
    {
        if (!slot_resources_free(&k_renderer_slot_resources, shader_index))
            return;

        cmd_buffer[put_pos].command_index = CMD_RELEASE_SHADER;

        cmd_buffer[put_pos].set_shader.shader_index = shader_index;
        cmd_buffer[put_pos].set_shader.shader_type  = shader_type;

        INC_WRAP(put_pos);
    }

    void renderer_release_buffer(u32 buffer_index)
    {
        if (!slot_resources_free(&k_renderer_slot_resources, buffer_index))
            return;

        cmd_buffer[put_pos].command_index = CMD_RELEASE_BUFFER;

        cmd_buffer[put_pos].command_data_index = buffer_index;

        INC_WRAP(put_pos);
    }

    void renderer_release_texture(u32 texture_index)
    {
        if (!slot_resources_free(&k_renderer_slot_resources, texture_index))
            return;

        cmd_buffer[put_pos].command_index = CMD_RELEASE_TEXTURE_2D;

        cmd_buffer[put_pos].command_data_index = texture_index;

        INC_WRAP(put_pos);
    }

    u32 renderer_create_sampler(const sampler_creation_params& scp)
    {
        cmd_buffer[put_pos].command_index = CMD_CREATE_SAMPLER;

        memory_cpy(&cmd_buffer[put_pos].create_sampler, (void*)&scp, sizeof(sampler_creation_params));

        u32 resource_slot                 = slot_resources_get_next(&k_renderer_slot_resources);
        cmd_buffer[put_pos].resource_slot = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    void renderer_set_texture(u32 texture_index, u32 sampler_index, u32 resource_slot, u32 shader_type, u32 flags)
    {
        cmd_buffer[put_pos].command_index = CMD_SET_TEXTURE;

        cmd_buffer[put_pos].set_texture.texture_index = texture_index;
        cmd_buffer[put_pos].set_texture.sampler_index = sampler_index;
        cmd_buffer[put_pos].set_texture.resource_slot = resource_slot;
        cmd_buffer[put_pos].set_texture.shader_type   = shader_type;
        cmd_buffer[put_pos].set_texture.flags         = flags;

        INC_WRAP(put_pos);
    }

    u32 renderer_create_rasterizer_state(const rasteriser_state_creation_params& rscp)
    {
        cmd_buffer[put_pos].command_index = CMD_CREATE_RASTER_STATE;

        memory_cpy(&cmd_buffer[put_pos].create_raster_state, (void*)&rscp, sizeof(rasteriser_state_creation_params));

        u32 resource_slot                 = slot_resources_get_next(&k_renderer_slot_resources);
        cmd_buffer[put_pos].resource_slot = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    void renderer_set_rasterizer_state(u32 rasterizer_state_index)
    {
        cmd_buffer[put_pos].command_index = CMD_SET_RASTER_STATE;

        cmd_buffer[put_pos].command_data_index = rasterizer_state_index;

        INC_WRAP(put_pos);
    }

    void renderer_set_viewport(const viewport& vp)
    {
        cmd_buffer[put_pos].command_index = CMD_SET_VIEWPORT;

        memory_cpy(&cmd_buffer[put_pos].set_viewport, (void*)&vp, sizeof(viewport));

        INC_WRAP(put_pos);
    }

    void renderer_set_scissor_rect(const rect& r)
    {
        cmd_buffer[put_pos].command_index = CMD_SET_SCISSOR_RECT;

        memory_cpy(&cmd_buffer[put_pos].set_rect, (void*)&r, sizeof(rect));

        INC_WRAP(put_pos);
    }

    void renderer_release_raster_state(u32 raster_state_index)
    {
        if (!slot_resources_free(&k_renderer_slot_resources, raster_state_index))
            return;

        cmd_buffer[put_pos].command_index = CMD_RELEASE_RASTER_STATE;

        cmd_buffer[put_pos].command_data_index = raster_state_index;

        INC_WRAP(put_pos);
    }

    u32 renderer_create_blend_state(const blend_creation_params& bcp)
    {
        cmd_buffer[put_pos].command_index = CMD_CREATE_BLEND_STATE;

        memory_cpy(&cmd_buffer[put_pos].create_blend_state, (void*)&bcp, sizeof(blend_creation_params));

        // alloc and copy the render targets blend modes. to save space in the cmd buffer
        u32   render_target_modes_size                        = sizeof(render_target_blend) * bcp.num_render_targets;
        void* mem                                             = memory_alloc(render_target_modes_size);
        cmd_buffer[put_pos].create_blend_state.render_targets = (render_target_blend*)mem;

        memory_cpy(cmd_buffer[put_pos].create_blend_state.render_targets, (void*)bcp.render_targets,
                   render_target_modes_size);

        u32 resource_slot                 = slot_resources_get_next(&k_renderer_slot_resources);
        cmd_buffer[put_pos].resource_slot = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    void renderer_set_blend_state(u32 blend_state_index)
    {
        cmd_buffer[put_pos].command_index = CMD_SET_BLEND_STATE;

        cmd_buffer[put_pos].command_data_index = blend_state_index;

        INC_WRAP(put_pos);
    }

    void renderer_set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 shader_type)
    {
        if (buffer_index == 0)
            return;

        cmd_buffer[put_pos].command_index = CMD_SET_CONSTANT_BUFFER;

        cmd_buffer[put_pos].set_constant_buffer.buffer_index  = buffer_index;
        cmd_buffer[put_pos].set_constant_buffer.resource_slot = resource_slot;
        cmd_buffer[put_pos].set_constant_buffer.shader_type   = shader_type;

        INC_WRAP(put_pos);
    }

    void renderer_update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset)
    {
        if (buffer_index == 0)
            return;

        cmd_buffer[put_pos].command_index = CMD_UPDATE_BUFFER;

        cmd_buffer[put_pos].update_buffer.buffer_index = buffer_index;
        cmd_buffer[put_pos].update_buffer.data_size    = data_size;
        cmd_buffer[put_pos].update_buffer.offset       = offset;
        cmd_buffer[put_pos].update_buffer.data         = memory_alloc(data_size);
        memory_cpy(cmd_buffer[put_pos].update_buffer.data, data, data_size);

        INC_WRAP(put_pos);
    }

    u32 renderer_create_depth_stencil_state(const depth_stencil_creation_params& dscp)
    {
        cmd_buffer[put_pos].command_index = CMD_CREATE_DEPTH_STENCIL_STATE;

        cmd_buffer[put_pos].p_create_depth_stencil_state =
            (depth_stencil_creation_params*)memory_alloc(sizeof(depth_stencil_creation_params));

        memory_cpy(cmd_buffer[put_pos].p_create_depth_stencil_state, &dscp, sizeof(depth_stencil_creation_params));

        u32 resource_slot                 = slot_resources_get_next(&k_renderer_slot_resources);
        cmd_buffer[put_pos].resource_slot = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    void renderer_set_depth_stencil_state(u32 depth_stencil_state)
    {
        cmd_buffer[put_pos].command_index = CMD_SET_DEPTH_STENCIL_STATE;

        cmd_buffer[put_pos].command_data_index = depth_stencil_state;

        INC_WRAP(put_pos);
    }

    void renderer_set_targets(u32* colour_targets, u32 num_colour_targets, u32 depth_target)
    {
        cmd_buffer[put_pos].command_index          = CMD_SET_TARGETS;
        cmd_buffer[put_pos].set_targets.num_colour = num_colour_targets;
        memory_cpy(&cmd_buffer[put_pos].set_targets.colour, colour_targets, num_colour_targets * sizeof(u32));
        cmd_buffer[put_pos].set_targets.depth = depth_target;

        INC_WRAP(put_pos);
    }

    void renderer_set_targets(u32 colour_target, u32 depth_target)
    {
        cmd_buffer[put_pos].command_index          = CMD_SET_TARGETS;
        cmd_buffer[put_pos].set_targets.num_colour = 1;
        cmd_buffer[put_pos].set_targets.colour[0]  = colour_target;
        cmd_buffer[put_pos].set_targets.depth      = depth_target;

        INC_WRAP(put_pos);
    }

    void renderer_set_targets_cube(u32 colour_target, u32 colour_face, u32 depth_target, u32 depth_face)
    {
        cmd_buffer[put_pos].command_index                = CMD_SET_TARGETS_CUBE;
        cmd_buffer[put_pos].set_targets_cube.colour      = colour_target;
        cmd_buffer[put_pos].set_targets_cube.colour_face = colour_face;
        cmd_buffer[put_pos].set_targets_cube.depth       = depth_target;
        cmd_buffer[put_pos].set_targets_cube.depth_face  = depth_face;

        INC_WRAP(put_pos);
    }

    void renderer_release_blend_state(u32 blend_state)
    {
        if (!slot_resources_free(&k_renderer_slot_resources, blend_state))
            return;

        cmd_buffer[put_pos].command_index      = CMD_RELEASE_BLEND_STATE;
        cmd_buffer[put_pos].command_data_index = blend_state;

        INC_WRAP(put_pos);
    }

    void renderer_release_render_target(u32 render_target)
    {
        if (!slot_resources_free(&k_renderer_slot_resources, render_target))
            return;

        cmd_buffer[put_pos].command_index = CMD_RELEASE_RENDER_TARGET;

        cmd_buffer[put_pos].command_data_index = render_target;

        INC_WRAP(put_pos);
    }

    void renderer_release_clear_state(u32 clear_state)
    {
        if (slot_resources_free(&k_renderer_slot_resources, clear_state))
            return;

        cmd_buffer[put_pos].command_index = CMD_RELEASE_CLEAR_STATE;

        cmd_buffer[put_pos].command_data_index = clear_state;

        INC_WRAP(put_pos);
    }

    void renderer_release_program(u32 program)
    {
        if (!slot_resources_free(&k_renderer_slot_resources, program))
            return;

        cmd_buffer[put_pos].command_index = CMD_RELEASE_PROGRAM;

        cmd_buffer[put_pos].command_data_index = program;

        INC_WRAP(put_pos);
    }

    void renderer_release_input_layout(u32 input_layout)
    {
        if (!slot_resources_free(&k_renderer_slot_resources, input_layout))
            return;

        cmd_buffer[put_pos].command_index = CMD_RELEASE_INPUT_LAYOUT;

        cmd_buffer[put_pos].command_data_index = input_layout;

        INC_WRAP(put_pos);
    }

    void renderer_release_sampler(u32 sampler)
    {
        if (!slot_resources_free(&k_renderer_slot_resources, sampler))
            return;

        cmd_buffer[put_pos].command_index = CMD_RELEASE_SAMPLER;

        cmd_buffer[put_pos].command_data_index = sampler;

        INC_WRAP(put_pos);
    }

    void renderer_release_depth_stencil_state(u32 depth_stencil_state)
    {
        if (!slot_resources_free(&k_renderer_slot_resources, depth_stencil_state))
            return;

        cmd_buffer[put_pos].command_index = CMD_RELEASE_DEPTH_STENCIL_STATE;

        cmd_buffer[put_pos].command_data_index = depth_stencil_state;

        INC_WRAP(put_pos);
    }

    void renderer_set_stream_out_target(u32 buffer_index)
    {
        cmd_buffer[put_pos].command_index = CMD_SET_SO_TARGET;

        cmd_buffer[put_pos].command_data_index = buffer_index;

        INC_WRAP(put_pos);
    }

    void renderer_resolve_target(u32 target, e_msaa_resolve_type type)
    {
        cmd_buffer[put_pos].command_index = CMD_RESOLVE_TARGET;

        cmd_buffer[put_pos].resolve_params.render_target = target;
        cmd_buffer[put_pos].resolve_params.resolve_type  = type;

        INC_WRAP(put_pos);
    }

    void renderer_draw_auto()
    {
        cmd_buffer[put_pos].command_index = CMD_DRAW_AUTO;

        INC_WRAP(put_pos);
    }

    void renderer_read_back_resource(const resource_read_back_params& rrbp)
    {
        cmd_buffer[put_pos].command_index = CMD_MAP_RESOURCE;

        cmd_buffer[put_pos].rrb_params = rrbp;

        INC_WRAP(put_pos);
    }

    void renderer_replace_resource(u32 dest, u32 src, e_renderer_resource type)
    {
        cmd_buffer[put_pos].command_index = CMD_REPLACE_RESOURCE;

        cmd_buffer[put_pos].replace_resource_params = {dest, src, type};

        INC_WRAP(put_pos);
    }

    u32 renderer_create_clear_state(const clear_state& cs)
    {
        u32 resource_slot = slot_resources_get_next(&k_renderer_slot_resources);

        cmd_buffer[put_pos].command_index      = CMD_CREATE_CLEAR_STATE;
        cmd_buffer[put_pos].clear_state_params = cs;
        cmd_buffer[put_pos].resource_slot      = resource_slot;

        INC_WRAP(put_pos);

        return resource_slot;
    }

    void renderer_push_perf_marker(const c8* name)
    {
        cmd_buffer[put_pos].command_index = CMD_PUSH_PERF_MARKER;

        // make copy of string to be able to use temporaries
        u32 len                  = string_length(name);
        cmd_buffer[put_pos].name = (c8*)memory_alloc(len);
        memory_cpy(cmd_buffer[put_pos].name, name, len);
        cmd_buffer[put_pos].name[len] = '\0';

        INC_WRAP(put_pos);
    }

    void renderer_pop_perf_marker()
    {
        cmd_buffer[put_pos].command_index = CMD_POP_PERF_MARKER;

        INC_WRAP(put_pos);
    }
} // namespace pen
