#include "threads.h"
#include "memory.h"
#include <windows.h>

namespace pen
{
	typedef struct thread
	{
		HANDLE handle;
		DWORD id;
	} thread;

	typedef struct mutex
	{
		CRITICAL_SECTION cs;
	} mutex;

	typedef struct semaphore
	{
		HANDLE  handle;
	} semaphore;

	pen::thread* threads_create( PEN_THREAD_ROUTINE( thread_func ), u32 stack_size, void* thread_params, thread_start_flags flags )
	{
		pen::thread* new_thread = (pen::thread*)pen::memory_alloc( sizeof( pen::thread ) );

		new_thread->handle = CreateThread( NULL, stack_size, thread_func, thread_params, flags, &new_thread->id );

		return new_thread;
	}

	void threads_destroy( pen::thread* p_mutex )
	{
		CloseHandle( p_mutex->handle );

		pen::memory_free( p_mutex );
	}

	void threads_suspend( pen::thread* p_thread )
	{
		SuspendThread( p_thread->handle );
	}

	pen::mutex* threads_mutex_create( )
	{
		pen::mutex* new_mutex = (pen::mutex*)pen::memory_alloc( sizeof( pen::mutex ) );

		InitializeCriticalSection( &new_mutex->cs );

		return new_mutex;
	}

	void threads_mutex_destroy( mutex* p_mutex )
	{
		DeleteCriticalSection( &p_mutex->cs );

		pen::memory_free( p_mutex );
	}

	void threads_mutex_lock( mutex* p_mutex )
	{
		EnterCriticalSection( &p_mutex->cs );
	}

	u32 threads_mutex_try_lock( mutex* p_mutex )
	{		
		return TryEnterCriticalSection( &p_mutex->cs );;
	}

	void threads_mutex_unlock( mutex* p_mutex )
	{
		LeaveCriticalSection( &p_mutex->cs );
	}

	pen::semaphore* threads_semaphore_create( u32 initial_count, u32 max_count )
	{
		pen::semaphore* new_semaphore = (pen::semaphore*)pen::memory_alloc( sizeof( pen::semaphore ) );

		new_semaphore->handle = CreateSemaphore( NULL, initial_count, max_count, NULL );

		return new_semaphore;
	}

	void threads_semaphore_destroy( semaphore* p_semaphore )
	{
		CloseHandle( p_semaphore->handle );

		pen::memory_free( p_semaphore );
	}

	bool threads_semaphore_wait( semaphore* p_semaphore )
	{
		DWORD res = WaitForSingleObject( p_semaphore->handle, INFINITE );

		if( !res )
		{
			return TRUE;
		}

		return FALSE;
	}

	void threads_semaphore_signal( semaphore* p_semaphore, u32 count )
	{
		ReleaseSemaphore( p_semaphore->handle, count, NULL );
	}

	void threads_sleep_ms( u32 milliseconds )
	{
		Sleep( milliseconds );
	}

	void threads_sleep_us( u32 microseconds )
	{

	}
}
