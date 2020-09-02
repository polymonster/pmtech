// renderer_cmd.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include <fstream>

#include "console.h"
#include "data_struct.h"
#include "file_system.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_string.h"
#include "renderer.h"
#include "renderer_shared.h"
#include "slot_resource.h"
#include "str/Str.h"
#include "threads.h"
#include "timer.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

extern pen::window_creation_params pen_window;
pen::resolve_resources             g_resolve_resources;

using namespace pen;

namespace
{
    enum commands : u32
    {
        CMD_NONE = 0,
        CMD_NEW_FRAME,
        CMD_CLEAR,
        CMD_CLEAR_TEXTURE,
        CMD_PRESENT,
        CMD_LOAD_SHADER,
        CMD_SET_SHADER,
        CMD_LINK_SHADER,
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
        CMD_SET_VIEWPORT_RATIO,
        CMD_SET_SCISSOR_RECT_RATIO,
        CMD_RELEASE_RASTER_STATE,
        CMD_CREATE_BLEND_STATE,
        CMD_SET_BLEND_STATE,
        CMD_SET_CONSTANT_BUFFER,
        CMD_SET_STRUCTURED_BUFFER,
        CMD_UPDATE_BUFFER,
        CMD_CREATE_DEPTH_STENCIL_STATE,
        CMD_SET_DEPTH_STENCIL_STATE,
        CMD_UPDATE_QUERIES,
        CMD_CREATE_RENDER_TARGET,
        CMD_SET_TARGETS,
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
        CMD_POP_PERF_MARKER,
        CMD_DISPATCH_COMPUTE,
        CMD_SET_STENCIL_REF
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
        u32 array_index;
    };

    struct clear_cmd
    {
        u32 clear_state;
        u32 array_index;
        u32 texture_index;
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
        u32 bind_flags;
    };

    struct set_buffer_cmd
    {
        u32 buffer_index;
        u32 resource_slot;
        u32 flags;
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

    struct compute_dispatch_params
    {
        uint3 grid;
        uint3 num_threads;
    };

    struct renderer_cmd
    {
        u32 command_index;
        u32 resource_slot;
        u64 frame_index;

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
            set_buffer_cmd                   set_buffer;
            update_buffer_cmd                update_buffer;
            depth_stencil_creation_params*   p_create_depth_stencil_state;
            texture_creation_params          create_render_target;
            set_target_cmd                   set_targets;
            clear_cmd                        clear;
            shader_link_params               link_params;
            resource_read_back_params        rrb_params;
            msaa_resolve_params              resolve_params;
            replace_resource                 replace_resource_params;
            clear_state                      clear_state_params;
            c8*                              name;
            compute_dispatch_params          cs_dispatch;
            u8                               stencil_ref;
        };

        renderer_cmd(){};
    };
        
    // front end render_ctx
    struct fe_render_ctx
    {
        pen::timer*               present_timer = nullptr;
        f64                       present_time = 0.0f;
        pen::resolve_resources    resolve_resources;
        pen::semaphore*           consume_semaphore = nullptr;
        pen::semaphore*           continue_semaphore = nullptr;
        pen::slot_resources       renderer_slot_resources;
        ring_buffer<renderer_cmd> cmd_buffer;
        ring_buffer<renderer_cmd> release_cmd_buffer;
        u32*                      free_slots = nullptr;
        std::atomic<s32>          wait;
    };
    static fe_render_ctx* _ctx;
    static render_ctx     _main_ctx;

} // namespace

namespace pen
{
    void end_frame_internal();
    void new_frame_internal();
    
    void renderer_get_present_time(f32& cpu_ms, f32& gpu_ms)
    {
        extern a_u64 g_gpu_total;

        cpu_ms = _ctx->present_time;
        gpu_ms = (f64)g_gpu_total / 1000.0 / 1000.0;
    }
    
