#include "console.h"
#include "pen.h"
#include "renderer.h"
#include "threads.h"

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
        p.window_title = "clear";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::renderer;
        return p;
    }
} // namespace pen

namespace
{
    pen::job_thread_params* job_params;
    pen::job*               p_thread_info;

    u32 s_clear_state;

    void* user_setup(void* params)
    {
        PEN_LOG("User Setup");

        // unpack the params passed to the thread and signal to the engine it ok to proceed
        job_params = (pen::job_thread_params*)params;
        p_thread_info = job_params->job_info;
        pen::semaphore_post(p_thread_info->p_sem_continue, 1);

        // create clear state
        static pen::clear_state cs = {
            0.0f, 0.3f, 0.2f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER,
        };

        s_clear_state = pen::renderer_create_clear_state(cs);

        pen_main_loop(user_update);
        return PEN_THREAD_OK;
    }

    void user_shutdown()
    {
        PEN_LOG("User Shutdown");

        pen::renderer_new_frame();
        pen::renderer_release_clear_state(s_clear_state);
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        pen::semaphore_post(p_thread_info->p_sem_terminated, 1);
    }

    loop_t user_update()
    {
        // start new frame
        pen::renderer_new_frame();

        // set render targets to backbuffer
        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);

        // clear
        pen::renderer_clear(s_clear_state);

        // present
        pen::renderer_present();
        pen::renderer_consume_cmd_buffer();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(p_thread_info->p_sem_exit))
        {
            user_shutdown();
            pen_main_loop_exit();
        }

        pen_main_loop_continue();
    }
} // namespace
