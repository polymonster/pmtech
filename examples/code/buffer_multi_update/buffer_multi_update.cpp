#include "loader.h"
#include "pmfx.h"

#include "file_system.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_string.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"

using namespace pen;
using namespace put;

namespace
{
    void*  user_setup(void* params);
    loop_t user_update();
    void   user_shutdown();
} // namespace

namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "buffer_multi_update";
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
        f32 x, y, z, w;
    };

    struct draw_call
    {
        f32 x, y, z, w;
        f32 r, g, b, a;
    };

    job_thread_params* s_job_params;
    job*               s_thread_info;
    u32                s_clear_state = 0;
    u32                s_raster_state = 0;
    u32                s_textured_shader = 0;
    u32                s_quad_vertex_buffer = 0;
    u32                s_quad_index_buffer = 0;
    u32                s_cbuffer_draw = 0;
    draw_call          s_draw_calls[4];

    void* user_setup(void* params)
    {
        // unpack the params passed to the thread and signal to the engine it ok to proceed
        s_job_params = (pen::job_thread_params*)params;
        s_thread_info = s_job_params->job_info;
        pen::semaphore_post(s_thread_info->p_sem_continue, 1);

        static pen::clear_state cs = {
            0.0f, 0.5f, 0.5f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
        };

        s_clear_state = pen::renderer_create_clear_state(cs);

        // raster state
        pen::rasteriser_state_creation_params rcp;
        pen::memory_zero(&rcp, sizeof(rasteriser_state_creation_params));
        rcp.fill_mode = PEN_FILL_SOLID;
        rcp.cull_mode = PEN_CULL_NONE;
        rcp.depth_bias_clamp = 0.0f;
        rcp.sloped_scale_depth_bias = 0.0f;

        s_raster_state = pen::renderer_create_rasterizer_state(rcp);

        // load shaders now requiring dependency on pmfx to make loading simpler.
        s_textured_shader = pmfx::load_shader("buffer_multi_update");

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

        s_quad_vertex_buffer = pen::renderer_create_buffer(bcp);

        // create index buffer
        u16 indices[] = {0, 1, 2, 2, 3, 0};

        bcp.usage_flags = PEN_USAGE_IMMUTABLE;
        bcp.bind_flags = PEN_BIND_INDEX_BUFFER;
        bcp.cpu_access_flags = 0;
        bcp.buffer_size = sizeof(u16) * 6;
        bcp.data = (void*)&indices[0];

        s_quad_index_buffer = pen::renderer_create_buffer(bcp);

        // cbuffer
        bcp.usage_flags = PEN_USAGE_DYNAMIC;
        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
        bcp.buffer_size = sizeof(draw_call);
        bcp.data = nullptr;

        s_cbuffer_draw = pen::renderer_create_buffer(bcp);

        f32 offsetx = 0.3f * x_size;
        f32 offsety = 0.3f;

        // multiple draw call info
        s_draw_calls[0] = {-offsetx, -offsety, 0.0f, 1.0f, 0.5f, 0.8f, 0.0f, 1.0f};
        s_draw_calls[1] = {offsetx, -offsety, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f, 1.0f};
        s_draw_calls[2] = {offsetx, offsety, 0.0f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f};
        s_draw_calls[3] = {-offsetx, offsety, 0.0f, 1.0f, 0.5f, 0.0f, 0.5f, 1.0f};

        pen_main_loop(user_update);
        return PEN_THREAD_OK;
    }

    void user_shutdown()
    {
        pen::renderer_new_frame();
        pmfx::release_shader(s_textured_shader);
        pen::renderer_release_clear_state(s_clear_state);
        pen::renderer_release_raster_state(s_raster_state);
        pen::renderer_release_buffer(s_quad_vertex_buffer);
        pen::renderer_release_buffer(s_quad_index_buffer);
        pen::renderer_release_buffer(s_cbuffer_draw);
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        pen::semaphore_post(s_thread_info->p_sem_terminated, 1);
    }

    loop_t user_update()
    {
        pen::renderer_new_frame();

        pen::renderer_set_rasterizer_state(s_raster_state);

        // bind back buffer and clear
        pen::viewport vp = {0.0f, 0.0f, PEN_BACK_BUFFER_RATIO, 1.0f, 0.0f, 1.0f};
        pen::renderer_set_viewport(vp);
        pen::renderer_set_scissor_rect(rect{vp.x, vp.y, vp.width, vp.height});

        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::renderer_clear(s_clear_state);

        // draw quads
        {
            for (u32 i = 0; i < 4; ++i)
            {
                // update cbuffer.. use single buffer and update multiple times.
                pen::renderer_update_buffer(s_cbuffer_draw, &s_draw_calls[i], sizeof(draw_call));

                // bind vertex layout and shaders
                pmfx::set_technique(s_textured_shader, 0);

                // bind vertex buffer
                u32 stride = sizeof(vertex);
                pen::renderer_set_vertex_buffer(s_quad_vertex_buffer, 0, stride, 0);
                pen::renderer_set_index_buffer(s_quad_index_buffer, PEN_FORMAT_R16_UINT, 0);

                // bind cuffer
                pen::renderer_set_constant_buffer(s_cbuffer_draw, 0, pen::CBUFFER_BIND_VS);

                // draw
                pen::renderer_draw_indexed(6, 0, 0, PEN_PT_TRIANGLELIST);
            }
        }

        // present
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(s_thread_info->p_sem_exit))
        {
            user_shutdown();
            pen_main_loop_exit();
        }

        pen_main_loop_continue();
    }
} // namespace