    void exec_cmd(const renderer_cmd& cmd)
    {
        switch (cmd.command_index)
        {
            case CMD_NEW_FRAME:
                new_frame_internal();
                break;
            case CMD_CLEAR:
                direct::renderer_clear(cmd.clear.clear_state, cmd.clear.array_index, cmd.clear.array_index);
                break;
            case CMD_CLEAR_TEXTURE:
                direct::renderer_clear_texture(cmd.clear.clear_state, cmd.clear.texture_index);
                break;
            case CMD_PRESENT:
                direct::renderer_present();
                end_frame_internal();
                _ctx->present_time = timer_elapsed_ms(_ctx->present_timer);
                timer_start(_ctx->present_timer);
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
                                             cmd.set_texture.resource_slot, cmd.set_texture.bind_flags);
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

            case CMD_SET_VIEWPORT_RATIO:
                _renderer_set_viewport_ratio(cmd.set_viewport);
                break;

            case CMD_SET_SCISSOR_RECT_RATIO:
                _renderer_set_scissor_ratio(cmd.set_rect);
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
                direct::renderer_set_constant_buffer(cmd.set_buffer.buffer_index, cmd.set_buffer.resource_slot,
                                                     cmd.set_buffer.flags);
                break;

            case CMD_SET_STRUCTURED_BUFFER:
                direct::renderer_set_structured_buffer(cmd.set_buffer.buffer_index, cmd.set_buffer.resource_slot,
                                                       cmd.set_buffer.flags);
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
                direct::renderer_set_targets(cmd.set_targets.colour, cmd.set_targets.num_colour, cmd.set_targets.depth,
                                             cmd.set_targets.array_index, cmd.set_targets.array_index);
                break;

            case CMD_RELEASE_BLEND_STATE:
                direct::renderer_release_blend_state(cmd.command_data_index);
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
                direct::renderer_resolve_target(cmd.resolve_params.render_target, cmd.resolve_params.resolve_type,
                                                _ctx->resolve_resources);
                break;

            case CMD_DRAW_AUTO:
                direct::renderer_draw_auto();
                break;

            case CMD_MAP_RESOURCE:
                direct::renderer_read_back_resource(cmd.rrb_params);
                break;

            case CMD_REPLACE_RESOURCE:
                direct::renderer_replace_resource(cmd.replace_resource_params.dest_handle,
                                                  cmd.replace_resource_params.src_handle, cmd.replace_resource_params.type);
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

            case CMD_DISPATCH_COMPUTE:
                direct::renderer_dispatch_compute(cmd.cs_dispatch.grid, cmd.cs_dispatch.num_threads);
                break;

            case CMD_SET_STENCIL_REF:
                direct::renderer_set_stencil_ref(cmd.stencil_ref);
                break;
        }
    }
    
    //
    // 
    //
    
    void renderer_wait_init()
    {
        semaphore_wait(_ctx->continue_semaphore);
    }

    void renderer_consume_cmd_buffer()
    {
        while (_ctx->wait > 0)
            pen::thread_sleep_ms(1);

        if (_ctx->consume_semaphore)
        {
            semaphore_post(_ctx->consume_semaphore, 1);
            semaphore_wait(_ctx->continue_semaphore);
        }

        // sync on window surface
        direct::renderer_sync();

        _ctx->wait++;
    }
    
    void new_frame_internal()
    {
        // free slots we have now deleted the resources for
        u32 ns = sb_count(_ctx->free_slots);
        for(u32 i = 0; i < ns; ++i)
        {
            slot_resources_free(&_ctx->renderer_slot_resources, _ctx->free_slots[i]);
        }
        sb_free(_ctx->free_slots);
        _ctx->free_slots = nullptr;
        
        // some api's need to set the current context on the caller thread.
        direct::renderer_new_frame();
    }
    
    void end_frame_internal()
    {
        // check the release cmd_buffer.. we need to wait a few frames before releasing resources
        // so they arent in flight on the gpu
        static const u32 k_waitFrames = 6;
        for(;;)
        {
            renderer_cmd* cmd = _ctx->release_cmd_buffer.check();
            u64 cf = pen::_renderer_frame_index();
            if(!cmd || cf - cmd->frame_index < k_waitFrames)
                break;
                
            cmd = _ctx->release_cmd_buffer.get();
            if (cmd->resource_slot)
            {
                if (cmd)
                    exec_cmd(*cmd);

                sb_push(_ctx->free_slots, cmd->resource_slot);
            }
        }

        direct::renderer_end_frame();
        semaphore_post(_ctx->continue_semaphore, 1);
        _ctx->wait--;
    }

    void renderer_wait_for_jobs()
    {
        // this is a dedicated thread which stays for the duration of the program
        semaphore_post(_ctx->continue_semaphore, 1);

        for (;;)
        {
            renderer_cmd* cmd = _ctx->cmd_buffer.get();
            
            while(cmd)
            {
                exec_cmd(*cmd);

                // break at present to re-call os update
                if (cmd->command_index == CMD_PRESENT)
                    break;

                cmd = _ctx->cmd_buffer.get();
            }
            
            if(!pen::os_update())
                break;
        }
    }
    
    bool renderer_dispatch()
    {
        // this function is invoked from mtk draw in view
        //if we start renderin  we need to wait for present to prevent command buffer being released before ending encoding

        renderer_cmd* cmd = _ctx->cmd_buffer.get();
        bool started = cmd;
        while(cmd)
        {
            exec_cmd(*cmd);

            // break at present to re-call os update
            if (cmd->command_index == CMD_PRESENT)
                break;
                    
            cmd = _ctx->cmd_buffer.get();
        }
        
        direct::renderer_retain();
        return started;
    }

    void init_resolve_resources(fe_render_ctx* ctx)
    {
        struct textured_vertex
        {
            float x, y, z, w;
            float u, v;
        };

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
            // flip gl in the y.
            std::swap<f32>(quad_vertices[0].v, quad_vertices[1].v);
            std::swap<f32>(quad_vertices[2].v, quad_vertices[3].v);
        }

        buffer_creation_params bcp;
        bcp.usage_flags = PEN_USAGE_DEFAULT;
        bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
        bcp.cpu_access_flags = 0;

        bcp.buffer_size = sizeof(textured_vertex) * 4;
        bcp.data = (void*)&quad_vertices[0];

        ctx->resolve_resources.vertex_buffer = renderer_create_buffer(bcp);

        // create index buffer
        u16 indices[] = {0, 1, 2, 2, 3, 0};

        bcp.usage_flags = PEN_USAGE_IMMUTABLE;
        bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
        bcp.cpu_access_flags = 0;
        bcp.buffer_size = sizeof(u16) * 6;
        bcp.data = (void*)&indices[0];

        ctx->resolve_resources.index_buffer = renderer_create_buffer(bcp);

        // create cbuffer
        bcp.usage_flags = PEN_USAGE_DYNAMIC;
        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
        bcp.buffer_size = sizeof(resolve_cbuffer);
        bcp.data = nullptr;

        ctx->resolve_resources.constant_buffer = renderer_create_buffer(bcp);
        g_resolve_resources = ctx->resolve_resources;
    }
    
    render_ctx renderer_create_context(u32 max_commands)
    {
        fe_render_ctx* new_ctx = new fe_render_ctx();
        new_ctx->cmd_buffer.create(max_commands);
        new_ctx->release_cmd_buffer.create(1024);
        new_ctx->present_timer = timer_create();
        timer_start(new_ctx->present_timer);
        new_ctx->present_time = 0.0f;
        new_ctx->consume_semaphore = semaphore_create(0, 1);
        new_ctx->continue_semaphore = semaphore_create(0, 1);
        slot_resources_init(&new_ctx->renderer_slot_resources, 2048);

        return (render_ctx*)new_ctx;
    }
    
    void renderer_init(void* user_data, bool wait_for_jobs, u32 max_commands)
    {
        // create main render context and bind it
        _main_ctx = renderer_create_context(max_commands);
        _ctx = (fe_render_ctx*)_main_ctx;
        
        // bb is backbuffer depth and colour
        u32 bb_res = slot_resources_get_next(&_ctx->renderer_slot_resources);
        u32 bb_depth_res = slot_resources_get_next(&_ctx->renderer_slot_resources);
        // reserve a bunch more slots for interal renderer implementations
        for (s64 i = 0; i < 10; ++i)
            slot_resources_get_next(&_ctx->renderer_slot_resources);

        // initialise backend renderer
        direct::renderer_initialise(user_data, bb_res, bb_depth_res);
        
        init_resolve_resources(_ctx);

        if (wait_for_jobs)
            renderer_wait_for_jobs();
    }
    
    // graphics test
    static bool s_run_test = false;
    static void renderer_test_read_complete(void* data, u32 row_pitch, u32 depth_pitch, u32 block_size)
    {
        // get reference image
        Str reference_filename = "data/textures/";
        reference_filename.appendf("%s%s", pen::window_get_title(), ".dds");

        void* file_data = nullptr;
        u32   file_data_size = 0;
        u32   pen_err = pen::filesystem_read_file_to_buffer(reference_filename.c_str(), &file_data, file_data_size);
        
        // platform specific output dir
        auto& ri = renderer_get_info();
        Str renderer_name = ri.api_version;
        renderer_name.append("_");
        renderer_name.append(renderer_get_info().renderer);
        renderer_name = str_replace_chars(renderer_name, '.', '_');
        renderer_name = str_replace_chars(renderer_name, ' ', '_');
        renderer_name = str_replace_chars(renderer_name, '/', '_');
        renderer_name = str_replace_chars(renderer_name, '\\', '_');
        renderer_name = str_to_lower(renderer_name);
        
        Str root_dir = "../../test_results";
        root_dir = str_sanitize_filepath(root_dir);

        Str output_dir = "";
        output_dir.appendf("../../test_results/%s/", renderer_name.c_str());
        output_dir = str_sanitize_filepath(output_dir);

        Str mk_output_dir = "mkdir ";
        mk_output_dir.append(output_dir.c_str());

        Str mk_root_dir = "mkdir ";
        mk_root_dir.append(root_dir.c_str());

        PEN_SYSTEM(mk_root_dir.c_str());
        PEN_SYSTEM(mk_output_dir.c_str());

        // swizzle bgra to rgba
        if(ri.caps & PEN_CAPS_BACKBUFFER_BGRA)
        {
            u8* swizz = (u8*)data;
            for (u32 i = 0; i < depth_pitch; i += 4)
                swap(swizz[i], swizz[i+2]);
        }
                
        // make test results
        u32 diffs = 0;
        if (pen_err == PEN_ERR_OK)
        {
            // file exists do image compare
            u8* ref_image = (u8*)file_data + 124; // size of DDS header and we know its RGBA8
            u8* cmp_image = (u8*)data;

            for (u32 i = 0; i < depth_pitch; i += 4)
            {
                if (ref_image[i + 2] != cmp_image[i + 0])
                    ++diffs;
                if (ref_image[i + 1] != cmp_image[i + 1])
                    ++diffs;
                if (ref_image[i + 0] != cmp_image[i + 2])
                    ++diffs;
                if (ref_image[i + 3] != cmp_image[i + 3])
                    ++diffs;
            }

            free(file_data);
        }
        
        // write result image
        Str output_file = "";
        output_file.appendf("%s/%s.jpg", output_dir.c_str(), pen_window.window_title);
        stbi_write_jpg(output_file.c_str(), pen_window.width, pen_window.height, 4, data, 50);

        // write result stats
        f32 percentage = 100.0f / ((f32)depth_pitch / (f32)diffs);
        PEN_LOG("test complete %i diffs: out of %i (%2.3f%%)\n", diffs, depth_pitch, percentage);

        Str output_results_file = "";
        output_results_file.appendf("%s/%s.txt", output_dir.c_str(), pen_window.window_title);

        std::ofstream ofs(output_results_file.c_str());
        ofs << diffs << "/" << depth_pitch << ", " << percentage;
        ofs.close();

        // cya!
        exit(0);
        pen::os_terminate(0);
    }

    void renderer_test_enable()
    {
        PEN_LOG("renderer test enabled.\n");
        s_run_test = true;
    }

    void renderer_test_run()
    {
        if (!s_run_test)
            return;

        // wait for the first swap.
        u32 count = _renderer_frame_index();
        if (count < 3)
            return;

        // run once, wait for result
        static bool ran = false;
        if (ran)
            return;

        PEN_LOG("running test %s.\n", pen_window.window_title);

        pen::resource_read_back_params rrbp;
        rrbp.block_size = 4;
        rrbp.format = PEN_TEX_FORMAT_RGBA8_UNORM;
        rrbp.resource_index = PEN_BACK_BUFFER_COLOUR;
        rrbp.row_pitch = pen_window.width * rrbp.block_size;
        rrbp.depth_pitch = pen_window.width * pen_window.height * rrbp.block_size;
        rrbp.data_size = pen_window.width * pen_window.height * rrbp.block_size;
        rrbp.call_back_function = &renderer_test_read_complete;

        pen::renderer_read_back_resource(rrbp);

        ran = true;
    }
} // namespace pen

