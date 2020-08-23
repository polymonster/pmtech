// threads.h
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

// Minimalist cross platform thread wrapper api.
// Includes functions to create jobs, threads, mutex and semaphore.

#pragma once

#include "pen.h"

namespace pen
{
    struct thread;
    struct mutex;
    struct semaphore;

    typedef void (*completion_callback)(void*);
    typedef void* (*dispatch_thread)(void*);

    // A Job is just a thread with some user data, a callback
    // and some syncronisation semaphores

    struct job
    {
        thread*             p_thread = nullptr;
        semaphore*          p_sem_consume = nullptr;
        semaphore*          p_sem_continue = nullptr;
        semaphore*          p_sem_exit = nullptr;
        semaphore*          p_sem_terminated = nullptr;
        completion_callback p_completion_callback = nullptr;

        f32 thread_time;
    };

    struct job_thread_params
    {
        job*  job_info;
        void* user_data;
    };

    namespace e_thread_start_flags
    {
        enum thread_start_flags_t
        {
            detached = 1,
            joinable = 1 << 2,
            call_function = 1 << 3
        };
    }
    typedef e_thread_start_flags::thread_start_flags_t thread_start_flags;

    struct default_thread_info
    {
        u32   flags;
        void* render_thread_params;
        void* audio_thread_params;
        void* user_thread_params;
    };

    // Threads
    thread* thread_create(dispatch_thread thread_func, u32 stack_size, void* thread_params, thread_start_flags flags);
    void    thread_sleep_ms(u32 milliseconds);
    void    thread_sleep_us(u32 microseconds);

    // Jobs
    bool    jobs_terminate_all();
    job*    jobs_create_job(dispatch_thread thread_func, u32 stack_size, void* user_data, thread_start_flags flags,
                            completion_callback cb = nullptr);

    // Mutex
    mutex* mutex_create();
    void   mutex_destroy(mutex* p_mutex);
    void   mutex_lock(mutex* p_mutex);
    u32    mutex_try_lock(mutex* p_mutex);
    void   mutex_unlock(mutex* p_mutex);

    // Semaphore
    semaphore* semaphore_create(u32 initial_count, u32 max_count);
    void       semaphore_destroy(semaphore* p_semaphore);
    bool       semaphore_try_wait(semaphore* p_semaphore);
    bool       semaphore_wait(semaphore* p_semaphore);
    void       semaphore_post(semaphore* p_semaphore, u32 count);

} // namespace pen
