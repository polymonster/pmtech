#include "audio.h"
#include "debug_render.h"
#include "file_system.h"
#include "loader.h"
#include "memory.h"
#include "pen.h"
#include "pen_string.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"

pen::window_creation_params pen_window{
    1280,        // width
    720,         // height
    4,           // MSAA samples
    "play_sound" // window title / process name
};

u32 clear_state_grey;
u32 raster_state_cull_back;

void renderer_state_init()
{
    // initialise the debug render system
    put::dbg::init();

    // create 2 clear states one for the render target and one for the main screen, so we can see the difference
    static pen::clear_state cs = {
        0.5f, 0.5f, 0.5f, 0.5f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };

    clear_state_grey = pen::renderer_create_clear_state(cs);

    // raster state
    pen::rasteriser_state_creation_params rcp;
    pen::memory_zero(&rcp, sizeof(pen::rasteriser_state_creation_params));
    rcp.fill_mode = PEN_FILL_SOLID;
    rcp.cull_mode = PEN_CULL_BACK;
    rcp.depth_bias_clamp = 0.0f;
    rcp.sloped_scale_depth_bias = 0.0f;
    rcp.depth_clip_enable = true;

    raster_state_cull_back = pen::renderer_create_rasterizer_state(rcp);
}

PEN_TRV pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::thread_semaphore_signal(p_thread_info->p_sem_continue, 1);

    pen::thread_create_job(put::audio_thread_function, 1024 * 10, nullptr, pen::THREAD_START_DETACHED);

    renderer_state_init();

    u32 sound_index = put::audio_create_sound("data/audio/singing.wav");
    u32 channel_index = put::audio_create_channel_for_sound(sound_index);
    u32 group_index = put::audio_create_channel_group();

    put::audio_add_channel_to_group(channel_index, group_index);

    put::audio_group_set_pitch(group_index, 0.5f);

    put::audio_group_set_volume(group_index, 1.0f);

    // cb
    pen::buffer_creation_params bcp;
    bcp.usage_flags = PEN_USAGE_DYNAMIC;
    bcp.bind_flags = PEN_BIND_CONSTANT_BUFFER;
    bcp.cpu_access_flags = PEN_CPU_ACCESS_WRITE;
    bcp.buffer_size = sizeof(float) * 16;
    bcp.data = (void*)nullptr;

    u32 cb_2d_view = pen::renderer_create_buffer(bcp);

    while (1)
    {
        pen::viewport vp = {0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};

        // create 2d view proj matrix
        float W = 2.0f / vp.width;
        float H = 2.0f / vp.height;
        float mvp[4][4] = {{W, 0.0, 0.0, 0.0}, {0.0, H, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {-1.0, -1.0, 0.0, 1.0}};
        pen::renderer_update_buffer(cb_2d_view, mvp, sizeof(mvp), 0);

        pen::renderer_set_rasterizer_state(raster_state_cull_back);

        // bind back buffer and clear
        pen::renderer_set_viewport(vp);
        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);
        pen::renderer_clear(clear_state_grey);

        put::dbg::add_text_2f(10.0f, 10.0f, vp, vec4f(0.0f, 1.0f, 0.0f, 1.0f), "%s", "Debug Text");

        put::dbg::render_2d(cb_2d_view);

        // present
        pen::renderer_present();

        pen::renderer_consume_cmd_buffer();

        put::audio_consume_command_buffer();

        // msg from the engine we want to terminate
        if (pen::thread_semaphore_try_wait(p_thread_info->p_sem_exit))
        {
            break;
        }
    }

    // clean up mem here
    put::dbg::shutdown();

    pen::renderer_release_buffer(cb_2d_view);
    pen::renderer_release_raster_state(raster_state_cull_back);
    pen::renderer_release_clear_state(clear_state_grey);
    pen::renderer_consume_cmd_buffer();

    put::audio_release_resource(sound_index);
    put::audio_release_resource(channel_index);
    put::audio_release_resource(group_index);
    put::audio_consume_command_buffer();

    // signal to the engine the thread has finished
    pen::thread_semaphore_signal(p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}
