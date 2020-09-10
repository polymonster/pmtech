#include "dev_ui.h"
#include "file_system.h"
#include "loader.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_string.h"
#include "pmfx.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"

using namespace pen;
using namespace put;

namespace
{
    void*   user_setup(void* params);
    loop_t  user_update();
    void    user_shutdown();
}

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "shader_toy";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    const c8* sampler_types[] = {
        "linear_clamp",
        "linear_wrap",
        "point_clamp",
        "point_wrap"
    };
    
    struct vertex
    {
        float x, y, z, w;
    };

    struct textured_vertex
    {
        float x, y, z, w;
        float u, v;
    };

    struct tweakable_cb
    {
        f32 size_x = 0.0f;
        f32 size_y = 0.0f;
        f32 time = 0.0f;
        f32 pad = 0.0f;

        vec4f test;
    };

    struct cbuf_test
    {
        float        foff[4][4];
        tweakable_cb cb;
    };
    
    struct texture_sampler_mapping
    {
        u32 texture = 0;
        s32 sampler_choice = 0;
        u32 sampler = 0;
    };

    struct render_handles
    {
        // states
        u32 clear_state;
        u32 raster_state;
        u32 ds_state;

        // buffers
        u32 vb;
        u32 ib;

        // samplers
        u32 sampler_linear_clamp;
        u32 sampler_linear_wrap;
        u32 sampler_point_clamp;
        u32 sampler_point_wrap;

        // cbuffers
        u32 view_cbuffer;
        u32 tweakable_cbuffer;

        pen::viewport vp;
        pen::rect     r;

        void release()
        {
            pen::renderer_release_clear_state(clear_state);
            pen::renderer_release_raster_state(raster_state);
            pen::renderer_release_depth_stencil_state(ds_state);
            pen::renderer_release_buffer(vb);
            pen::renderer_release_buffer(ib);
            pen::renderer_release_buffer(view_cbuffer);
            pen::renderer_release_buffer(tweakable_cbuffer);
            pen::renderer_release_sampler(sampler_linear_clamp);
            pen::renderer_release_sampler(sampler_linear_wrap);
            pen::renderer_release_sampler(sampler_point_clamp);
            pen::renderer_release_sampler(sampler_point_wrap);
        }
    };
    
    
    pen::job_thread_params* s_job_params = nullptr;
    pen::job*               s_thread_info = nullptr;
    render_handles          s_render_handles;
    tweakable_cb            s_tweakables;
    texture_sampler_mapping s_tex_samplers[4] = {};
    u32                     s_sampler_states[4] = {0};
    u32                     s_shader_toy_pmfx = 0;
    
    void init_renderer()
    {
        s_shader_toy_pmfx = pmfx::load_shader("shader_toy");
        
        // clear state
        static pen::clear_state cs = {
            0.5f, 0.5, 0.5f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
        };

        s_render_handles.clear_state = pen::renderer_create_clear_state(cs);

        // raster state
        pen::rasteriser_state_creation_params rcp;
        pen::memory_zero(&rcp, sizeof(pen::rasteriser_state_creation_params));
        rcp.fill_mode = PEN_FILL_SOLID;
        rcp.cull_mode = PEN_CULL_NONE;
        rcp.depth_bias_clamp = 0.0f;
        rcp.sloped_scale_depth_bias = 0.0f;

        s_render_handles.raster_state = pen::renderer_create_rasterizer_state(rcp);

        // buffers
        // create vertex buffer for a quad
        textured_vertex quad_vertices[] = {
            0.0f, 0.0f, 0.5f, 1.0f, // p1
            0.0f, 0.0f,             // uv1

            0.0f, 1.0f, 0.5f, 1.0f, // p2
            0.0f, 1.0f,             // uv2

            1.0f, 1.0f, 0.5f, 1.0f, // p3
            1.0f, 1.0f,             // uv3

            1.0f, 0.0f, 0.5f, 1.0f, // p4
            1.0f, 0.0f,             // uv4
        };

        pen::buffer_creation_params bcp;
        bcp.usage_flags = PEN_USAGE_DEFAULT;
        bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
        bcp.cpu_access_flags = 0;

        bcp.buffer_size = sizeof(textured_vertex) * 4;
        bcp.data = (void*)&quad_vertices[0];

        s_render_handles.vb = pen::renderer_create_buffer(bcp);

        // create index buffer
        u16 indices[] = {0, 1, 2, 2, 3, 0};

        bcp.usage_flags = PEN_USAGE_IMMUTABLE;
        bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
        bcp.cpu_access_flags = 0;
        bcp.buffer_size = sizeof(u16) * 6;
        bcp.data = (void*)&indices[0];

        s_render_handles.ib = pen::renderer_create_buffer(bcp);

        // sampler states
        // create a sampler object so we can sample a texture
        pen::sampler_creation_params scp;
        pen::memory_zero(&scp, sizeof(pen::sampler_creation_params));
        scp.filter = PEN_FILTER_MIN_MAG_MIP_LINEAR;
        scp.address_u = PEN_TEXTURE_ADDRESS_CLAMP;
        scp.address_v = PEN_TEXTURE_ADDRESS_CLAMP;
        scp.address_w = PEN_TEXTURE_ADDRESS_CLAMP;
        scp.comparison_func = PEN_COMPARISON_ALWAYS;
        scp.min_lod = 0.0f;
        scp.max_lod = 4.0f;

        s_render_handles.sampler_linear_clamp = pen::renderer_create_sampler(scp);

        scp.filter = PEN_FILTER_MIN_MAG_MIP_POINT;
        s_render_handles.sampler_point_clamp = pen::renderer_create_sampler(scp);

        scp.address_u = PEN_TEXTURE_ADDRESS_WRAP;
        scp.address_v = PEN_TEXTURE_ADDRESS_WRAP;
        scp.address_w = PEN_TEXTURE_ADDRESS_WRAP;
        s_render_handles.sampler_point_wrap = pen::renderer_create_sampler(scp);

        scp.filter = PEN_FILTER_MIN_MAG_MIP_LINEAR;
        s_render_handles.sampler_linear_wrap = pen::renderer_create_sampler(scp);

        // depth stencil state
        pen::depth_stencil_creation_params depth_stencil_params = {0};

        // Depth test parameters
        depth_stencil_params.depth_enable = false;
        depth_stencil_params.depth_write_mask = 1;
        depth_stencil_params.depth_func = PEN_COMPARISON_ALWAYS;
        depth_stencil_params.stencil_enable = false;

        s_render_handles.ds_state = pen::renderer_create_depth_stencil_state(depth_stencil_params);

        // constant buffer
        bcp.usage_flags = PEN_USAGE_DYNAMIC;
        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
        bcp.buffer_size = sizeof(float) * 32;
        bcp.data = (void*)nullptr;

        s_render_handles.view_cbuffer = pen::renderer_create_buffer(bcp);

        bcp.buffer_size = sizeof(tweakable_cb);
        s_render_handles.tweakable_cbuffer = pen::renderer_create_buffer(bcp);
    }
    
    void show_ui()
    {
        s_sampler_states[0] = s_render_handles.sampler_linear_clamp;
        s_sampler_states[1] = s_render_handles.sampler_linear_wrap;
        s_sampler_states[2] = s_render_handles.sampler_point_clamp;
        s_sampler_states[3] = s_render_handles.sampler_point_wrap;

        put::dev_ui::new_frame();

        bool open = true;
        ImGui::Begin("Shader Toy", &open);

        if (s_tex_samplers[0].texture == 0)
            s_tex_samplers[0].texture = put::load_texture("data/textures/01.dds");

        static bool browser_open = false;
        static s32  browser_slot = -1;
        for (s32 i = 0; i < 4; ++i)
        {
            ImGui::PushID(i);

            ImGui::Combo("Sampler", &s_tex_samplers[i].sampler_choice, (const c8**)sampler_types, 4);
            s_tex_samplers[i].sampler = s_sampler_states[s_tex_samplers[i].sampler_choice];

            if (s_tex_samplers[i].texture != 0)
                ImGui::Image(IMG(s_tex_samplers[i].texture), ImVec2(128, 128));

            if (ImGui::Button("Load Image"))
            {
                browser_slot = i;
                browser_open = true;
            }

            ImGui::PopID();
        }

        if (browser_open)
        {
            const char* fn = put::dev_ui::file_browser(browser_open, dev_ui::e_file_browser_flags::open);

            if (fn && browser_slot >= 0)
            {
                s_tex_samplers[browser_slot].texture = put::load_texture(fn);
                browser_slot = -1;
            }
        }

        ImGui::End();
    }
    
    void* user_setup(void* params)
    {
        // unpack the params passed to the thread and signal to the engine it ok to proceed
        s_job_params = (pen::job_thread_params*)params;
        s_thread_info = s_job_params->job_info;
        pen::semaphore_post(s_thread_info->p_sem_continue, 1);
        
        init_renderer();

        put::dev_ui::init();
        put::init_hot_loader();
		
        pen_main_loop(user_update);
        return PEN_THREAD_OK;
    }

    void user_shutdown()
    {
        put::dev_ui::shutdown();
        
    	pen::renderer_new_frame();
     
        pmfx::release_shader(s_shader_toy_pmfx);
        s_render_handles.release();
    	
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();
        
        pen::semaphore_post(s_thread_info->p_sem_terminated, 1);
    }

    loop_t user_update()
    {
        pen::renderer_new_frame();

        show_ui();

        pen::renderer_set_rasterizer_state(s_render_handles.raster_state);

        s_render_handles.vp.x = 0;
        s_render_handles.vp.y = 0;
        s_render_handles.vp.width = PEN_BACK_BUFFER_RATIO;
        s_render_handles.vp.height = 1.0f;
        s_render_handles.vp.min_depth = 0.0f;
        s_render_handles.vp.max_depth = 1.0f;

        // update cbuffers
        // view
        s32 w, h;
        pen::window_get_size(w, h);

        float L = 0.0f;
        float R = w;
        float B = h;
        float T = 0.0f;

        float mvp[4][4] = {
            {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
            {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
            {0.0f, 0.0f, 0.5f, 0.0f},
            {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
        };

        pen::renderer_update_buffer(s_render_handles.view_cbuffer, &mvp, sizeof(mvp), 0);

        // tweakbles
        s_tweakables.size_x = w;
        s_tweakables.size_y = h;
        s_tweakables.time = pen::get_time_ms();

        pen::renderer_update_buffer(s_render_handles.tweakable_cbuffer, &s_tweakables, sizeof(tweakable_cb), 0);

        // bind back buffer and clear
        pen::renderer_set_viewport(s_render_handles.vp);
        pen::renderer_set_scissor_rect(
            rect{s_render_handles.vp.x, s_render_handles.vp.y, s_render_handles.vp.width, s_render_handles.vp.height});
        pen::renderer_set_depth_stencil_state(s_render_handles.ds_state);
        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::renderer_clear(s_render_handles.clear_state);

        // draw quad
        {
            // bind shaders
            pmfx::set_technique(s_shader_toy_pmfx, 0);

            // bind vertex buffer
            pen::renderer_set_vertex_buffer(s_render_handles.vb, 0, sizeof(textured_vertex), 0);
            pen::renderer_set_index_buffer(s_render_handles.ib, PEN_FORMAT_R16_UINT, 0);

            // bind textures and samplers
            for (s32 i = 0; i < 4; ++i)
            {
                pen::renderer_set_texture(s_tex_samplers[i].texture, s_tex_samplers[i].sampler, i, pen::TEXTURE_BIND_PS);
            }

            // bind cbuffers
            pen::renderer_set_constant_buffer(s_render_handles.view_cbuffer, 0, pen::CBUFFER_BIND_VS);
            pen::renderer_set_constant_buffer(s_render_handles.tweakable_cbuffer, 1, pen::CBUFFER_BIND_VS);

            // draw
            pen::renderer_draw_indexed(6, 0, 0, PEN_PT_TRIANGLELIST);
        }

        // present
        put::dev_ui::render();

        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        pmfx::poll_for_changes();
        put::poll_hot_loader();
        
        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(s_thread_info->p_sem_exit))
        {
            user_shutdown();
            pen_main_loop_exit();
        }
        
        pen_main_loop_continue();
    }
}
