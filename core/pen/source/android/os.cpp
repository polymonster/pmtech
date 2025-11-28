// os.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "os.h"

#include "threads.h"
#include "renderer.h"
#include "timer.h"
#include "input.h"

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#undef stdin
#undef stdout
#undef stderr

FILE* stdin = NULL;
FILE* stdout = NULL;
FILE* stderr = NULL;

// setup diig android build
// filesystem functions
// openURL etc

// BLOG NOTES:
// - gradle version, always changing, sdk etc bs bs bs
// - deprecated jcenter etc
// - trying to link .so vs .a
// - EGL_NONE, array terminator.
// - No implementation found for void cc.pmtech.pen_activity.entry() (tried Java_cc_pmtech_pen_1activity_entry and Java_cc_pmtech_pen_1activity_entry__) - is the library loaded, e.g. System.loadLibrary?
// - 50gb+ sdk install
// - random sdk manager --licenses
// - FMOD
// - Could not create task ':app:processDebugResources'.
// Cannot use @TaskAction annotation on method IncrementalTask.taskAction$gradle_core() because interface org.gradle.api.tasks.incremental.IncrementalTaskInputs is not a valid parameter to an action method.
// DEBUGGER INTERMITTENT HANG AND FAIL
// DEBUG INFO works better with device

// DONE:
// call c++ from java
// need to copy fmod.jar and add it as impl in gradle
// build shaders
// setup assetdirs
// asset manager
// need to copy example/src/main/jniLibs (fmod)
// inject strings for other samples res/values
// windows setup
// viewport and window sizes
// touch input events
// orientation changes
// debug info? on device
// sort out fmod version

#define PEN_JNIFUNC(ret, actname, funcname) extern "C" JNIEXPORT ret JNICALL Java_cc_pmtech_##actname##_##funcname

// global externs
pen::user_info              pen_user_info;
pen::window_creation_params pen_window;

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    return JNI_VERSION_1_6;
}

extern void audio_init_fmod_android(JNIEnv* env, jobject thiz, jobject activity);

extern "C" JNIEXPORT void JNICALL
Java_cc_pmtech_pen_1activity_initFMOD(JNIEnv* env, jobject thiz, jobject activity)
{
    audio_init_fmod_android(env, thiz, activity);
}

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

    struct pmtech_context
    {
        pen::window_frame           window;
        pen::pen_creation_params    params;
        Str                         user_dir;
    };
    pmtech_context s_pmtech_context;
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
    // stubbed but left for extension later
    pen::timer_system_intialise();
}

PEN_JNIFUNC(void, pen_1activity, register_1asset_1manager)(JNIEnv* env, jclass thiz, jobject asset_manager)
{
    s_android_context.m_asset_manager = AAssetManager_fromJava(env, asset_manager);
}


PEN_JNIFUNC(void, pen_1activity, set_1persistent_1data_1dir)(JNIEnv* env, jobject thiz, jstring cache_dir)
{
    jboolean iscopy;
    s_pmtech_context.user_dir = env->GetStringUTFChars(cache_dir, &iscopy);
}

PEN_JNIFUNC(void, SurfaceWrapper, render)(JNIEnv* env, jclass thiz, jobject caller)
{
    pen::os_update();
    pen::renderer_dispatch();
}

PEN_JNIFUNC(void, SurfaceWrapper, surface_1created)(JNIEnv* env, jclass thiz, jobject surface, int window_width, int window_height, int device_width, int device_height, int orientation, long app_ptr)
{
    // set window info
    s_pmtech_context.window.x = 0;
    s_pmtech_context.window.y = 0;
    s_pmtech_context.window.width = window_width;
    s_pmtech_context.window.height = window_height;

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
    s_pmtech_context.params = pen::pen_entry(0, nullptr);

    // init renderer
    pen::renderer_init(nullptr, false, s_pmtech_context.params.max_renderer_commands);

    pen::jobs_create_job(s_pmtech_context.params.user_thread_function,
                         1024 * 1024, s_pmtech_context.params.user_data,
                         pen::e_thread_start_flags::detached);
}


PEN_JNIFUNC(void, SurfaceWrapper, surface_1changed)(JNIEnv* env, jclass thiz, int width, int height)
{
    s_pmtech_context.window.width = width;
    s_pmtech_context.window.height = height;
}

