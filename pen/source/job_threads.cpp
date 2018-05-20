#include "audio.h"
#include "renderer.h"
#include "threads.h"

namespace pen
{
#define MAX_THREADS 8

    static job s_jt[MAX_THREADS];
    static u32 s_num_active_threads = 0;

    pen::job* thread_create_job(PEN_THREAD_ROUTINE(thread_func), u32 stack_size, void* user_data, thread_start_flags flags,
                                completion_callback cb)
    {
        if (s_num_active_threads >= MAX_THREADS)
        {
            return nullptr;
        }

        job_thread_params params;

        job* jt = &s_jt[s_num_active_threads++];

        jt->p_sem_continue        = thread_semaphore_create(0, 1);
        jt->p_sem_consume         = thread_semaphore_create(0, 1);
        jt->p_sem_exit            = thread_semaphore_create(0, 1);
        jt->p_sem_terminated      = thread_semaphore_create(0, 1);
        jt->p_completion_callback = cb;

        params.user_data = user_data;
        params.job_info  = jt;

        jt->p_thread = thread_create(thread_func, stack_size, (void*)&params, flags);

        // wait till the thread initialises so any data passed to it is ok.
        pen::thread_semaphore_wait(jt->p_sem_continue);

        return jt;
    }

    void thread_create_default_jobs(const pen::default_thread_info& info)
    {
        if (info.flags & PEN_CREATE_RENDER_THREAD)
        {
            // Render thread is created on the main (window) thread now by default
            thread_create_job(&pen::renderer_thread_function, 1024 * 1024, info.render_thread_params,
                              pen::THREAD_START_DETACHED);
        }

        if (info.flags & PEN_CREATE_AUDIO_THREAD)
        {
#if !TARGET_OS_IPHONE
            thread_create_job(&pen::audio_thread_function, 1024 * 1024, info.audio_thread_params, pen::THREAD_START_DETACHED);
#endif
        }

        thread_create_job(&pen::user_entry, 1024 * 1024, info.user_thread_params, pen::THREAD_START_DETACHED);
    }

    bool thread_terminate_jobs()
    {
        // remove threads in reverse order
        for (s32 i = s_num_active_threads - 1; i > 0; --i)
        {
            pen::thread_semaphore_signal(s_jt[i].p_sem_exit, 1);
            if (pen::thread_semaphore_try_wait(s_jt[i].p_sem_terminated))
            {
                s_num_active_threads--;
            }
            else
            {
                return false;
            }
        }

        return true;
    }
} // namespace pen
