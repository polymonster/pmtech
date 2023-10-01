#include "console.h"
#include "file_system.h"
#include "pen.h"
#include "threads.h"

// curls include 
#define CURL_STATICLIB
#include <curl/curl.h>

namespace
{
    void*  user_setup(void* params);
    loop_t user_update();
    void   user_shutdown();
} // namespace

// entry function, where we can configure low level details, like window or renderer in pen_creation_params
namespace pen
{
    pen_creation_params pen_entry(int argc, char** argv)
    {
        pen::pen_creation_params p;
        p.window_width = 1280;
        p.window_height = 720;
        p.window_title = "curl_example";
        p.window_sample_count = 4;
        p.user_thread_function = user_setup;
        p.flags = pen::e_pen_create_flags::console_app;
        return p;
    }
} // namespace pen

// web friendly main loop
namespace
{
    pen::job_thread_params* job_params;
    pen::job*               p_thread_info;

    void* user_setup(void* params)
    {
        // unpack the params passed to the thread and signal to the engine it ok to proceed
        job_params = (pen::job_thread_params*)params;
        p_thread_info = job_params->job_info;
        pen::semaphore_post(p_thread_info->p_sem_continue, 1);

        // we call user_update once per frame
        pen_main_loop(user_update);
        return PEN_THREAD_OK;
    }

    void user_shutdown()
    {
        pen::semaphore_post(p_thread_info->p_sem_terminated, 1);
    }

    loop_t user_update()
    {
        pen::thread_sleep_ms(16);

        // msg from the engine we want to terminate
        if (pen::semaphore_try_wait(p_thread_info->p_sem_exit))
        {
            user_shutdown();
            pen_main_loop_exit();
        }

        exit(0);
        pen_main_loop_continue();
    }


    const c8* list[] = {
        "hello"
        "world"
    };

} // namespace