#include <pthread.h>
#include <semaphore.h>
#include "threads.h"
#include "memory.h"
#include <unistd.h>

namespace pen
{
	typedef struct thread
	{
        pthread_t handle;
	} thread;

	typedef struct mutex
	{
        pthread_mutex_t* handle;
	} mutex;

	typedef struct semaphore
	{
        sem_t* handle;
	} semaphore;

	pen::thread* threads_create( PEN_THREAD_ROUTINE( thread_func ), u32 stack_size, void* thread_params, thread_start_flags flags )
	{
        //allocate penthread handle
        pen::thread* new_thread = (pen::thread*)pen::memory_alloc( sizeof( pen::thread ) );
        
        // create the thread using posix
        pthread_attr_t  attr;
        int             err;
        int             thread_err;
        
        err = pthread_attr_init(&attr);
        assert(!err);
        
        err = pthread_attr_setdetachstate(&attr, flags);
        assert(!err);
        
        thread_err = pthread_create(&new_thread->handle, &attr, thread_func, thread_params );
        
        err = pthread_attr_destroy(&attr);
        assert(!err);
        
        assert(!thread_err);

		return new_thread;
	}

	void threads_destroy( pen::thread* p_mutex )
	{
		//CloseHandle( p_mutex->handle );

		//pen::memory_free( p_mutex );
	}

	void threads_suspend( pen::thread* p_thread )
	{
		//SuspendThread( p_thread->handle );
	}

	pen::mutex* threads_mutex_create( )
	{
		//pen::mutex* new_mutex = (pen::mutex*)pen::memory_alloc( sizeof( pen::mutex ) );

		//InitializeCriticalSection( &new_mutex->cs );

		return nullptr;
	}

	void threads_mutex_destroy( mutex* p_mutex )
	{
		//DeleteCriticalSection( &p_mutex->cs );

		//pen::memory_free( p_mutex );
	}

	void threads_mutex_lock( mutex* p_mutex )
	{
		//EnterCriticalSection( &p_mutex->cs );
	}

	u32 threads_mutex_try_lock( mutex* p_mutex )
	{		
        return 0; //TryEnterCriticalSection( &p_mutex->cs );;
	}

	void threads_mutex_unlock( mutex* p_mutex )
	{
		//LeaveCriticalSection( &p_mutex->cs );
	}

	pen::semaphore* threads_semaphore_create( u32 initial_count, u32 max_count )
	{
		//pen::semaphore* new_semaphore = (pen::semaphore*)pen::memory_alloc( sizeof( pen::semaphore ) );

		//new_semaphore->handle = CreateSemaphore( NULL, initial_count, max_count, NULL );

		return nullptr;
	}

	void threads_semaphore_destroy( semaphore* p_semaphore )
	{
		//CloseHandle( p_semaphore->handle );

		//pen::memory_free( p_semaphore );
	}

	bool threads_semaphore_wait( semaphore* p_semaphore )
	{
		//DWORD res = WaitForSingleObject( p_semaphore->handle, INFINITE );

		//if( !res )
		//{
		//	return TRUE;
		//}

		//return FALSE;
	}

	void threads_semaphore_signal( semaphore* p_semaphore, u32 count )
	{
		//ReleaseSemaphore( p_semaphore->handle, count, NULL );
	}

	void threads_sleep_ms( u32 milliseconds )
	{
        usleep(milliseconds*1000);
	}

	void threads_sleep_us( u32 microseconds )
	{
        usleep(microseconds);
	}
}
