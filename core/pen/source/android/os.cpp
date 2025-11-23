// os.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "os.h"

#include "threads.h"
#include "renderer.h"

#include <jni.h>
#include <stdio.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

// need to copy example/src/main/jniLibs (fmod)
// inject strings for other samples res/values
// viewport and window sizes
// filesystem functions into os?

// BLOG NOTES:
// - gradle version, always changing
// - deprecated maven etc
// - trying to link .so vs .a
// - EGL_NONE, array terminator.
// - No implementation found for void cc.pmtech.pen_activity.entry() (tried Java_cc_pmtech_pen_1activity_entry and Java_cc_pmtech_pen_1activity_entry__) - is the library loaded, e.g. System.loadLibrary?

// DONE:
// call c++ from java
// need to copy fmod.jar and add it as impl in gradle
// build shaders
// setup assetdirs
// asset manager

#define PEN_JNIFUNC(ret, actname, funcname) extern "C" JNIEXPORT ret JNICALL Java_cc_pmtech_##actname##_##funcname

// global externs
pen::user_info              pen_user_info;
pen::window_creation_params pen_window;

namespace
{
    struct egl_context
    {
        EGLContext ctx;
        EGLSurface surface;
        EGLDisplay display;
    };
    egl_context s_egl_context;

    struct android_context
    {
        JavaVM*         m_java_vm = nullptr;
        AAssetManager*  m_asset_manager = nullptr;
        ANativeWindow*  m_window = nullptr;
        jclass          m_surface_wrapper_class;
        jobject         m_surface_wrapper_object;
    };
    android_context s_android_context;
}

void pen_make_gl_context_current()
{

}

void pen_gl_swap_buffers()
{
    eglSwapBuffers(s_egl_context.display, s_egl_context.surface);
}

PEN_JNIFUNC(void, pen_1activity, entry)(JNIEnv* env, jclass thiz)
{
    PEN_LOG("hello print %i\n", 69);
}

PEN_JNIFUNC(void, pen_1activity, register_1asset_1manager)(JNIEnv* env, jclass thiz, jobject asset_manager)
{
    s_android_context.m_asset_manager = AAssetManager_fromJava(env, asset_manager);
}

PEN_JNIFUNC(void, SurfaceWrapper, render)(JNIEnv* env, jclass thiz, jobject caller)
{
    pen::os_update();
    pen::renderer_dispatch();
}

PEN_JNIFUNC(void, SurfaceWrapper, surface_1created)(JNIEnv* env, jclass thiz, jobject surface, int window_width, int window_height, int device_width, int device_height, int orientation, long app_ptr)
{
    auto window = ANativeWindow_fromSurface(env, surface);

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    PEN_ASSERT(display != EGL_NO_DISPLAY);
    EGLBoolean res = eglInitialize(display, nullptr, nullptr);
    PEN_ASSERT(res == EGL_TRUE);

    EGLint attr[] = {
        EGL_BUFFER_SIZE, 32,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_NONE
    };

    EGLint num_configs;
    EGLConfig config;
    res = eglChooseConfig(display, &attr[0], &config, 1, &num_configs);
    PEN_ASSERT(res == EGL_TRUE);

    EGLint ctx_attr[] = {
        EGL_CONTEXT_MAJOR_VERSION, 2,
        EGL_NONE
    };

    eglBindAPI(EGL_OPENGL_ES_API);
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctx_attr);
    PEN_ASSERT(context != EGL_NO_CONTEXT);

    EGLSurface egl_surface = eglCreateWindowSurface(display, config, window, nullptr);
    PEN_ASSERT(surface != EGL_NO_SURFACE);

    res = eglMakeCurrent(
            display,
            egl_surface,
            egl_surface,
            context
    );
    PEN_ASSERT(res == EGL_TRUE);

    s_egl_context.ctx = context;
    s_egl_context.display = display;
    s_egl_context.surface = egl_surface;

    // user setup
    pen::pen_creation_params params = pen::pen_entry(0, nullptr);

    // init renderer
    pen::renderer_init(nullptr, false, params.max_renderer_commands);

    pen::jobs_create_job(params.user_thread_function, 1024 * 1024, params.user_data, pen::e_thread_start_flags::detached);
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

    pen_error filesystem_read_file_to_buffer(const c8* filename, void** p_buffer, u32& buffer_size)
    {
        AAsset* asset = AAssetManager_open(s_android_context.m_asset_manager, filename, AASSET_MODE_STREAMING);

        if(asset)
        {
            off64_t length = AAsset_getLength64(asset) + 1;
            void* buf = pen::memory_alloc(length);

            AAsset_read(asset, buf, length - 1);
            AAsset_close(asset);
            u8* eof = (u8*)(buf) + (length - 1);
            *eof = '\0';

            buffer_size = length;
            *p_buffer = buf;
        }
        else
        {
            // check for abs files
            FILE* file = fopen(filename, "rb");
            if (file)
            {
                fseek(file, 0L, SEEK_END);
                size_t size = (u32)ftell(file);
                fseek(file, 0L, SEEK_SET);
                void* buf = pen::memory_alloc(size + 1);
                ((c8*)buf)[size] = '\0';
                fread(buf, 1, size, file);
                fclose(file);

                buffer_size = size;
                *p_buffer = buf;
            }
        }

        return PEN_ERR_OK;
    }

} // namespace pen
