#include <jni.h>
#include "threads.h"

//extern pen::window_creation_params pen_window;
//extern PEN_TRV pen::user_entry( void* params );

int main()
{
    //pen::user_entry(nullptr);
}

namespace pen
{
    void thread_semaphore_signal(pen::semaphore*, unsigned int)
    {

    }

    void thread_sleep_us(unsigned int)
    {

    }

    bool thread_semaphore_try_wait(pen::semaphore*)
    {
        return true;
    }
}

