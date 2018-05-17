#include "threads.h"
#include "console.h"
#include "memory.h"
#include "pen_string.h"
#include <pthread.h>

#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>

namespace pen
{
    typedef struct thread
    {
        pthread_t handle;
    } thread;

    typedef struct mutex
    {
        pthread_mutex_t handle;
    } mutex;

    typedef struct semaphore
    {
        sem_t* handle;
    } semaphore;

    u32 semaphone_index = 0;

    pen::thread* thread_create(PEN_THREAD_ROUTINE(thread_func), u32 stack_size, void* thread_params, thread_start_flags flags)
    {
        // allocate penthread handle
        pen::thread* new_thread = (pen::thread*)pen::memory_alloc(sizeof(pen::thread));

        // create the thread using posix
        pthread_attr_t attr;
        int err;
        int thread_err;

        err = pthread_attr_init(&attr);
        assert(!err);

        err = pthread_attr_setdetachstate(&attr, flags);
        assert(!err);

        thread_err = pthread_create(&new_thread->handle, &attr, thread_func, thread_params);

        err = pthread_attr_destroy(&attr);
        assert(!err);

        assert(!thread_err);

        return new_thread;
    }

    void thread_destroy(pen::thread* p_thread)
    {
        pthread_cancel(p_thread->handle);

        pen::memory_free(p_thread);
    }

    pen::mutex* thread_mutex_create()
    {
        pen::mutex* new_mutex = (pen::mutex*)pen::memory_alloc(sizeof(pen::mutex));

        int err;
        pthread_mutexattr_t mta;

        err = pthread_mutexattr_init(&mta);

        err = pthread_mutex_init(&new_mutex->handle, &mta);

        return new_mutex;
    }

    void thread_mutex_destroy(mutex* p_mutex)
    {
        pthread_mutex_destroy(&p_mutex->handle);

        pen::memory_free(p_mutex);
    }

    void thread_mutex_lock(mutex* p_mutex)
    {
        pthread_mutex_lock(&p_mutex->handle);
    }

    u32 thread_mutex_try_lock(mutex* p_mutex)
    {
        int err = pthread_mutex_trylock(&p_mutex->handle);

        return err == 0;
    }

    void thread_mutex_unlock(mutex* p_mutex)
    {
        pthread_mutex_unlock(&p_mutex->handle);
    }

    pen::semaphore* thread_semaphore_create(u32 initial_count, u32 max_count)
    {
        pen::semaphore* new_semaphore = (pen::semaphore*)pen::memory_alloc(sizeof(pen::semaphore));

        char name_buf[16];

        pen::string_format(&name_buf[0], 32, "sem%i", semaphone_index++);

        sem_unlink(name_buf);
        new_semaphore->handle = sem_open(name_buf, O_CREAT, 0, 0);

        assert(!(new_semaphore->handle == (void*)-1));

        return new_semaphore;
    }

    void thread_semaphore_destroy(semaphore* p_semaphore)
    {
        sem_close(p_semaphore->handle);

        pen::memory_free(p_semaphore);
    }

    bool thread_semaphore_wait(semaphore* p_semaphore)
    {
        sem_wait(p_semaphore->handle);

        return true;
    }

    bool thread_semaphore_try_wait(pen::semaphore* p_semaphore)
    {
        if (sem_trywait(p_semaphore->handle) == 0)
        {
            return true;
        }

        return false;
    }

    void thread_semaphore_signal(semaphore* p_semaphore, u32 count)
    {
        sem_post(p_semaphore->handle);
    }

    void thread_sleep_ms(u32 milliseconds)
    {
        usleep(milliseconds * 1000);
    }

    void thread_sleep_us(u32 microseconds)
    {
        usleep(microseconds);
    }
} // namespace pen