namespace pen
{
    //
    // allow shared context for hot loaded dll
    //
    
    void renderer_set_current_ctx(render_ctx ctx)
    {
        _ctx = (fe_render_ctx*)ctx;
    }
    
    render_ctx renderer_get_main_context()
    {
        return _main_ctx;
    }
    
    //
    // command buffer api
    //

    void renderer_new_frame()
    {
        renderer_cmd cmd;
        cmd.command_index = CMD_NEW_FRAME;
        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_update_queries()
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_UPDATE_QUERIES;
        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_clear(u32 clear_state_index, u32 array_index)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_CLEAR;
        cmd.clear.clear_state = clear_state_index;
        cmd.clear.array_index = array_index;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_clear_texture(u32 clear_state_index, u32 texture)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_CLEAR_TEXTURE;
        cmd.clear.clear_state = clear_state_index;
        cmd.clear.texture_index = texture;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_present()
    {
        pen::renderer_test_run();
                
        renderer_cmd cmd;
        cmd.command_index = CMD_PRESENT;
        _ctx->cmd_buffer.put(cmd);
    }

    u32 renderer_load_shader(const shader_load_params& params)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_LOAD_SHADER;

        cmd.shader_load.byte_code_size = params.byte_code_size;
        cmd.shader_load.type = params.type;

        if (params.byte_code)
        {
            cmd.shader_load.byte_code = memory_alloc(params.byte_code_size);
            memcpy(cmd.shader_load.byte_code, params.byte_code, params.byte_code_size);
        }

        cmd.shader_load.so_decl_entries = nullptr;
        if (params.so_decl_entries)
        {
            cmd.shader_load.so_num_entries = params.so_num_entries;

            u32 entries_size = sizeof(stream_out_decl_entry) * params.so_num_entries;
            cmd.shader_load.so_decl_entries = (stream_out_decl_entry*)memory_alloc(entries_size);

            memcpy(cmd.shader_load.so_decl_entries, params.so_decl_entries, entries_size);
        }

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);
        