PEN_JNIFUNC(void, SurfaceWrapper, on_1touch_1down)(JNIEnv* env, jclass thiz, int id, float x, float y, float pressure,
    float majoraxis, float minoraxis, float angle)
{
    pen::input_set_mouse_down(PEN_MOUSE_L);
    pen::input_set_mouse_pos(x, y);
}

PEN_JNIFUNC(void, SurfaceWrapper, on_1touch_1moved)(JNIEnv* env, jclass thiz, int id, float x, float y, float pressure,
    float majoraxis, float minoraxis, float angle)
{
    pen::input_set_mouse_pos(x, y);
}

PEN_JNIFUNC(void, SurfaceWrapper, on_1touch_1up)(JNIEnv* env, jclass thiz, int id, float x, float y, float pressure,
    float majoraxis, float minoraxis, float angle)
{
    pen::input_set_mouse_up(PEN_MOUSE_L);
    pen::input_set_mouse_pos(x, y);
}

PEN_JNIFUNC(void, SurfaceWrapper, on_1touch_1cancelled)(JNIEnv* env, jclass thiz, int id, float x, float y)
{
    pen::input_set_mouse_up(PEN_MOUSE_L);
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
        return s_pmtech_context.params.window_title;
    }

    void* window_get_primary_display_handle()
    {
        return nullptr;
    }

    void window_get_frame(window_frame& f)
    {
        f = s_pmtech_context.window;
    }

    void window_set_frame(const window_frame& f)
    {
        s_pmtech_context.window = f;
    }

    void window_get_size(s32& width, s32& height)
    {
        width = s_pmtech_context.window.width;
        height = s_pmtech_context.window.height;
    }

    void window_set_size(s32 width, s32 height)
    {
        s_pmtech_context.window.width = width;
        s_pmtech_context.window.height = height;
    }

    f32 window_get_aspect()
    {
        return (f32)s_pmtech_context.window.width / (f32)s_pmtech_context.window.height;
    }

    const Str os_path_for_resource(const c8* filename)
    {
        Str prefix ="file:///android_asset/";
        prefix.append(filename);

        return prefix;
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

    Str os_get_persistent_data_directory()
    {

    }

    Str os_get_cache_data_directory()
    {

    }
    
    void os_create_directory(const Str& dir)
    {

    }

    bool os_delete_directory(const Str& filename)
    {

    }

    void os_open_url(const Str& url)
    {

    }

    void os_ignore_slient()
    {

    }

    void os_enable_background_audio(bool enabled)
    {

    }

    f32 os_get_status_bar_portrait_height()
    {

    }

    void os_haptic_selection_feedback()
    {

    }

    void os_init_on_screen_keyboard()
    {

    }

    void os_show_on_screen_keyboard(bool show)
    {

    }

    bool os_set_keychain_item(const Str& identifier, const Str& key, const Str& value)
    {

    }

    Str os_get_keychain_item(const Str& identifier, const Str& key)
    {

    }

    bool os_is_backgrounded()
    {

    }

    void os_register_background_callback(void (*callback)(bool))
    {

    }

    bool os_require_audio_reinit(bool reset)
    {
        
    }

    // music

    const music_item* music_get_items()
    {

    }

    music_file music_open_file(const music_item& item)
    {

    }

    void music_close_file(const music_file& file)
    {
        
    }

    void music_enable_remote_control(const music_player_remote& fns)
    {
        
    }

    void music_set_now_playing(const Str& artist, const Str& album, const Str& track)
    {
        
    }

    void music_set_now_playing_artwork(void* data, u32 w, u32 h, u32 bpp, u32 row_pitch)
    {
        
    }

    void music_set_now_playing_time_info(u32 position_ms, u32 duration_ms)
    {
        
    }

    // filesystem 

    const c8* filesystem_get_user_directory()
    {
        return s_pmtech_context.user_dir.c_str();
    }

    bool filesystem_file_exists(const c8* filename)
    {
        AAsset* asset = AAssetManager_open(s_android_context.m_asset_manager, filename, AASSET_MODE_STREAMING);
        
        if(asset)
        {
            AAsset_close(asset);
            return true;
        }

        return false;
    }

    size_t filesystem_getsize(const c8* filename)
    {
        AAsset* asset = AAssetManager_open(s_android_context.m_asset_manager, filename, AASSET_MODE_STREAMING);
        if(asset)
        {
            off64_t length = AAsset_getLength64(asset);
            AAsset_close(asset);

            return length;
        }

        return 0;
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
