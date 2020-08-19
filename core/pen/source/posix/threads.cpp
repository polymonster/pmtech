// threads.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "threads.h"
#include "console.h"
#include "hash.h"
#include "memory.h"
#include "os.h"
#include "pen_string.h"

#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

namespace pen
{
    struct thread
    {
        pthread_t handle;
    };

    struct mutex
    {
        pthread_mutex_t handle;
    };

    a_u32 semaphone_index = {0};

    pen::thread* thread_create(dispatch_thread thread_func, u32 stack_size, void* thread_params, thread_start_flags flags)
    {
        // allocate penthread handle
        pen::thread* new_thread = (pen::thread*)pen::memory_alloc(sizeof(pen::thread));

        // create the thread using posix
        pthread_attr_t attr;
        int            err;
        int            thread_err;

        err = pthread_attr_init(&attr);
        PEN_ASSERT(!err);

        err = pthread_attr_setdetachstate(&attr, flags);
        PEN_ASSERT(!err);

        thread_err = pthread_create(&new_thread->handle, &attr, thread_func, thread_params);

        err = pthread_attr_destroy(&attr);
        PEN_ASSERT(!err);

        PEN_ASSERT(!thread_err);

        return new_thread;
    }

    pen::mutex* mutex_create()
    {
        pen::mutex* new_mutex = (pen::mutex*)pen::memory_alloc(sizeof(pen::mutex));

        int                 err;
        pthread_mutexattr_t mta;

        err = pthread_mutexattr_init(&mta);

        err = pthread_mutex_init(&new_mutex->handle, &mta);

        return new_mutex;
    }

    void mutex_destroy(mutex* p_mutex)
    {
        pthread_mutex_destroy(&p_mutex->handle);

        pen::memory_free(p_mutex);
    }

    void mutex_lock(mutex* p_mutex)
    {
        pthread_mutex_lock(&p_mutex->handle);
    }

    u32 mutex_try_lock(mutex* p_mutex)
    {
        int err = pthread_mutex_trylock(&p_mutex->handle);

        return err == 0;
    }

    void mutex_unlock(mutex* p_mutex)
    {
        pthread_mutex_unlock(&p_mutex->handle);
    }

    void thread_sleep_ms(u32 milliseconds)
    {
        usleep(milliseconds * 1000);
    }

    void thread_sleep_us(u32 microseconds)
    {
        usleep(microseconds);
    }

#ifndef PEN_PLATFORM_WEB // posix semaphore implementation proper
    struct semaphore
    {
        sem_t* handle;
    };
    pen::semaphore* semaphore_create(u32 initial_count, u32 max_count)
    {
        pen::semaphore* new_semaphore = (pen::semaphore*)pen::memory_alloc(sizeof(pen::semaphore));

        c8 name_buf[32];
        pen::string_format(&name_buf[0], 32, "sem%i%i", semaphone_index++, window_get_id());
        
        sem_unlink(name_buf);
        new_semaphore->handle = sem_open(name_buf, O_CREAT, 0, 0);

        assert(!(new_semaphore->handle == (void*)-1));
        return new_semaphore;
    }

    void semaphore_destroy(semaphore* p_semaphore)
    {
        sem_close(p_semaphore->handle);
        pen::memory_free(p_semaphore);
    }

    bool semaphore_wait(semaphore* p_semaphore)
    {
        sem_wait(p_semaphore->handle);
        return true;
    }

    bool semaphore_try_wait(pen::semaphore* p_semaphore)
    {
        if (sem_trywait(p_semaphore->handle) == 0)
            return true;

        return false;
    }

    void semaphore_post(semaphore* p_semaphore, u32 count)
    {
        sem_post(p_semaphore->handle);
    }
#else // emscripten posix sem api emulation
    // emscripten posix sem api returns null when calling sem_open.
    // this implementation uses atomics and a spin lock to emulate posix semaphore
    struct semaphore
    {
        a_u32 handle;
    };
    pen::semaphore* semaphore_create(u32 initial_count, u32 max_count)
    {
        pen::semaphore* new_semaphore = (pen::semaphore*)pen::memory_alloc(sizeof(pen::semaphore));

        return new_semaphore;
    }

    void semaphore_destroy(semaphore* p_semaphore)
    {
        pen::memory_free(p_semaphore);
    }

    bool semaphore_wait(semaphore* p_semaphore)
    {
        while(p_semaphore->handle == 0)
            usleep(10);

        p_semaphore->handle = 0;
        return true;
    }

    bool semaphore_try_wait(pen::semaphore* p_semaphore)
    {
        if (p_semaphore->handle > 0)
        {
            p_semaphore->handle = 0;
            return true;
        }

        return false;
    }

    void semaphore_post(semaphore* p_semaphore, u32 count)
    {
        p_semaphore->handle++;
    }
#endif
} // namespace pen
