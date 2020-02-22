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

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "texture_array";
        p.window_sample_count = 4;
        p.user_thread_function = user_entry;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

struct vertex
{
    float x, y, z, w;
};

struct textured_vertex
{
    float x, y, z, w;
    float u, v;
};

void* pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::semaphore_post(p_thread_info->p_sem_continue, 1);

    static pen::clear_state cs = {
        133.0f / 255.0f, 179.0f / 255.0f, 178.0f / 255.0f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };

    u32 clear_state = pen::renderer_create_clear_state(cs);

    // raster state
    pen::rasteriser_state_creation_params rcp;
    pen::memory_zero(&rcp, sizeof(rasteriser_state_creation_params));
    rcp.fill_mode = PEN_FILL_SOLID;
    rcp.cull_mode = PEN_CULL_NONE;
    rcp.depth_bias_clamp = 0.0f;
    rcp.sloped_scale_depth_bias = 0.0f;

    u32 raster_state = pen::renderer_create_rasterizer_state(rcp);

    // viewport
    pen::viewport vp = {0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};

    // load shaders now requiring dependency on pmfx to make loading simpler.
    u32 texture_array_shader = pmfx::load_shader("texture_array");

    // load texture array
    u32 texture_array = put::load_texture("data/textures/zombie.dds");

    // get frame / slice count from texture info
    texture_info ti;
    put::get_texture_info(texture_array, ti);
    f32 num_frames = ti.num_arrays;

    // manually scale 16:9 to 1:1
    f32 x_size = 0.5f / pen::window_get_aspect();

    f32 uv_y_a = 1.0f;
    f32 uv_y_b = 0.0f;

    // create vertex buffer for a quad
    textured_vertex quad_vertices[] = {
        -x_size, -0.5f,  0.5f, 1.0f, // p1
        0.0f,    uv_y_a,             // uv1

        -x_size, 0.5f,   0.5f, 1.0f, // p2
        0.0f,    uv_y_b,             // uv2

        x_size,  0.5f,   0.5f, 1.0f, // p3
        1.0f,    uv_y_b,             // uv3

        x_size,  -0.5f,  0.5f, 1.0f, // p4
        1.0f,    uv_y_a,             // uv4
    };

    pen::buffer_creation_params bcp;
    bcp.usage_flags = PEN_USAGE_DEFAULT;
    bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
    bcp.cpu_access_flags = 0;

    bcp.buffer_size = sizeof(textured_vertex) * 4;
    bcp.data = (void*)&quad_vertices[0];

    u32 quad_vertex_buffer = pen::renderer_create_buffer(bcp);

    // create index buffer
    u16 indices[] = {0, 1, 2, 2, 3, 0};

    bcp.usage_flags = PEN_USAGE_IMMUTABLE;
    bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
    bcp.cpu_access_flags = 0;
    bcp.buffer_size = sizeof(u16) * 6;
    bcp.data = (void*)&indices[0];

    u32 quad_index_buffer = pen::renderer_create_buffer(bcp);

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

    u32 linear_sampler = pen::renderer_create_sampler(scp);

    // cbuffer to set the array slice
    f32 time[4] = {0};
    bcp.usage_flags = PEN_USAGE_DYNAMIC;
    bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
    bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
    bcp.buffer_size = sizeof(time);
    bcp.data = &time[0];

    u32 cbuffer = pen::renderer_create_buffer(bcp);

    f32         ft = 0.0f;
    pen::timer* t = pen::timer_create();
    pen::timer_start(t);

    while (1)
    {
        pen::renderer_set_rasterizer_state(raster_state);

        ft += pen::timer_elapsed_ms(t);
        pen::timer_start(t);

        f32 ms_frame = 100.0f;
        if (ft > ms_frame)
        {
            time[0] += 1.0f;
            if (time[0] >= num_frames)
                time[0] = 0.0f;

            ft -= ms_frame;
        }

        pen::renderer_update_buffer(cbuffer, (void*)&time[0], sizeof(time));

        // bind back buffer and clear
        pen::renderer_set_viewport(vp);
        pen::renderer_set_scissor_rect(rect{vp.x, vp.y, vp.width, vp.height});

        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::renderer_clear(clear_state);

        // draw quad
        {
            // bind vertex layout and shaders
            pmfx::set_technique(texture_array_shader, 0);

            // bind buffers
            pen::renderer_set_vertex_buffer(quad_vertex_buffer, 0, sizeof(textured_vertex), 0);
            pen::renderer_set_index_buffer(quad_index_buffer, PEN_FORMAT_R16_UINT, 0);
            pen::renderer_set_constant_buffer(cbuffer, 0, pen::CBUFFER_BIND_PS);

            // bind render target as texture on sampler 0
            pen::renderer_set_texture(texture_array, linear_sampler, 0, pen::TEXTURE_BIND_PS);

            // draw
            pen::renderer_draw_indexed(6, 0, 0, PEN_PT_TRIANGLELIST);
        }

        // present
        pen::renderer_present();

        // for unit test
        pen::renderer_test_run();

        pen::renderer_consume_cmd_buffer();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(p_thread_info->p_sem_exit))
        {
            break;
        }
    }

    // clean up mem here
    pmfx::release_shader(texture_array_shader);

    pen::renderer_release_buffer(quad_vertex_buffer);
    pen::renderer_release_buffer(quad_index_buffer);
    pen::renderer_release_sampler(linear_sampler);
    pen::renderer_release_texture(texture_array);
    pen::renderer_consume_cmd_buffer();

    // signal to the engine the thread has finished
    pen::semaphore_post(p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}
