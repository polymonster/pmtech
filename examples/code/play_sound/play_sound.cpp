#include "audio/audio.h"
#include "debug_render.h"
#include "loader.h"

#include "file_system.h"
#include "memory.h"
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
        p.window_title = "play_sound";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    pen::job_thread_params* s_job_params = nullptr;
    pen::job*               s_thread_info = nullptr;
    u32                     s_clear_state_grey = 0;
    u32                     s_raster_state_cull_back = 0;
    u32                     s_cb_2d_view = 0;
    u32                     s_sound_index = 0;
    u32                     s_channel_index = 0;
    u32                     s_group_index = 0;

    void* user_setup(void* params)
    {
        // unpack the params passed to the thread and signal to the engine it ok to proceed
        s_job_params = (pen::job_thread_params*)params;
        s_thread_info = s_job_params->job_info;
        pen::semaphore_post(s_thread_info->p_sem_continue, 1);

        pen::jobs_create_job(put::audio_thread_function, 1024 * 10, nullptr, pen::e_thread_start_flags::detached);

        // initialise the debug render system
        put::dbg::init();

        // create 2 clear states one for the render target and one for the main screen, so we can see the difference
        static pen::clear_state cs = {
            0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
        };

        s_clear_state_grey = pen::renderer_create_clear_state(cs);

        // raster state
        pen::rasteriser_state_creation_params rcp;
        pen::memory_zero(&rcp, sizeof(pen::rasteriser_state_creation_params));
        rcp.fill_mode = PEN_FILL_SOLID;
        rcp.cull_mode = PEN_CULL_BACK;
        rcp.depth_bias_clamp = 0.0f;
        rcp.sloped_scale_depth_bias = 0.0f;
        rcp.depth_clip_enable = true;

        s_raster_state_cull_back = pen::renderer_create_rasterizer_state(rcp);

        s_sound_index = put::audio_create_sound("data/audio/singing.wav");
        s_channel_index = put::audio_create_channel_for_sound(s_sound_index);
        s_group_index = put::audio_create_channel_group();

        put::audio_add_channel_to_group(s_channel_index, s_group_index);
        put::audio_group_set_volume(s_group_index, 1.0f);

        // cb
        pen::buffer_creation_params bcp;
        bcp.usage_flags = PEN_USAGE_DYNAMIC;
        bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
        bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
        bcp.buffer_size = sizeof(float) * 20; // 4x4 matrix + 4 floats user_data
        bcp.data = (void*)nullptr;

        s_cb_2d_view = pen::renderer_create_buffer(bcp);

        pen_main_loop(user_update);
        return PEN_THREAD_OK;
    }

    void user_shutdown()
    {
        // release render resources
        pen::renderer_new_frame();
        put::dbg::shutdown();
        pen::renderer_release_buffer(s_cb_2d_view);
        pen::renderer_release_raster_state(s_raster_state_cull_back);
        pen::renderer_release_clear_state(s_clear_state_grey);
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        // release audio resources
        //put::audio_release_resource(s_sound_index);
        //put::audio_release_resource(s_channel_index);
        //put::audio_release_resource(s_group_index);
        //put::audio_consume_command_buffer();

        pen::semaphore_post(s_thread_info->p_sem_terminated, 1);
    }

    loop_t user_update()
    {
        pen::renderer_new_frame();

        s32 w, h;
        pen::window_get_size(w, h);

        pen::viewport vp = {0.0f, 0.0f, PEN_BACK_BUFFER_RATIO, 1.0f, 0.0f, 1.0f};
        pen::viewport dbg_vp = {0.0f, 0.0f, (f32)w, (f32)h, 0.0f, 1.0f};

        // create 2d view proj matrix
        f32 mvp[4][4] = {
            {(f32)2.0f / w, 0.0, 0.0, 0.0}, {0.0, (f32)2.0f / h, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {-1.0, -1.0, 0.0, 1.0}};
        pen::renderer_update_buffer(s_cb_2d_view, mvp, sizeof(mvp), 0);

        // bind back buffer and clear
        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::renderer_set_viewport(vp);
        pen::renderer_set_scissor_rect(rect{vp.x, vp.y, vp.width, vp.height});

        pen::renderer_set_rasterizer_state(s_raster_state_cull_back);
        pen::renderer_clear(s_clear_state_grey);

        put::dbg::add_text_2f(10.0f, 10.0f, dbg_vp, vec4f(0.0f, 1.0f, 0.0f, 1.0f), "%s",
                              "Play Sound: data/audio/singing.wav");
        put::dbg::render_2d(s_cb_2d_view);

        // present renderer
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        // present audio
        put::audio_consume_command_buffer();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(s_thread_info->p_sem_exit))
        {
            user_shutdown();
            pen_main_loop_exit();
        }

        pen_main_loop_continue();
    }
} // namespace
