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
#include <unistd.h>

namespace pen
{
    struct thread
    {
        u32 handle;
    };

    struct mutex
    {
        u32 handle;
    };

    struct semaphore
    {
        u32 count;
    };

    a_u32 semaphone_index = {0};

    pen::thread* thread_create(dispatch_thread thread_func, u32 stack_size, void* thread_params, thread_start_flags flags)
    {
        pen::thread* new_thread = (pen::thread*)pen::memory_alloc(sizeof(pen::thread));        
        thread_func(thread_params);
        return new_thread;
    }

    pen::mutex* mutex_create()
    {
        pen::mutex* new_mutex = (pen::mutex*)pen::memory_alloc(sizeof(pen::mutex));
        return new_mutex;
    }

    void mutex_destroy(mutex* p_mutex)
    {
        pen::memory_free(p_mutex);
    }

    void mutex_lock(mutex* p_mutex)
    {
    }

    u32 mutex_try_lock(mutex* p_mutex)
    {
        return true;
    }

    void mutex_unlock(mutex* p_mutex)
    {
    }

    void thread_sleep_ms(u32 milliseconds)
    {
        usleep(milliseconds * 1000);
    }

    void thread_sleep_us(u32 microseconds)
    {
        usleep(microseconds);
    }

    pen::semaphore* semaphore_create(u32 initial_count, u32 max_count)
    {
        pen::semaphore* new_semaphore = (pen::semaphore*)pen::memory_alloc(sizeof(pen::semaphore));
        new_semaphore->count = 0;
        return new_semaphore;
    }

    void semaphore_destroy(semaphore* p_semaphore)
    {
        pen::memory_free(p_semaphore);
    }

    bool semaphore_wait(semaphore* p_semaphore)
    {
        return true;
    }

    bool semaphore_try_wait(pen::semaphore* p_semaphore)
    {
        if(p_semaphore->count != 0)
            return true;
        return false;
    }

    void semaphore_post(semaphore* p_semaphore, u32 count)
    {
        
    }
} // namespace pen
