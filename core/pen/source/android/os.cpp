// os.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "os.h"

#include "threads.h"
#include "renderer.h"
#include "timer.h"
#include "input.h"
#include "file_system.h"

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <android/keycodes.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <map>

#include <sys/stat.h>   // for mkdir()
#include <sys/types.h>  // for mode_t and related types

#undef stdin
#undef stdout
#undef stderr

FILE* stdin = NULL;
FILE* stdout = NULL;
FILE* stderr = NULL;

// google play
// legacy packaging

// BLOG NOTES:
// - gradle version, always changing, sdk etc bs bs bs
// - 50gb+ sdk install
// - random sdk manager --licenses
// - deprecated jcenter etc
// - trying to link .so vs .a
// - EGL_NONE, array terminator.
// - No implementation found for void cc.pmtech.pen_activity.entry() (tried Java_cc_pmtech_pen_1activity_entry and Java_cc_pmtech_pen_1activity_entry__) - is the library loaded, e.g. System.loadLibrary?
// - FMOD
// - Could not create task ':app:processDebugResources'.
// Cannot use @TaskAction annotation on method IncrementalTask.taskAction$gradle_core() because interface org.gradle.api.tasks.incremental.IncrementalTaskInputs is not a valid parameter to an action method.
// DEBUGGER INTERMITTENT HANG AND FAIL
// DEBUG INFO works better with device
// horrors of getting jvm methods, name mangling etc
// applicationId not in manifest
// install gradle. to build
// API access service account hell
// https://help.radio.co/en/articles/6232140-how-to-get-your-google-play-json-key

// ONHOLD
// backgrounding
// audio modes (pause / background etc)

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
// setup diig android build
// generate or create a wrapper for pen_activity + new manifest
// OSK
// openURL
// filesystem functions
// keychain / creds
// filesystem enum (dirent)
// vibrate

#define PEN_JNIFUNC(ret, actname, funcname) extern "C" JNIEXPORT ret JNICALL Java_cc_pmtech_##actname##_##funcname

// global externs
pen::user_info              pen_user_info;
pen::window_creation_params pen_window;

