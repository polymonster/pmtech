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
        p.window_title = "buffer_multi_update";
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

struct draw_call
{
    float x, y, z, w;
    float r, g, b, a;
};

void* pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::semaphore_post(p_thread_info->p_sem_continue, 1);

    static pen::clear_state cs = {
        0.0f, 0.5f, 0.5f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
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

    // load shaders now requiring dependency on pmfx to make loading simpler.
    u32 textured_shader = pmfx::load_shader("buffer_multi_update");

    // manually scale 16:9 to 1:1
    f32 x_size = 0.5f / pen::window_get_aspect();

    // create vertex buffer for a quad
    vertex quad_vertices[] = {
        -x_size, -0.5f, 0.5f, 1.0f, // p1
        -x_size, 0.5f,  0.5f, 1.0f, // p
        x_size,  0.5f,  0.5f, 1.0f, // p3
        x_size,  -0.5f, 0.5f, 1.0f  // p4
    };

    pen::buffer_creation_params bcp;
    bcp.usage_flags = PEN_USAGE_DEFAULT;
    bcp.bind_flags = PEN_BIND_VERTEX_BUFFER;
    bcp.cpu_access_flags = 0;

    bcp.buffer_size = sizeof(vertex) * 4;
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

    // cbuffer
    bcp.usage_flags = PEN_USAGE_DYNAMIC;
    bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
    bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
    bcp.buffer_size = sizeof(draw_call);
    bcp.data = nullptr;

    u32 cbuffer_draw = pen::renderer_create_buffer(bcp);

    f32 offsetx = 0.3f * x_size;
    f32 offsety = 0.3f;

    // multiple draw call info
    draw_call draw_calls[] = {{-offsetx, -offsety, 0.0f, 1.0f, 0.5f, 0.8f, 0.0f, 1.0f},
                              {offsetx, -offsety, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f, 1.0f},
                              {offsetx, offsety, 0.0f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f},
                              {-offsetx, offsety, 0.0f, 1.0f, 0.5f, 0.0f, 0.5f, 1.0f}};

    while (1)
    {
        pen::renderer_new_frame();

        pen::renderer_set_rasterizer_state(raster_state);

        // bind back buffer and clear
        pen::viewport vp = {0.0f, 0.0f, PEN_BACK_BUFFER_RATIO, 1.0f, 0.0f, 1.0f};
        pen::renderer_set_viewport(vp);
        pen::renderer_set_scissor_rect(rect{vp.x, vp.y, vp.width, vp.height});

        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::renderer_clear(clear_state);

        // draw quads
        {
            for (u32 i = 0; i < 4; ++i)
            {
                // update cbuffer.. use single buffer and update multiple times.
                pen::renderer_update_buffer(cbuffer_draw, &draw_calls[i], sizeof(draw_call));

                // bind vertex layout and shaders
                pmfx::set_technique(textured_shader, 0);

                // bind vertex buffer
                u32 stride = sizeof(vertex);
                pen::renderer_set_vertex_buffer(quad_vertex_buffer, 0, stride, 0);
                pen::renderer_set_index_buffer(quad_index_buffer, PEN_FORMAT_R16_UINT, 0);

                // bind cuffer
                pen::renderer_set_constant_buffer(cbuffer_draw, 0, pen::CBUFFER_BIND_VS);

                // draw
                pen::renderer_draw_indexed(6, 0, 0, PEN_PT_TRIANGLELIST);
            }
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
    pmfx::release_shader(textured_shader);

    pen::renderer_release_buffer(quad_vertex_buffer);
    pen::renderer_release_buffer(quad_index_buffer);
    pen::renderer_release_buffer(cbuffer_draw);
    pen::renderer_consume_cmd_buffer();

    // signal to the engine the thread has finished
    pen::semaphore_post(p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}
