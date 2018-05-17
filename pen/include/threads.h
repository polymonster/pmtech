#ifndef _thread_h
#define _thread_h

#include "pen.h"

#if _WIN32
#include <windows.h>
#define PEN_THREAD_ROUTINE(FP) LPTHREAD_START_ROUTINE FP
#else
#define PEN_THREAD_ROUTINE(FP) PEN_TRV (*FP)(void* data)
#endif

namespace pen
{
    // Minimalist C-Style thread wrapper API
    // Includes functions to create jobs, threads, mutex and semaphore
    // Implementations currently in posix and win32

    struct thread;
    struct mutex;
    struct semaphore;

    typedef void (*completion_callback)(void*);

    // A Job is just a thread with some user data, a callback
    // and some syncronisation semaphores

    struct job
    {
        thread* p_thread = nullptr;
        semaphore* p_sem_consume = nullptr;
        semaphore* p_sem_continue = nullptr;
        semaphore* p_sem_exit = nullptr;
        semaphore* p_sem_terminated = nullptr;
        completion_callback p_completion_callback = nullptr;

        f32 thread_time;
    };

    struct job_thread_params
    {
        job* job_info;
        void* user_data;
    };

    enum thread_start_flags : u32
    {
        THREAD_START_DETACHED = 1,
        THREAD_START_JOINABLE = 2,
        THREAD_CALL_FUNCTION = 3
    };

    enum default_thread_create_flags
    {
        PEN_CREATE_RENDER_THREAD = 1 << 0,
        PEN_CREATE_AUDIO_THREAD = 1 << 1,
    };

    struct default_thread_info
    {
        u32 flags;
        void* render_thread_params;
        void* audio_thread_params;
        void* user_thread_params;
    };

    // Job
    void thread_create_default_jobs(const default_thread_info& info);
    void thread_terminate_jobs();
    job* thread_create_job(PEN_THREAD_ROUTINE(thread_func), u32 stack_size, void* user_data, thread_start_flags flags,
                           completion_callback cb = nullptr);

    // Threads
    thread* thread_create(PEN_THREAD_ROUTINE(thread_func), u32 stack_size, void* thread_params, thread_start_flags flags);
    void thread_destroy(pen::thread* p_thread);

    // Mutex
    mutex* thread_mutex_create();
    void thread_mutex_destroy(mutex* p_mutex);

    void thread_mutex_lock(mutex* p_mutex);
    u32 thread_mutex_try_lock(mutex* p_mutex);
    void thread_mutex_unlock(mutex* p_mutex);

    // Semaphore
    semaphore* thread_semaphore_create(u32 initial_count, u32 max_count);
    void thread_semaphore_destroy(semaphore* p_semaphore);

    bool thread_semaphore_try_wait(semaphore* p_semaphore);
    bool thread_semaphore_wait(semaphore* p_semaphore);
    void thread_semaphore_signal(semaphore* p_semaphore, u32 count);

    // Actions
    void thread_sleep_ms(u32 milliseconds);
    void thread_sleep_us(u32 microseconds);
} // namespace pen

#endif
