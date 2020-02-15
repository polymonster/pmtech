// job_threads.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "renderer.h"
#include "threads.h"

namespace pen
{
#define MAX_THREADS 8

    static job s_jt[MAX_THREADS];
    static u32 s_num_active_threads = 0;

    pen::job* jobs_create_job(dispatch_thread thread_func, u32 stack_size, void* user_data, thread_start_flags flags,
                              completion_callback cb)
    {
        if (s_num_active_threads >= MAX_THREADS)
        {
            return nullptr;
        }

        job_thread_params params;

        job* jt = &s_jt[s_num_active_threads++];

        jt->p_sem_continue = semaphore_create(0, 1);
        jt->p_sem_consume = semaphore_create(0, 1);
        jt->p_sem_exit = semaphore_create(0, 1);
        jt->p_sem_terminated = semaphore_create(0, 1);
        jt->p_completion_callback = cb;

        params.user_data = user_data;
        params.job_info = jt;

        jt->p_thread = thread_create(thread_func, stack_size, (void*)&params, flags);

        // wait till the thread initialises so any data passed to it is ok.
        pen::semaphore_wait(jt->p_sem_continue);

        return jt;
    }

    void jobs_create_default(const pen::default_thread_info& info)
    {
        jobs_create_job(&pen::user_entry, 1024 * 1024, info.user_thread_params, pen::THREAD_START_DETACHED);
    }

    bool jobs_terminate_all()
    {
        // remove threads in reverse order
        for (s32 i = s_num_active_threads - 1; i > 0; --i)
        {
            pen::semaphore_post(s_jt[i].p_sem_exit, 1);
            if (pen::semaphore_try_wait(s_jt[i].p_sem_terminated))
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
