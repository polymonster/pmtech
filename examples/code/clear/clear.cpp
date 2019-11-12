#include "console.h"
#include "file_system.h"
#include "memory.h"
#include "os.h"
#include "pen.h"
#include "pen_string.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"

pen::window_creation_params pen_window{
    1280,            // width
    720,             // height
    4,               // MSAA samples
    "clear"          // window title / process name
};

bool test()
{
    return true;
}

PEN_TRV pen::user_entry(void* params)
{
    // unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job*               p_thread_info = job_params->job_info;
    pen::semaphore_post(p_thread_info->p_sem_continue, 1);

    // create clear state
    static pen::clear_state cs = {
        0.0f, 0.3f, 0.2f, 1.0f, 1.0f, 0x00, PEN_CLEAR_COLOUR_BUFFER,
    };

    u32 clear_state = pen::renderer_create_clear_state(cs);

    while (1)
    {
        // set render targets to backbuffer
        pen::renderer_set_targets(PEN_BACK_BUFFER_COLOUR, PEN_BACK_BUFFER_DEPTH);

        // clear
        pen::renderer_clear(clear_state);

        // present
        pen::renderer_present();

        // consume command buffer
        pen::renderer_consume_cmd_buffer();

        // for unit test
        pen::renderer_test_run();

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(p_thread_info->p_sem_exit))
        {
            break;
        }
    }

    // signal to the engine the thread has finished
    pen::semaphore_post(p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}