extern void audio_init_fmod_android(JNIEnv* env, jobject thiz, jobject activity);

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
        jobject         m_activity_object;
        jclass          m_activity_class;
    };
    android_context s_android_context;

    struct pmtech_context
    {
        pen::window_frame           window;
        pen::pen_creation_params    params;
        Str                         user_dir;
        Str                         cache_dir;
        bool                        keyboard_visible = false;
    };
    pmtech_context s_pmtech_context;

    std::map<int, virtual_key> s_key_map = {
        { AKEYCODE_0,                       PK_0            },
        { AKEYCODE_1,                       PK_1            },
        { AKEYCODE_2,                       PK_2            },
        { AKEYCODE_3,                       PK_3            },
        { AKEYCODE_4,                       PK_4            },
        { AKEYCODE_5,                       PK_5            },
        { AKEYCODE_6,                       PK_6            },
        { AKEYCODE_7,                       PK_7            },
        { AKEYCODE_8,                       PK_8            },
        { AKEYCODE_9,                       PK_9            },
        { AKEYCODE_A,                       PK_A            },
        { AKEYCODE_B,                       PK_B            },
        { AKEYCODE_C,                       PK_C            },
        { AKEYCODE_D,                       PK_D            },
        { AKEYCODE_E,                       PK_E            },
        { AKEYCODE_F,                       PK_F            },
        { AKEYCODE_G,                       PK_G            },
        { AKEYCODE_H,                       PK_H            },
        { AKEYCODE_I,                       PK_I            },
        { AKEYCODE_J,                       PK_J            },
        { AKEYCODE_K,                       PK_K            },
        { AKEYCODE_L,                       PK_L            },
        { AKEYCODE_M,                       PK_M            },
        { AKEYCODE_N,                       PK_N            },
        { AKEYCODE_O,                       PK_O            },
        { AKEYCODE_P,                       PK_P            },
        { AKEYCODE_Q,                       PK_Q            },
        { AKEYCODE_R,                       PK_R            },
        { AKEYCODE_S,                       PK_S            },
        { AKEYCODE_T,                       PK_T            },
        { AKEYCODE_U,                       PK_U            },
        { AKEYCODE_V,                       PK_V            },
        { AKEYCODE_W,                       PK_W            },
        { AKEYCODE_X,                       PK_X            },
        { AKEYCODE_Y,                       PK_Y            },
        { AKEYCODE_Z,                       PK_Z            },
        { AKEYCODE_NUMPAD_0,                PK_NUMPAD0      },
        { AKEYCODE_NUMPAD_1,                PK_NUMPAD1      },
        { AKEYCODE_NUMPAD_2,                PK_NUMPAD2      },
        { AKEYCODE_NUMPAD_3,                PK_NUMPAD3      },
        { AKEYCODE_NUMPAD_4,                PK_NUMPAD4      },
        { AKEYCODE_NUMPAD_5,                PK_NUMPAD5      },
        { AKEYCODE_NUMPAD_6,                PK_NUMPAD6      },
        { AKEYCODE_NUMPAD_7,                PK_NUMPAD7      },
        { AKEYCODE_NUMPAD_8,                PK_NUMPAD8      },
        { AKEYCODE_NUMPAD_9,                PK_NUMPAD9      },
        { AKEYCODE_NUMPAD_MULTIPLY,         PK_MULTIPLY      },
        { AKEYCODE_NUMPAD_ADD,              PK_ADD           },
        { AKEYCODE_NUMPAD_COMMA,            PK_SEPARATOR     },
        { AKEYCODE_NUMPAD_SUBTRACT,         PK_SUBTRACT      },
        { AKEYCODE_NUMPAD_DOT,              PK_DECIMAL       },
        { AKEYCODE_NUMPAD_DIVIDE,           PK_DIVIDE        },
        { AKEYCODE_F1,                      PK_F1            },
        { AKEYCODE_F2,                      PK_F2            },
        { AKEYCODE_F3,                      PK_F3            },
        { AKEYCODE_F4,                      PK_F4            },
        { AKEYCODE_F5,                      PK_F5            },
        { AKEYCODE_F6,                      PK_F6            },
        { AKEYCODE_F7,                      PK_F7            },
        { AKEYCODE_F8,                      PK_F8            },
        { AKEYCODE_F9,                      PK_F9            },
        { AKEYCODE_F10,                     PK_F10           },
        { AKEYCODE_F11,                     PK_F11           },
        { AKEYCODE_F12,                     PK_F12           },
        { AKEYCODE_DEL,                     PK_BACK          },
        { AKEYCODE_TAB,                     PK_TAB           },
        { AKEYCODE_CLEAR,                   PK_CLEAR         },
        { AKEYCODE_ENTER,                   PK_RETURN        },
        { AKEYCODE_SHIFT_LEFT,              PK_SHIFT         },
        { AKEYCODE_SHIFT_RIGHT,             PK_SHIFT         },
        { AKEYCODE_CTRL_LEFT,               PK_CONTROL       },
        { AKEYCODE_CTRL_RIGHT,              PK_CONTROL       },
        { AKEYCODE_ALT_LEFT,                PK_MENU          },
        { AKEYCODE_ALT_RIGHT,               PK_MENU          },
        { AKEYCODE_MEDIA_PAUSE,             PK_PAUSE         },
        { AKEYCODE_CAPS_LOCK,               PK_CAPITAL       },
        { AKEYCODE_ESCAPE,                  PK_ESCAPE        },
        { AKEYCODE_SPACE,                   PK_SPACE         },
        { AKEYCODE_MEDIA_PREVIOUS,          PK_PRIOR         },
        { AKEYCODE_MEDIA_NEXT,              PK_NEXT          },
        { AKEYCODE_MOVE_END,                PK_END           },
        { AKEYCODE_MOVE_HOME,               PK_HOME          },
        { AKEYCODE_SYSTEM_NAVIGATION_LEFT,  PK_LEFT          },
        { AKEYCODE_SYSTEM_NAVIGATION_UP,    PK_UP            },
        { AKEYCODE_SYSTEM_NAVIGATION_RIGHT, PK_RIGHT         },
        { AKEYCODE_SYSTEM_NAVIGATION_DOWN,  PK_DOWN          },
        { AKEYCODE_INSERT,                  PK_INSERT        },
        { AKEYCODE_FORWARD_DEL,             PK_DELETE        },
        { AKEYCODE_HELP,                    PK_HELP          },
        { AKEYCODE_NUM_LOCK,                PK_NUMLOCK       },
        { AKEYCODE_SCROLL_LOCK,             PK_SCROLL        },
        { AKEYCODE_MINUS,                   PK_MINUS         },
        { AKEYCODE_PLUS,                    PK_EQUAL         },
        { AKEYCODE_NUMPAD_LEFT_PAREN,       PK_OPEN_BRACKET  },
        { AKEYCODE_NUMPAD_RIGHT_PAREN,      PK_CLOSE_BRACKET },
        { AKEYCODE_SEMICOLON,               PK_SEMICOLON     },
        { AKEYCODE_BACKSLASH,               PK_BACK_SLASH    },
        { AKEYCODE_COMMA,                   PK_COMMA         },
        { AKEYCODE_PERIOD,                  PK_PERIOD        },
        { AKEYCODE_SLASH,                   PK_FORWARD_SLASH },
        { AKEYCODE_APOSTROPHE,              PK_APOSTRAPHE    },
        { AKEYCODE_GRAVE,                   PK_GRAVE         }
    };

    std::string codepoint_to_utf8(uint32_t cp) {
        std::string out;

        if (cp <= 0x7F) {
            // 1-byte sequence
            out.push_back(static_cast<char>(cp));
        }
        else if (cp <= 0x7FF) {
            // 2-byte sequence
            out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp <= 0xFFFF) {
            // 3-byte sequence
            out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp <= 0x10FFFF) {
            // 4-byte sequence
            out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }

        return out;
    }
}

