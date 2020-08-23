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

using namespace put;
using namespace pen;

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
        p.window_title = "depth_texture";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    struct vertex
    {
        float x, y, z, w;
    };

    struct textured_vertex
    {
        float x, y, z, w;
        float u, v;
    };

    pen::job_thread_params* job_params = nullptr;
    pen::job*               p_thread_info = nullptr;
    
    u32 s_clear_state;
    u32 s_clear_state_rt;
    u32 s_raster_state;
    u32 s_textured_shader;
    u32 s_quad_vertex_buffer;
    u32 s_quad_index_buffer;
    u32 s_colour_render_target;
    u32 s_basic_tri_shader;
    u32 s_depth_stencil_state;
    u32 s_triangle_vertex_buffer;
    u32 s_render_target_texture_sampler;
    pen::viewport s_vp_rt;

    void* user_setup(void* params)
    {
        // unpack the params passed to the thread and signal to the engine it ok to proceed
        job_params = (pen::job_thread_params*)params;
        p_thread_info = job_params->job_info;
        pen::semaphore_post(p_thread_info->p_sem_continue, 1);

        // create 2 clear states one for the depth target and one for the main screen, so we can see the difference
        static pen::clear_state cs = {
            0.5f, 0.0, 0.5f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
        };

        s_clear_state = pen::renderer_create_clear_state(cs);

        static pen::clear_state cs_rt = {
            0.0f, 0.0, 0.5f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
        };

        s_clear_state_rt = pen::renderer_create_clear_state(cs_rt);

        // raster state
        pen::rasteriser_state_creation_params rcp;
        pen::memory_zero(&rcp, sizeof(rasteriser_state_creation_params));
        rcp.fill_mode = PEN_FILL_SOLID;
        rcp.cull_mode = PEN_CULL_NONE;
        rcp.depth_bias_clamp = 0.0f;
        rcp.sloped_scale_depth_bias = 0.0f;

        s_raster_state = pen::renderer_create_rasterizer_state(rcp);

        // viewport for render target
        s_vp_rt = {0.0f, 0.0f, 1024.0f, 512.0f, 0.0f, 1.0f};

        // create render target
        pen::texture_creation_params tcp = {0};
        tcp.width = (u32)s_vp_rt.width;
        tcp.height = (u32)s_vp_rt.height;
        tcp.cpu_access_flags = 0;
        tcp.format = PEN_TEX_FORMAT_RGBA8_UNORM;
        tcp.num_arrays = 1;
        tcp.num_mips = 1;
        tcp.bind_flags = PEN_BIND_RENDER_TARGET | PEN_BIND_SHADER_RESOURCE;
        tcp.pixels_per_block = 1;
        tcp.sample_count = 1;
        tcp.sample_quality = 0;
        tcp.block_size = 32;
        tcp.usage = PEN_USAGE_DEFAULT;
        tcp.flags = 0;

        s_colour_render_target = pen::renderer_create_render_target(tcp);

        // load shaders now requiring dependency on pmfx to make loading simpler.
        s_basic_tri_shader = pmfx::load_shader("basictri");
        s_textured_shader = pmfx::load_shader("textured");

        // create vertex buffer for a triangle
        vertex triangle_vertices[] = {
            -0.5f, -0.5f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f, 1.0f, 0.5f, -0.5f, 0.5f, 1.0f,
        };

        pen::buffer_creation_params bcp;
        bcp.usage_flags = PEN_USAGE_DEFAULT;
        bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
        bcp.cpu_access_flags = 0;

        bcp.buffer_size = sizeof(vertex) * 3;
        bcp.data = (void*)&triangle_vertices[0];

        s_triangle_vertex_buffer = pen::renderer_create_buffer(bcp);

        f32 uv_y_a = 1.0f;
        f32 uv_y_b = 0.0f;

        // create vertex buffer for a quad
        textured_vertex quad_vertices[] = {
            -0.5f, -0.5f,  0.5f, 1.0f, // p1
            0.0f,  uv_y_a,             // uv1

            -0.5f, 0.5f,   0.5f, 1.0f, // p2
            0.0f,  uv_y_b,             // uv2

            0.5f,  0.5f,   0.5f, 1.0f, // p3
            1.0f,  uv_y_b,             // uv3

            0.5f,  -0.5f,  0.5f, 1.0f, // p4
            1.0f,  uv_y_a,             // uv4
        };
        bcp.buffer_size = sizeof(textured_vertex) * 4;
        bcp.data = (void*)&quad_vertices[0];

        s_quad_vertex_buffer = pen::renderer_create_buffer(bcp);

        // create index buffer
        u16 indices[] = {0, 1, 2, 2, 3, 0};

        bcp.usage_flags = PEN_USAGE_DEFAULT;
        bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
        bcp.cpu_access_flags = 0;
        bcp.buffer_size = sizeof(u16) * 6;
        bcp.data = (void*)&indices[0];

        s_quad_index_buffer = pen::renderer_create_buffer(bcp);

        // create a sampler object so we can sample a texture
        pen::sampler_creation_params scp;
        pen::memory_zero(&scp, sizeof(pen::sampler_creation_params));
        scp.filter = PEN_FILTER_MIN_MAG_MIP_LINEAR;
        scp.address_u = PEN_TEXTURE_ADDRESS_CLAMP;
        scp.address_v = PEN_TEXTURE_ADDRESS_CLAMP;
        scp.address_w = PEN_TEXTURE_ADDRESS_CLAMP;
        scp.comparison_func = PEN_COMPARISON_DISABLED;
        scp.min_lod = 0.0f;
        scp.max_lod = 4.0f;

        pen::depth_stencil_creation_params depth_stencil_params = {0};

        depth_stencil_params.depth_enable = true;
        depth_stencil_params.depth_write_mask = 1;
        depth_stencil_params.depth_func = PEN_COMPARISON_ALWAYS;

        s_depth_stencil_state = pen::renderer_create_depth_stencil_state(depth_stencil_params);

        s_render_target_texture_sampler = pen::renderer_create_sampler(scp);

        pen_main_loop(user_update);
        return PEN_THREAD_OK;
    }
    
    void user_shutdown()
    {
        // clean up mem
        pen::renderer_new_frame();
        pen::renderer_release_clear_state(s_clear_state);
        pen::renderer_release_clear_state(s_clear_state_rt);
        pen::renderer_release_depth_stencil_state(s_depth_stencil_state);
        pen::renderer_release_raster_state(s_raster_state);
        pen::renderer_release_buffer(s_triangle_vertex_buffer);
        pen::renderer_release_buffer(s_quad_vertex_buffer);
        pen::renderer_release_buffer(s_quad_index_buffer);
        pen::renderer_release_render_target(s_colour_render_target);
        pen::renderer_release_depth_stencil_state(s_depth_stencil_state);
        pmfx::release_shader(s_textured_shader);
        pmfx::release_shader(s_basic_tri_shader);
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        // signal to the engine the thread has finished
        pen::semaphore_post(p_thread_info->p_sem_terminated, 1);
    }

    loop_t user_update()
    {
        pen::renderer_new_frame();

        // bind render target and draw basic triangle
        pen::renderer_set_rasterizer_state(s_raster_state);

        // bind and clear render target
        pen::renderer_set_targets(s_colour_render_target, PEN_NULL_DEPTH_BUFFER);

        pen::viewport vp = {0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};

        pen::renderer_set_viewport(s_vp_rt);
        pen::renderer_set_scissor_rect(rect{s_vp_rt.x, s_vp_rt.y, s_vp_rt.width, s_vp_rt.height});
        pen::renderer_clear(s_clear_state_rt);

        pen::renderer_set_depth_stencil_state(s_depth_stencil_state);

        // draw tri into the render target
        if (1)
        {
            // bind shader / input layout
            pmfx::set_technique(s_basic_tri_shader, 0);

            // bind vertex buffer
            pen::renderer_set_vertex_buffer(s_triangle_vertex_buffer, 0, sizeof(vertex), 0);

            // draw
            pen::renderer_draw(3, 0, PEN_PT_TRIANGLELIST);
        }

        // bind back buffer and clear
        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::renderer_set_viewport(vp);
        pen::renderer_set_scissor_rect(rect{vp.x, vp.y, vp.width, vp.height});
        pen::renderer_clear(s_clear_state);

        // draw quad
        {
            // bind shader / input layout
            pmfx::set_technique(s_textured_shader, 0);

            // set vertex buffer
            pen::renderer_set_vertex_buffer(s_quad_vertex_buffer, 0, sizeof(textured_vertex), 0);

            pen::renderer_set_index_buffer(s_quad_index_buffer, PEN_FORMAT_R16_UINT, 0);

            // bind render target as texture on sampler 0
            pen::renderer_set_texture(s_colour_render_target, s_render_target_texture_sampler, 0, pen::TEXTURE_BIND_PS);

            // draw
            pen::renderer_draw_indexed(6, 0, 0, PEN_PT_TRIANGLELIST);

            // unbind render target from the sampler
            pen::renderer_set_texture(0, s_render_target_texture_sampler, 0, pen::TEXTURE_BIND_PS);
        }

        // present
        pen::renderer_present();

        // for unit test
        pen::renderer_test_run();

        pen::renderer_consume_cmd_buffer();
            
        if (pen::semaphore_try_wait(p_thread_info->p_sem_exit))
        {
            user_shutdown();
            pen_main_loop_exit();
        }
               
        pen_main_loop_continue();
    }
}
