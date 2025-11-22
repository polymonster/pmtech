// os.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "os.h"

#include "threads.h"

#include <jni.h>
#include <stdio.h>

// need to copy example/src/main/jniLibs (fmod)
// inject strings for other samples res/values

// BLOG NOTES:
// - gradle version, always changing
// - depracated maven etc
// - trying to link .so vs .a
// - No implementation found for void cc.pmtech.pen_activity.entry() (tried Java_cc_pmtech_pen_1activity_entry and Java_cc_pmtech_pen_1activity_entry__) - is the library loaded, e.g. System.loadLibrary?

// DONE:
// call c++ from java
// need to copy fmod.jar and add it as impl in gradle

// global externs
pen::user_info              pen_user_info;
pen::window_creation_params pen_window;

void pen_make_gl_context_current()
{

}

void pen_gl_swap_buffers()
{

}

#define PEN_JNIFUNC(ret, actname, funcname) extern "C" JNIEXPORT ret JNICALL Java_cc_pmtech_##actname##_##funcname

PEN_JNIFUNC(void, pen_1activity, entry)(JNIEnv* env, jclass thiz)
{
    PEN_LOG("hello print %i\n", 69);
}

namespace pen
{
    u32 window_init(void* params)
    {
        return 0;
    }

    hash_id window_get_id()
    {
        return 0;
    }

    const c8* window_get_title()
    {
        return "pmtech_window";
    }

    void* window_get_primary_display_handle()
    {
        return nullptr;
    }

    void window_get_frame(window_frame& f)
    {

    }

    void window_set_frame(const window_frame& f)
    {

    }

    void window_get_size(s32& width, s32& height)
    {

    }

    void window_set_size(s32 width, s32 height)
    {

    }

    f32 window_get_aspect()
    {
        return 0.0f;
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

    void os_set_cursor_pos(u32 client_x, u32 client_y)
    {

    }

    const user_info& os_get_user_info()
    {
        return {};
    }

    bool input_undo_pressed()
    {
        return false;
    }

    bool input_redo_pressed()
    {
        return false;
    }

} // namespace pen
