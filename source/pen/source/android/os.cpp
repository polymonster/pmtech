// os.cpp
// Copyright 2014 - 2019 Alex Dixon. 
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "threads.h"
#include <jni.h>

// extern pen::window_creation_params pen_window;
// extern PEN_TRV pen::user_entry( void* params );

int main()
{
    // pen::user_entry(nullptr);
}

namespace pen
{
    void semaphore_post(pen::semaphore*, unsigned int)
    {
    }

    void thread_sleep_us(unsigned int)
    {
    }

    bool semaphore_try_wait(pen::semaphore*)
    {
        return true;
    }
} // namespace pen
