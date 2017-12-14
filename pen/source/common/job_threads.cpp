#include "threads.h"
#include "audio.h"
#include "renderer.h"

namespace pen
{
#define MAX_THREADS 8
    
    static job_thread   s_jt[ MAX_THREADS ];
    static u32          s_num_active_threads = 0;
    
    pen::job_thread* threads_create_job( PEN_THREAD_ROUTINE( thread_func ), u32 stack_size, void* user_data, thread_start_flags flags )
    {
        if( s_num_active_threads >= MAX_THREADS )
        {
            return nullptr;
        }
        
        job_thread_params params;
        
        job_thread* jt = &s_jt[ s_num_active_threads++ ];
        
        jt->p_sem_continue = threads_semaphore_create(0,1);
        jt->p_sem_consume = threads_semaphore_create(0,1);
        jt->p_sem_exit = threads_semaphore_create(0,1);
        jt->p_sem_terminated = threads_semaphore_create(0,1);
        
        params.user_data = user_data;
        params.job_thread_info = jt;

        jt->p_thread = threads_create( thread_func, stack_size, (void*)&params, flags );
        
        //wait till the thread initialises so any data passed to it is ok.
        pen::threads_semaphore_wait( jt->p_sem_continue );
        
        return jt;
    }
    
	void threads_create_default_jobs( const pen::default_thread_info& info )
    {
		if (info.flags & PEN_CREATE_RENDER_THREAD)
		{
			threads_create_job(&pen::renderer_thread_function, 1024 * 1024, info.render_thread_params, pen::THREAD_START_DETACHED);
		}

		if (info.flags & PEN_CREATE_AUDIO_THREAD)
		{
			threads_create_job(&pen::audio_thread_function, 1024 * 1024, info.audio_thread_params, pen::THREAD_START_DETACHED);
		}

		threads_create_job(&pen::game_entry, 1024 * 1024, info.user_thread_params, pen::THREAD_START_DETACHED);
    }
    
    void threads_terminate_jobs()
    {
        //remove threads in reverse order
        for( s32 i = s_num_active_threads - 1; i > 0; --i )
        {
            pen::threads_semaphore_signal( s_jt[i].p_sem_exit, 1 );
            pen::threads_semaphore_wait( s_jt[i].p_sem_terminated );
        }
    }
}