extern "C" JNIEXPORT void JNICALL
Java_cc_pmtech_pen_1activity_init(JNIEnv* env, jobject thiz, jobject activity)
{
    // stubbed but left for extension later
    pen::timer_system_intialise();

    s_android_context.m_activity_object = env->NewGlobalRef((jobject)activity);
    s_android_context.m_activity_class = (jclass)env->NewGlobalRef(env->GetObjectClass(s_android_context.m_activity_object));

    audio_init_fmod_android(env, thiz, activity);
}

PEN_JNIFUNC(void, pen_1activity, native_1on_1key_1down)(JNIEnv* env, jclass thiz, int key_code, int unicode_char)
{
    if(s_key_map.find(key_code) != s_key_map.end())
    {
        auto pk = s_key_map[key_code];
        pen::input_set_key_down(pk);
    }

    if(unicode_char != 0)
    {
        auto utf8 = codepoint_to_utf8(unicode_char);
        pen::input_add_unicode_input(utf8.c_str());
    }
}

PEN_JNIFUNC(void, pen_1activity, native_1on_1key_1up)(JNIEnv* env, jclass thiz, int key_code)
{
    if(s_key_map.find(key_code) != s_key_map.end())
    {
        auto pk = s_key_map[key_code];
        if(pk != PK_BACK && pk != PK_RETURN) // these are handled specially
            pen::input_set_key_up(pk);
    }
}

PEN_JNIFUNC(void, pen_1activity, native_1back_1button_1pressed)(JNIEnv* env, jclass thiz)
{
    // ??
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    s_android_context.m_java_vm = vm;
    return JNI_VERSION_1_6;
}

void pen_make_gl_context_current()
{
    // stub
}

void pen_gl_swap_buffers()
{
    eglSwapBuffers(s_egl_context.display, s_egl_context.surface);
}

PEN_JNIFUNC(void, pen_1activity, register_1asset_1manager)(JNIEnv* env, jclass thiz, jobject asset_manager)
{
    s_android_context.m_asset_manager = AAssetManager_fromJava(env, asset_manager);
}

PEN_JNIFUNC(void, pen_1activity, set_1persistent_1data_1dir)(JNIEnv* env, jobject thiz, jstring persistent_data_dir)
{
    jboolean iscopy;
    s_pmtech_context.user_dir = env->GetStringUTFChars(persistent_data_dir, &iscopy);
}

