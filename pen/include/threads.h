#ifndef _threads_h
#define _threads_h

#include "pen.h"

#if _WIN32
#include <windows.h>
#define PEN_THREAD_ROUTINE( FP ) LPTHREAD_START_ROUTINE FP
#else
#define PEN_THREAD_ROUTINE( FP ) PEN_TRV (*FP)(void* data)
#endif

namespace pen
{
	// Minimalist C-Style thread wrapper API 
	// Includes functions to create jobs, threads, mutex and semaphore
	// Implementations currently in posix and win32

	struct thread;
	struct mutex;
	struct semaphore;

	typedef void(*completion_callback)(void*);
    
	// A Job is just a thread with some data, a callback
	// and some syncronisation semaphores

    struct job_thread
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
        job_thread* job_thread_info;
        void*       user_data;
    };
    
    enum thread_start_flags : u32
    {
        THREAD_START_DETACHED = 1,
        THREAD_START_JOINABLE = 2,
        THREAD_CALL_FUNCTION = 3
    };

	enum default_thread_create_flags
	{
		PEN_CREATE_RENDER_THREAD = 1<<0,
		PEN_CREATE_AUDIO_THREAD = 1<<1,
	};

	struct default_thread_info
	{
		u32 flags;
		void* render_thread_params;
		void* audio_thread_params;
		void* user_thread_params;
	};

    //thread job
    
    void             threads_create_default_jobs( const default_thread_info& info) ;
    void             threads_terminate_jobs();
    pen::job_thread* threads_create_job( PEN_THREAD_ROUTINE( thread_func ), u32 stack_size, void* user_data, thread_start_flags flags, completion_callback cb = nullptr);
    
	//threads
	pen::thread*	threads_create( PEN_THREAD_ROUTINE( thread_func ), u32 stack_size, void* thread_params, thread_start_flags flags );
	void			threads_destroy( pen::thread* p_thread );

	//mutex
	pen::mutex*		threads_mutex_create( );
	void			threads_mutex_destroy( pen::mutex* p_mutex );

	void			threads_mutex_lock( pen::mutex* p_mutex );
	u32				threads_mutex_try_lock( pen::mutex* p_mutex );
	void			threads_mutex_unlock( pen::mutex* p_mutex );

	//semaphore
	pen::semaphore* threads_semaphore_create( u32 initial_count, u32 max_count );
	void			threads_semaphore_destroy( pen::semaphore* p_semaphore );

    bool			threads_semaphore_try_wait( pen::semaphore* p_semaphore );
	bool			threads_semaphore_wait( pen::semaphore* p_semaphore );
	void			threads_semaphore_signal( pen::semaphore* p_semaphore, u32 count );

	//actions
	void			threads_sleep_ms( u32 milliseconds );
	void			threads_sleep_us( u32 microseconds );
}


#endif
