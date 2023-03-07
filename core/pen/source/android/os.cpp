// os.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "threads.h"
#include <jni.h>

int main()
{
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