        return resource_slot;
    }

    u32 renderer_link_shader_program(const shader_link_params& params)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_LINK_SHADER;

        cmd.link_params = params;

        u32 num = params.num_constants;
        u32 layout_size = sizeof(constant_layout_desc) * num;
        cmd.link_params.constants = (constant_layout_desc*)memory_alloc(layout_size);

        constant_layout_desc* c = cmd.link_params.constants;
        for (u32 i = 0; i < num; ++i)
        {
            c[i].location = params.constants[i].location;
            c[i].type = params.constants[i].type;

            u32 len = string_length(params.constants[i].name);
            c[i].name = (c8*)memory_alloc(len + 1);

            memcpy(c[i].name, params.constants[i].name, len);
            c[i].name[len] = '\0';
        }

        cmd.link_params.stream_out_names = nullptr;
        if (params.stream_out_shader != 0)
        {
            u32 num_so = params.num_stream_out_names;
            cmd.link_params.stream_out_names = (c8**)memory_alloc(sizeof(c8*) * num_so);

            c8** so = cmd.link_params.stream_out_names;
            for (u32 i = 0; i < num_so; ++i)
            {
                u32 len = string_length(params.stream_out_names[i]);
                so[i] = (c8*)memory_alloc(len + 1);

                memcpy(so[i], params.stream_out_names[i], len);
                so[i][len] = '\0';
            }
        }

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);

        return resource_slot;
    }

    void renderer_set_shader(u32 shader_index, u32 shader_type)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_SHADER;

        cmd.set_shader.shader_index = shader_index;
        cmd.set_shader.shader_type = shader_type;

        _ctx->cmd_buffer.put(cmd);
    }

    u32 renderer_create_input_layout(const input_layout_creation_params& params)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_CREATE_INPUT_LAYOUT;

        // simple data
        cmd.create_input_layout.num_elements = params.num_elements;
        cmd.create_input_layout.vs_byte_code_size = params.vs_byte_code_size;

        // copy buffer
        cmd.create_input_layout.vs_byte_code = memory_alloc(params.vs_byte_code_size);
        memcpy(cmd.create_input_layout.vs_byte_code, params.vs_byte_code, params.vs_byte_code_size);

        // copy array
        u32 input_layouts_size = sizeof(input_layout_desc) * params.num_elements;
        cmd.create_input_layout.input_layout = (input_layout_desc*)memory_alloc(input_layouts_size);

        memcpy(cmd.create_input_layout.input_layout, params.input_layout, input_layouts_size);

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);

        return resource_slot;
    }

    void renderer_set_input_layout(u32 layout_index)
    {
        renderer_cmd cmd;
        cmd.command_index = CMD_SET_INPUT_LAYOUT;
        cmd.command_data_index = layout_index;

        _ctx->cmd_buffer.put(cmd);
    }

    u32 renderer_create_buffer(const buffer_creation_params& params)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_CREATE_BUFFER;

        memcpy(&cmd.create_buffer, (void*)&params, sizeof(buffer_creation_params));

        if (params.data)
        {
            // make a copy of the buffers data
            cmd.create_buffer.data = memory_alloc(params.buffer_size);
            memcpy(cmd.create_buffer.data, params.data, params.buffer_size);
        }

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);

        return resource_slot;
    }

    void renderer_set_vertex_buffer(u32 buffer_index, u32 start_slot, u32 stride, u32 offset)
    {
        renderer_set_vertex_buffers(&buffer_index, 1, start_slot, &stride, &offset);
    }

    void renderer_set_vertex_buffers(u32* buffer_indices, u32 num_buffers, u32 start_slot, const u32* strides,
                                     const u32* offsets)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_VERTEX_BUFFER;

        cmd.set_vertex_buffer.start_slot = start_slot;
        cmd.set_vertex_buffer.num_buffers = num_buffers;

        cmd.set_vertex_buffer.buffer_indices = (u32*)memory_alloc(sizeof(u32) * num_buffers);
        cmd.set_vertex_buffer.strides = (u32*)memory_alloc(sizeof(u32) * num_buffers);
        cmd.set_vertex_buffer.offsets = (u32*)memory_alloc(sizeof(u32) * num_buffers);

        for (u32 i = 0; i < num_buffers; ++i)
        {
            cmd.set_vertex_buffer.buffer_indices[i] = buffer_indices[i];
            cmd.set_vertex_buffer.strides[i] = strides[i];
            cmd.set_vertex_buffer.offsets[i] = offsets[i];
        }

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_set_index_buffer(u32 buffer_index, u32 format, u32 offset)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_INDEX_BUFFER;
        cmd.set_index_buffer.buffer_index = buffer_index;
        cmd.set_index_buffer.format = format;
        cmd.set_index_buffer.offset = offset;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_draw(u32 vertex_count, u32 start_vertex, u32 primitive_topology)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_DRAW;
        cmd.draw.vertex_count = vertex_count;
        cmd.draw.start_vertex = start_vertex;
        cmd.draw.primitive_topology = primitive_topology;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_draw_indexed(u32 index_count, u32 start_index, u32 base_vertex, u32 primitive_topology)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_DRAW_INDEXED;
        cmd.draw_indexed.index_count = index_count;
        cmd.draw_indexed.start_index = start_index;
        cmd.draw_indexed.base_vertex = base_vertex;
        cmd.draw_indexed.primitive_topology = primitive_topology;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_draw_indexed_instanced(u32 instance_count, u32 start_instance, u32 index_count, u32 start_index,
                                         u32 base_vertex, u32 primitive_topology)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_DRAW_INDEXED_INSTANCED;
        cmd.draw_indexed_instanced.instance_count = instance_count;
        cmd.draw_indexed_instanced.start_instance = start_instance;
        cmd.draw_indexed_instanced.index_count = index_count;
        cmd.draw_indexed_instanced.start_index = start_index;
        cmd.draw_indexed_instanced.base_vertex = base_vertex;
        cmd.draw_indexed_instanced.primitive_topology = primitive_topology;

        _ctx->cmd_buffer.put(cmd);
    }

    u32 renderer_create_render_target(const texture_creation_params& tcp)
    {
        renderer_cmd cmd;

        PEN_ASSERT(tcp.width != 0 && tcp.height != 0);
        if(tcp.collection_type == pen::TEXTURE_COLLECTION_ARRAY)
        {
            PEN_ASSERT(tcp.num_arrays > 0);
        }

        cmd.command_index = CMD_CREATE_RENDER_TARGET;

        memcpy(&cmd.create_render_target, (void*)&tcp, sizeof(texture_creation_params));

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);

        return resource_slot;
    }

    u32 renderer_create_texture(const texture_creation_params& tcp)
    {
        renderer_cmd cmd;

        switch ((pen::texture_collection_type)tcp.collection_type)
        {
            case TEXTURE_COLLECTION_NONE:
            case TEXTURE_COLLECTION_CUBE:
            case TEXTURE_COLLECTION_VOLUME:
            case TEXTURE_COLLECTION_ARRAY:
                break;
            default:
                PEN_ASSERT_MSG(0, "inavlid collection type");
                break;
        }

        cmd.command_index = CMD_CREATE_TEXTURE;

        memcpy(&cmd.create_texture, (void*)&tcp, sizeof(texture_creation_params));

        cmd.create_texture.data = memory_alloc(tcp.data_size);

        if (tcp.data)
        {
            memcpy(cmd.create_texture.data, tcp.data, tcp.data_size);
        }
        else
        {
            cmd.create_texture.data = nullptr;
        }

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);

        return resource_slot;
    }

    u32 renderer_create_sampler(const sampler_creation_params& scp)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_CREATE_SAMPLER;

        memcpy(&cmd.create_sampler, (void*)&scp, sizeof(sampler_creation_params));

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);

        return resource_slot;
    }

    void renderer_set_texture(u32 texture_index, u32 sampler_index, u32 resource_slot, u32 bind_flags)
    {
        renderer_cmd cmd;
        
        cmd.command_index = CMD_SET_TEXTURE;

        cmd.set_texture.texture_index = texture_index;
        cmd.set_texture.sampler_index = sampler_index;
        cmd.set_texture.resource_slot = resource_slot;
        cmd.set_texture.bind_flags = bind_flags;

        _ctx->cmd_buffer.put(cmd);
    }

    u32 renderer_create_rasterizer_state(const rasteriser_state_creation_params& rscp)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_CREATE_RASTER_STATE;

        memcpy(&cmd.create_raster_state, (void*)&rscp, sizeof(rasteriser_state_creation_params));

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);

        return resource_slot;
    }

    void renderer_set_rasterizer_state(u32 rasterizer_state_index)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_RASTER_STATE;

        cmd.command_data_index = rasterizer_state_index;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_set_viewport(const viewport& vp)
    {
        renderer_cmd cmd;
        cmd.command_index = CMD_SET_VIEWPORT;
        memcpy(&cmd.set_viewport, (void*)&vp, sizeof(viewport));
        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_set_scissor_rect(const rect& r)
    {
        renderer_cmd cmd;
        cmd.command_index = CMD_SET_SCISSOR_RECT;
        memcpy(&cmd.set_rect, (void*)&r, sizeof(rect));
        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_set_viewport_ratio(const viewport& vp)
    {
        renderer_cmd cmd;
        cmd.command_index = CMD_SET_VIEWPORT_RATIO;
        memcpy(&cmd.set_viewport, (void*)&vp, sizeof(viewport));
        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_set_scissor_rect_ratio(const rect& r)
    {
        renderer_cmd cmd;
        cmd.command_index = CMD_SET_SCISSOR_RECT_RATIO;
        memcpy(&cmd.set_rect, (void*)&r, sizeof(rect));
        _ctx->cmd_buffer.put(cmd);
    }

    u32 renderer_create_blend_state(const blend_creation_params& bcp)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_CREATE_BLEND_STATE;

        memcpy(&cmd.create_blend_state, (void*)&bcp, sizeof(blend_creation_params));

        // alloc and copy the render targets blend modes. to save space in the cmd buffer
        u32   render_target_modes_size = sizeof(render_target_blend) * bcp.num_render_targets;
        void* mem = memory_alloc(render_target_modes_size);
        cmd.create_blend_state.render_targets = (render_target_blend*)mem;

        memcpy(cmd.create_blend_state.render_targets, (void*)bcp.render_targets, render_target_modes_size);

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);

        return resource_slot;
    }

    void renderer_set_blend_state(u32 blend_state_index)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_BLEND_STATE;

        cmd.command_data_index = blend_state_index;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_set_constant_buffer(u32 buffer_index, u32 resource_slot, u32 flags)
    {
        renderer_cmd cmd;
        
        cmd.command_index = CMD_SET_CONSTANT_BUFFER;

        cmd.set_buffer.buffer_index = buffer_index;
        cmd.set_buffer.resource_slot = resource_slot;
        cmd.set_buffer.flags = flags;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_set_structured_buffer(u32 buffer_index, u32 resource_slot, u32 flags)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_STRUCTURED_BUFFER;

        cmd.set_buffer.buffer_index = buffer_index;
        cmd.set_buffer.resource_slot = resource_slot;
        cmd.set_buffer.flags = flags;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_update_buffer(u32 buffer_index, const void* data, u32 data_size, u32 offset)
    {
        renderer_cmd cmd;
        
        if (buffer_index == 0)
            return;

        cmd.command_index = CMD_UPDATE_BUFFER;

        cmd.update_buffer.buffer_index = buffer_index;
        cmd.update_buffer.data_size = data_size;
        cmd.update_buffer.offset = offset;
        cmd.update_buffer.data = memory_alloc(data_size);
        memcpy(cmd.update_buffer.data, data, data_size);

        _ctx->cmd_buffer.put(cmd);
    }

    u32 renderer_create_depth_stencil_state(const depth_stencil_creation_params& dscp)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_CREATE_DEPTH_STENCIL_STATE;

        cmd.p_create_depth_stencil_state =
            (depth_stencil_creation_params*)memory_alloc(sizeof(depth_stencil_creation_params));

        memcpy(cmd.p_create_depth_stencil_state, &dscp, sizeof(depth_stencil_creation_params));

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);

        return resource_slot;
    }

    void renderer_set_depth_stencil_state(u32 depth_stencil_state)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_DEPTH_STENCIL_STATE;

        cmd.command_data_index = depth_stencil_state;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_set_targets(u32* colour_targets, u32 num_colour_targets, u32 depth_target, u32 array_index)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_TARGETS;
        cmd.set_targets.num_colour = num_colour_targets;
        memcpy(&cmd.set_targets.colour, colour_targets, num_colour_targets * sizeof(u32));
        cmd.set_targets.depth = depth_target;
        cmd.set_targets.array_index = array_index;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_set_targets(u32 colour_target, u32 depth_target)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_TARGETS;
        cmd.set_targets.num_colour = is_valid(colour_target) ? 1 : 0;
        cmd.set_targets.colour[0] = colour_target;
        cmd.set_targets.depth = depth_target;
        cmd.set_targets.array_index = 0;

        _ctx->cmd_buffer.put(cmd);
    }
    
    void renderer_release_shader(u32 shader_index, u32 shader_type)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RELEASE_SHADER;
        cmd.frame_index = pen::_renderer_frame_index();
        cmd.resource_slot = shader_index;
        cmd.set_shader.shader_index = shader_index;
        cmd.set_shader.shader_type = shader_type;

        _ctx->release_cmd_buffer.put(cmd);
    }

    void renderer_release_buffer(u32 buffer_index)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RELEASE_BUFFER;
        cmd.frame_index = pen::_renderer_frame_index();
        cmd.resource_slot = buffer_index;
        cmd.command_data_index = buffer_index;

        _ctx->release_cmd_buffer.put(cmd);
    }

    void renderer_release_texture(u32 texture_index)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RELEASE_TEXTURE_2D;
        cmd.resource_slot = texture_index;
        cmd.command_data_index = texture_index;
        cmd.frame_index = pen::_renderer_frame_index();

        _ctx->release_cmd_buffer.put(cmd);
    }

    void renderer_release_blend_state(u32 blend_state)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RELEASE_BLEND_STATE;
        cmd.frame_index = pen::_renderer_frame_index();
        cmd.resource_slot = blend_state;
        cmd.command_data_index = blend_state;

        _ctx->release_cmd_buffer.put(cmd);
    }

    void renderer_release_render_target(u32 render_target)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RELEASE_RENDER_TARGET;
        cmd.frame_index = pen::_renderer_frame_index();
        cmd.resource_slot = render_target;
        cmd.command_data_index = render_target;

        _ctx->release_cmd_buffer.put(cmd);
    }

    void renderer_release_clear_state(u32 clear_state)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RELEASE_CLEAR_STATE;
        cmd.frame_index = pen::_renderer_frame_index();
        cmd.resource_slot = clear_state;
        cmd.command_data_index = clear_state;

        _ctx->release_cmd_buffer.put(cmd);
    }

    void renderer_release_input_layout(u32 input_layout)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RELEASE_INPUT_LAYOUT;
        cmd.frame_index = pen::_renderer_frame_index();
        cmd.resource_slot = input_layout;
        cmd.command_data_index = input_layout;

        _ctx->release_cmd_buffer.put(cmd);
    }

    void renderer_release_sampler(u32 sampler)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RELEASE_SAMPLER;
        cmd.frame_index = pen::_renderer_frame_index();
        cmd.resource_slot = sampler;
        cmd.command_data_index = sampler;

        _ctx->release_cmd_buffer.put(cmd);
    }

    void renderer_release_depth_stencil_state(u32 depth_stencil_state)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RELEASE_DEPTH_STENCIL_STATE;
        cmd.frame_index = pen::_renderer_frame_index();
        cmd.resource_slot = depth_stencil_state;
        cmd.command_data_index = depth_stencil_state;

        _ctx->release_cmd_buffer.put(cmd);
    }
    
    void renderer_release_raster_state(u32 raster_state_index)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RELEASE_RASTER_STATE;
        cmd.resource_slot = raster_state_index;
        cmd.command_data_index = raster_state_index;

        _ctx->release_cmd_buffer.put(cmd);
    }

    void renderer_set_stream_out_target(u32 buffer_index)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_SO_TARGET;
        cmd.command_data_index = buffer_index;
        
        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_resolve_target(u32 target, e_msaa_resolve_type type)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_RESOLVE_TARGET;

        cmd.resolve_params.render_target = target;
        cmd.resolve_params.resolve_type = type;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_draw_auto()
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_DRAW_AUTO;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_dispatch_compute(uint3 grid, uint3 num_threads)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_DISPATCH_COMPUTE;
        cmd.cs_dispatch.grid = grid;
        cmd.cs_dispatch.num_threads = num_threads;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_read_back_resource(const resource_read_back_params& rrbp)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_MAP_RESOURCE;

        cmd.rrb_params = rrbp;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_replace_resource(u32 dest, u32 src, e_renderer_resource type)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_REPLACE_RESOURCE;

        cmd.replace_resource_params = {dest, src, type};

        _ctx->cmd_buffer.put(cmd);
    }

    u32 renderer_create_clear_state(const clear_state& cs)
    {
        renderer_cmd cmd;

        u32 resource_slot = slot_resources_get_next(&_ctx->renderer_slot_resources);

        cmd.command_index = CMD_CREATE_CLEAR_STATE;
        cmd.clear_state_params = cs;
        cmd.resource_slot = resource_slot;

        _ctx->cmd_buffer.put(cmd);

        return resource_slot;
    }

    void renderer_set_stencil_ref(u8 ref)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_SET_STENCIL_REF;
        cmd.stencil_ref = ref;

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_push_perf_marker(const c8* name)
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_PUSH_PERF_MARKER;

        // make copy of string to be able to use temporaries
        u32 len = string_length(name);
        cmd.name = (c8*)memory_alloc(len);
        memcpy(cmd.name, name, len);
        cmd.name[len] = '\0';

        _ctx->cmd_buffer.put(cmd);
    }

    void renderer_pop_perf_marker()
    {
        renderer_cmd cmd;

        cmd.command_index = CMD_POP_PERF_MARKER;

        _ctx->cmd_buffer.put(cmd);
    }
}