PEN_JNIFUNC(void, pen_1activity, set_1cache_1dir)(JNIEnv* env, jobject thiz, jstring cache_dir)
{
    jboolean iscopy;
    s_pmtech_context.cache_dir = env->GetStringUTFChars(cache_dir, &iscopy);
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

    static bool setup = true;
    if(setup)
    {
        // user setup
        s_pmtech_context.params = pen::pen_entry(0, nullptr);

        // init renderer
        pen::renderer_init(nullptr, false, s_pmtech_context.params.max_renderer_commands);

        pen::jobs_create_job(s_pmtech_context.params.user_thread_function,
            1024 * 1024, s_pmtech_context.params.user_data,
            pen::e_thread_start_flags::detached);

        setup = false;
    }
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
    JNIEnv* get_jni_env()
    {
        if (!s_android_context.m_java_vm)
            return nullptr;

        JNIEnv* env;
        int status = s_android_context.m_java_vm->GetEnv((void**)&env, JNI_VERSION_1_4);
        if (status == JNI_EDETACHED)
        {
            if (s_android_context.m_java_vm->AttachCurrentThread(&env, nullptr) != 0)
                return nullptr;
        }
        else if (status != JNI_OK)
        {
            return nullptr;
        }
        return env;
    }

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
        // stub
    }

    void os_set_cursor_pos(u32 client_x, u32 client_y)
    {
        
    }

    const user_info& os_get_user_info()
    {
        // TODO:
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
        return s_pmtech_context.user_dir.c_str();
    }

    Str os_get_cache_data_directory()
    {
        return s_pmtech_context.cache_dir.c_str();
    }
    
    void os_create_directory(const Str& dir)
    {
        auto env = get_jni_env();
        if(env)
        {
            jstring js = env->NewStringUTF(dir.c_str());
            jmethodID method = env->GetStaticMethodID(s_android_context.m_activity_class, "createDirectory", "(Ljava/lang/String;)V");
            env->CallStaticVoidMethod(s_android_context.m_activity_class, method, js);
        }
    }

    bool os_delete_directory(const Str& filename)
    {
        auto env = get_jni_env();
        if(env)
        {
            jstring js = env->NewStringUTF(filename.c_str());
            jmethodID method = env->GetStaticMethodID(s_android_context.m_activity_class, "deleteDirectory", "(Ljava/lang/String;)V");
            env->CallStaticVoidMethod(s_android_context.m_activity_class, method, js);
        }
    }

    void os_open_url(const Str& url)
    {
        auto env = get_jni_env();
        if(env)
        {
            jstring js = env->NewStringUTF(url.c_str());
            jmethodID method = env->GetStaticMethodID(s_android_context.m_activity_class, "openURL", "(Ljava/lang/String;)V");
            env->CallStaticVoidMethod(s_android_context.m_activity_class, method, js);
        }
    }

    void os_ignore_slient()
    {
        // stub
    }

    void os_enable_background_audio(bool enabled)
    {
        // stub
    }

    f32 os_get_status_bar_portrait_height()
    {
        auto env = get_jni_env();
        if(env)
        {
            jmethodID method = env->GetMethodID(s_android_context.m_activity_class, "getStatusBarHeight", "()I");
            int res = env->CallIntMethod(s_android_context.m_activity_object, method);
            return (f32)res;
        }

        return 0.0f;
    }

    void os_haptic_selection_feedback()
    {
        auto env = get_jni_env();
        if(env)
        {
            jmethodID method = env->GetMethodID(s_android_context.m_activity_class, "triggerVibration", "()V");
            env->CallVoidMethod(s_android_context.m_activity_object, method);
        }
    }

    void os_init_on_screen_keyboard()
    {
        auto env = get_jni_env();
        if(env)
        {
            jmethodID method = env->GetStaticMethodID(s_android_context.m_activity_class, "showKeyboard", "(Z)V");
            env->CallStaticVoidMethod(s_android_context.m_activity_class, method, false);
        }
    }

    void os_show_on_screen_keyboard(bool show)
    {
        if(s_pmtech_context.keyboard_visible == show)
            return;

        auto env = get_jni_env();
        if(env)
        {
            jmethodID method = env->GetStaticMethodID(s_android_context.m_activity_class, "showKeyboard", "(Z)V");
            env->CallStaticVoidMethod(s_android_context.m_activity_class, method, show);

            s_pmtech_context.keyboard_visible = show;
        }
    }

    bool os_set_keychain_item(const Str& identifier, const Str& key, const Str& value)
    {
        auto env = get_jni_env();
        if(env)
        {
            jstring jkey = env->NewStringUTF(key.c_str());
            jstring jval = env->NewStringUTF(value.c_str());

            jmethodID method = env->GetMethodID(s_android_context.m_activity_class, "setCredential", "(Ljava/lang/String;Ljava/lang/String;)Z");
            bool result =  env->CallBooleanMethod(s_android_context.m_activity_object, method, jkey, jval);

            return result;
        }

        return false;
    }

    Str os_get_keychain_item(const Str& identifier, const Str& key)
    {
        auto env = get_jni_env();
        if(env)
        {
            jstring jkey = env->NewStringUTF(key.c_str());
            jmethodID method = env->GetMethodID(s_android_context.m_activity_class, "getCredential", "(Ljava/lang/String;)Ljava/lang/String;");
            jstring result = (jstring)env->CallObjectMethod(s_android_context.m_activity_object, method, jkey);

            const char* result_cstr = env->GetStringUTFChars(result, nullptr);
            Str return_result = result_cstr;

            env->ReleaseStringUTFChars(result, result_cstr);

            return return_result;
        }

        return "";
    }

    bool os_is_backgrounded()
    {
        return false;
    }

    void os_register_background_callback(void (*callback)(bool))
    {

    }

    bool os_require_audio_reinit(bool reset)
    {
        return false;
    }

    // music

    const music_item* music_get_items()
    {
        return nullptr;
    }

    music_file music_open_file(const music_item& item)
    {
        return {};
    }

    void music_close_file(const music_file& file)
    {
        // stub
    }

    void music_enable_remote_control(const music_player_remote& fns)
    {
        // stub
    }

    void music_set_now_playing(const Str& artist, const Str& album, const Str& track)
    {
        // stub
    }

    void music_set_now_playing_artwork(void* data, u32 w, u32 h, u32 bpp, u32 row_pitch)
    {
        // stub
    }

    void music_set_now_playing_time_info(u32 position_ms, u32 duration_ms)
    {
        // stub
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

        u32 tt = 0;
        return pen::filesystem_getmtime(filename, tt) != PEN_ERR_FILE_NOT_FOUND;
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

        FILE* file = fopen(filename, "rb");
        if (file)
        {
            fseek(file, 0L, SEEK_END);
            size_t size = (u32)ftell(file);
            fclose(file);

            return size;
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

    Str os_get_clipboard_string()
    {
        auto env = get_jni_env();
        if(env)
        {
            jmethodID method = env->GetMethodID(s_android_context.m_activity_class, "getClipboardString", "()Ljava/lang/String;");
            auto result = (jstring)env->CallObjectMethod(s_android_context.m_activity_object, method);

            const char* result_cstr = env->GetStringUTFChars(result, nullptr);
            Str return_result = result_cstr;

            env->ReleaseStringUTFChars(result, result_cstr);

            return return_result;
        }

        return "";
    }

    void os_clear_clipboard_string()
    {
        auto env = get_jni_env();
        if(env)
        {
            jmethodID method = env->GetMethodID(s_android_context.m_activity_class, "clearClipboardString", "()V");
            env->CallVoidMethod(s_android_context.m_activity_object, method);
        }
    }

    void os_enable_paste_popup(bool enable)
    {
        auto env = get_jni_env();
        if(env)
        {
            jboolean jenable = enable;

            jmethodID method = env->GetMethodID(s_android_context.m_activity_class, "enablePaste", "(Z)V");
            env->CallVoidMethod(s_android_context.m_activity_object, method, jenable);
        }
    }

    bool os_tapped()
    {
        auto env = get_jni_env();
        if(env)
        {
            jmethodID method = env->GetMethodID(s_android_context.m_activity_class, "wasTapped", "()Z");
            return env->CallBooleanMethod(s_android_context.m_activity_object, method);
        }
    }

} // namespace pen
