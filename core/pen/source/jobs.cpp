// jobs.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "console.h"
#include "data_struct.h"
#include "renderer.h"
#include "threads.h"

#define MAX_THREADS 32 // lazy fixed sized array to avoid any thread saftey issues

using namespace pen;

namespace
{
    job                        s_jt[MAX_THREADS];
    u32                        s_num_active_threads = 0;
    single_thread_update_func* s_single_thread_funcs = nullptr;
} // namespace

namespace pen
{
    pen::job* jobs_create_job(dispatch_thread thread_func, u32 stack_size, void* user_data, thread_start_flags flags,
                              completion_callback cb)
    {
        if (s_num_active_threads >= MAX_THREADS)
            return nullptr;

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

    bool jobs_terminate_all()
    {
        // remove threads in reverse order
        for (s32 i = s_num_active_threads - 1; i >= 0; --i)
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

    void jobs_create_single_thread_update(single_thread_update_func func)
    {
        sb_push(s_single_thread_funcs, func);
    }

    void jobs_run_single_threaded()
    {
        s32 count = sb_count(s_single_thread_funcs);
        for (s32 i = 0; i < count; ++i)
        {
            ((single_thread_update_func)s_single_thread_funcs[i])();
        }
    }
} // namespace pen
