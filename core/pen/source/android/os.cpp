// os.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "os.h"

#include "threads.h"

#include <jni.h>

// global externs
pen::user_info              pen_user_info;
pen::window_creation_params pen_window;

int main()
{
}

void pen_make_gl_context_current()
{

}

void pen_gl_swap_buffers()
{

}

namespace pen
{
    /*
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
    */

    hash_id window_get_id()
    {
        return 0;
    }

    const c8* window_get_title()
    {
        return "pmtech_window";
    }

    const Str os_path_for_resource(const c8* filename)
    {
        return "todo";
    }

    bool os_update()
    {
        return true;
    }

    void os_terminate(u32 return_code)
    {

    }

    void window_get_size(s32& width, s32& height)
    {

    }
} // namespace pen
