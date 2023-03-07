// os.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md
#include "GL/glew.h"

#include "console.h"
#include "hash.h"
#include "input.h"
#include "os.h"
#include "pen.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"
#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/Xlib.h>

using namespace pen;

// pen required externs
window_creation_params pen_window;
pen::user_info         pen_user_info;

// glx / gl stuff
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_FLAGS_ARB 0x2094
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

static int visual_attribs[] = {GLX_X_RENDERABLE, True, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_RENDER_TYPE, GLX_RGBA_BIT,
                               GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                               GLX_ALPHA_SIZE, 8, GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8, GLX_DOUBLEBUFFER, True,
                               // GLX_SAMPLE_BUFFERS  , 1,
                               // GLX_SAMPLES         , 4,
                               None};

GLXContext _gl_context = 0;
Display*   _display;
Window     _window;

// externs for the gl implementation
void pen_make_gl_context_current()
{
    glXMakeCurrent(_display, _window, _gl_context);
}

void pen_gl_swap_buffers()
{
    glXSwapBuffers(_display, _window);
}

namespace
{
    XIM          _xim;
    XIC          _xic;
    bool         _ctx_error_occured = false;
    window_frame _window_frame;
    bool         _invalidate_window_frame = false;

    u32                 s_error_code = 0;
    bool                s_pen_terminate_app = false;
    bool                s_windowed = false;
    pen_creation_params s_creation_params;

    void users()
    {
        static struct passwd* pw = getpwuid(getuid());
        const char*           homedir = pw->pw_dir;
        pen_user_info.user_name = &homedir[6];
    }

    int ctx_error_handler(Display* dpy, XErrorEvent* ev)
    {
        PEN_LOG("CONTEXT ERROR %i", ev->error_code);
        _ctx_error_occured = true;
        return 0;
    }

    int pen_run_windowed(int argc, char** argv)
    {
        Visual*              visual;
        int                  depth;
        XSetWindowAttributes frame_attributes;

        _display = XOpenDisplay(NULL);
        visual = DefaultVisual(_display, 0);
        depth = DefaultDepth(_display, 0);

        // Check glx version
        s32 glx_major, glx_minor = 0;
        glXQueryVersion(_display, &glx_major, &glx_minor);

        // glx setup
        const char* glxExts = glXQueryExtensionsString(_display, DefaultScreen(_display));

        _ctx_error_occured = false;
        int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&ctx_error_handler);

        // find fb with matching samples
        s32          fbcount;
        s32          chosen_fb = 0;
        GLXFBConfig* fbc = glXChooseFBConfig(_display, DefaultScreen(_display), visual_attribs, &fbcount);
        for (int i = 0; i < fbcount; ++i)
        {
            XVisualInfo* vi = glXGetVisualFromFBConfig(_display, fbc[i]);

            int samp_buf, samples;
            glXGetFBConfigAttrib(_display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf);
            glXGetFBConfigAttrib(_display, fbc[i], GLX_SAMPLES, &samples);

            if (samples == pen_window.sample_count)
            {
                chosen_fb = i;
                break;
            }
        }

        GLXFBConfig best_fbc = fbc[chosen_fb];

        // Create window
        XVisualInfo* vi = glXGetVisualFromFBConfig(_display, best_fbc);

        XSetWindowAttributes swa;
        Colormap             cmap;
        swa.colormap = cmap = XCreateColormap(_display, RootWindow(_display, vi->screen), vi->visual, AllocNone);
        swa.background_pixmap = None;
        swa.border_pixel = 0;
        swa.event_mask = StructureNotifyMask;

        _window = XCreateWindow(_display, RootWindow(_display, vi->screen), 0, 0, pen_window.width, pen_window.height, 0,
                                vi->depth, InputOutput, vi->visual, CWBorderPixel | CWColormap | CWEventMask, &swa);

        XStoreName(_display, _window, pen_window.window_title);
        XSelectInput(_display, _window,
                     ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask |
                         KeyReleaseMask | ConfigureNotify);

        // Create Gl Context
        glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
        glXCreateContextAttribsARB =
            (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

        // Pmtech supports minimum OpenGL 3.1, just try brute force in reverse order to get the highest
        for (s32 major = 4; major > 2; --major)
        {
            for (s32 minor = 6; minor >= 0; --minor)
            {
                int context_attribs[] = {GLX_CONTEXT_MAJOR_VERSION_ARB, major, GLX_CONTEXT_MINOR_VERSION_ARB, minor, None};

                _gl_context = glXCreateContextAttribsARB(_display, best_fbc, 0, True, context_attribs);
                if (_gl_context)
                    goto found_context;
            }
        }
        if (_ctx_error_occured || !_gl_context)
        {
            PEN_LOG("ERROR: OpenGL 3.1+ Context Failed to create");
            return 1;
        }
    found_context:
        XMapWindow(_display, _window);

        // obtain input context
        _xim = XOpenIM(_display, NULL, NULL, NULL);
        if (_xim == NULL)
        {
            PEN_LOG("ERROR: Could not open input method\n");
            return 1;
        }

        XIMStyles* styles;
        XIMStyle   xim_requested_style;
        char*      failed_arg;
        failed_arg = XGetIMValues(_xim, XNQueryInputStyle, &styles, NULL);
        if (failed_arg != NULL)
        {
            PEN_LOG("ERROR: XIM Can't get styles\n");
            return 1;
        }

        _xic = XCreateIC(_xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, _window, NULL);
        if (_xic == NULL)
        {
            printf("ERROR: Could not open IC\n");
            return 1;
        }

        XSetICFocus(_xic);

        // Make current and init glew
        pen_make_gl_context_current();

        glewExperimental = true;
        GLenum err = glewInit();
        if (err != GLEW_OK)
        {
            PEN_LOG("ERROR: glewInit failed: %s\n", glewGetErrorString(err));
            return 1;
        }

        // enable vysnc.. we could pass this into pen create params
        PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = 0;
        glXSwapIntervalEXT = (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddress((const GLubyte*)"glXSwapIntervalEXT");
        if (glXSwapIntervalEXT)
            glXSwapIntervalEXT(_display, _window, 1);

        s_windowed = true;

        if(argc > 1)
        {
            if(strcmp(argv[1],"-test") == 0)
            {
                pen::renderer_test_enable();
            }
        }

        // inits renderer and loops in wait for jobs, calling os update
        renderer_init(nullptr, true, s_creation_params.max_renderer_commands);

        // exit, kill other threads and wait
        pen::jobs_terminate_all();

        XDestroyWindow(_display, _window);
        XCloseDisplay(_display);

        return s_error_code;
    }

    int pen_run_console_app()
    {
        for (;;)
        {
            if (!pen::os_update())
                break;

            pen::thread_sleep_us(100);
        }

        pen::jobs_terminate_all();
        return s_error_code;
    }

    s32 translate_mouse_button(s32 b)
    {
        static f32 mw = 0.0f;
        switch (b)
        {
            case 1:
                return 0;
            case 3:
                return 1;
            case 2:
                return 2;
            case 4:
                pen::input_set_mouse_wheel(1.0f);
                break;
            case 5:
                pen::input_set_mouse_wheel(-1.0f);
                break;
        };

        return -1;
    }

    u32 translate_key_sym(u32 k)
    {
        switch (k)
        {
            // Misc
            case XK_KP_Space:
                return PK_SPACE;
            case XK_BackSpace:
                return PK_BACK;
            case XK_Tab:
                return PK_TAB;
            case XK_Linefeed:
                break; //?
            case XK_Clear:
                return PK_CLEAR;
            case XK_Return:
                return PK_RETURN;
            case XK_Pause:
                return PK_PAUSE;
            case XK_Scroll_Lock:
                return PK_SCROLL;
            case XK_Sys_Req:
                break; //?
            case XK_Escape:
                return PK_ESCAPE;
            case XK_Delete:
                return PK_DELETE;
            case XK_Home:
                return PK_HOME;
            case XK_Left:
                return PK_LEFT;
            case XK_Up:
                return PK_UP;
            case XK_Right:
                return PK_RIGHT;
            case XK_Down:
            ERROR:
                return PK_DOWN;
            case XK_Prior:
                return PK_PRIOR;
            case XK_Next:
                return PK_NEXT;
            case XK_Num_Lock:
                return PK_NUMLOCK;
            case XK_semicolon:
                return PK_SEMICOLON;
            case XK_comma:
                return PK_COMMA;
            case XK_period:
                return PK_PERIOD;
            case XK_dead_tilde:
                return PK_TILDE;
            case XK_equal:
                return PK_EQUAL;
            case XK_minus:
                return PK_MINUS;
            case XK_backslash:
                return PK_BACK_SLASH;
            case XK_bracketleft:
                return PK_OPEN_BRACKET;
            case XK_bracketright:
                return PK_CLOSE_BRACKET;
            case XK_grave:
                return PK_GRAVE;
            case XK_apostrophe:
                return PK_APOSTRAPHE;
            case 47:
                return PK_FORWARD_SLASH;
            case XK_Insert:
                return PK_INSERT;
            case 65367:
                return PK_END;

            // F
            case XK_F1:
                return PK_F1;
            case XK_F2:
                return PK_F2;
            case XK_F3:
                return PK_F3;
            case XK_F4:
                return PK_F4;
            case XK_F5:
                return PK_F5;
            case XK_F6:
                return PK_F6;
            case XK_F7:
                return PK_F7;
            case XK_F8:
                return PK_F8;
            case XK_F9:
                return PK_F9;
            case XK_F10:
                return PK_F10;
            case XK_F11:
                return PK_F11;
            case XK_F12:
                return PK_F12;

            // Modifiers
            case XK_Shift_L:
                return PK_SHIFT;
            case XK_Shift_R:
                return PK_SHIFT;
            case XK_Control_L:
                return PK_CONTROL;
            case XK_Control_R:
                return PK_CONTROL;
            case XK_Caps_Lock:
                return PK_CAPITAL;
            case XK_Alt_L:
                return PK_MENU;
            case XK_Alt_R:
                return PK_MENU;

            // Keypad
            case XK_KP_Equal:
                return PK_ADD;
            case XK_KP_Multiply:
                return PK_MULTIPLY;
            case XK_KP_Add:
                return PK_ADD;
            case XK_KP_Separator:
                return PK_SEPARATOR;
            case XK_KP_Subtract:
                return PK_SUBTRACT;
            case XK_KP_Decimal:
                return PK_DECIMAL;
            case XK_KP_Divide:
                return PK_DIVIDE;
        };

        // numerical keys
        if (k >= XK_0 && k <= XK_9)
            return PK_0 + (k - XK_0);

        if (k >= XK_a && k <= XK_z)
            return PK_A + (k - XK_a);

        if (k >= XK_A && k <= XK_Z)
            return PK_A + (k - XK_A);

        return k;
    }

    void update_window()
    {
        while (XPending(_display) > 0)
        {
            XEvent event;
            XNextEvent(_display, (XEvent*)&event);
            switch (event.type)
            {
                case Expose:
                case ConfigureNotify:
                {
                    XWindowAttributes attribs;
                    XGetWindowAttributes(_display, _window, &attribs);
                    pen_window.width = attribs.width;
                    pen_window.height = attribs.height;

                    Window unused;

                    XTranslateCoordinates(_display, _window, XDefaultRootWindow(_display), 0, 0, (int*)&_window_frame.x,
                                          (int*)&_window_frame.y, &unused);

                    _window_frame.width = attribs.width;
                    _window_frame.height = attribs.height;
                }
                break;
                case KeyPress:
                {
                    static c8 buf[255];
                    KeySym    k;
                    Status    status;

                    // todo for utf8
                    Xutf8LookupString(_xic, &event.xkey, buf, sizeof(buf) - 1, &k, &status);
                    //XLookupString(&event.xkey, buf, 255, &k, 0);
                    pen::input_add_unicode_input(buf);

                    u32 ks = translate_key_sym(k);
                    pen::input_set_key_down(ks);
                }
                break;
                case KeyRelease:
                {
                    static c8 buf[255];
                    KeySym    k;

                    XLookupString(&event.xkey, buf, 255, &k, 0);
                    pen::input_set_unicode_key_up(buf[0]);

                    u32 ks = translate_key_sym(k);
                    pen::input_set_key_up(ks);
                }
                break;
                case ButtonPress:
                {
                    s32 mb = translate_mouse_button(event.xbutton.button);
                    if (mb != -1)
                        pen::input_set_mouse_down(mb);
                }
                break;
                case ButtonRelease:
                {
                    s32 mb = translate_mouse_button(event.xbutton.button);
                    if (mb != -1)
                        pen::input_set_mouse_up(mb);
                }
                break;
            }
        }

        // update window
        if (_invalidate_window_frame)
        {
            XMoveResizeWindow(_display, _window, (int)_window_frame.x, (int)_window_frame.y, (int)_window_frame.width,
                              (int)_window_frame.height);
            _invalidate_window_frame = false;
        }

        // Update cursor
        int          root_x, root_y;
        int          win_x, win_y;
        unsigned int mask_return;
        Window       window_returned;

        int result = XQueryPointer(_display, _window, &window_returned, &window_returned, &root_x, &root_y, &win_x, &win_y,
                                   &mask_return);
        pen::input_set_mouse_pos((f32)win_x, (f32)win_y);

        pen::input_gamepad_update();
    }
} // namespace

int main(int argc, char* argv[])
{
    // initilaise any generic systems
    users();
    timer_system_intialise();
    input_gamepad_init();

    pen::pen_creation_params pc = pen::pen_entry(argc, argv);
    pen_window.width = pc.window_width;
    pen_window.height = pc.window_height;
    pen_window.window_title = pc.window_title;
    pen_window.sample_count = pc.window_sample_count;
    s_creation_params = pc;

    if (pc.flags & e_pen_create_flags::renderer)
    {
        pen_run_windowed(argc, argv);
    }
    else
    {
        pen_run_console_app();
    }

    return s_error_code;
}

namespace pen
{
    const Str os_path_for_resource(const c8* filename)
    {
        Str fn = filename;
        return fn;
    }

    bool os_update()
    {
        static bool init_jobs = false;
        if (!init_jobs)
        {
            auto& pcp = s_creation_params;
            jobs_create_job(pcp.user_thread_function, 1024 * 1024, pcp.user_data, pen::e_thread_start_flags::detached);
            init_jobs = true;
        }

        if (s_windowed)
            update_window();

        // Check for terminate and poll terminated jobs
        if (s_pen_terminate_app)
        {
            if (pen::jobs_terminate_all())
                return false;
        }

        return true;
    }

    void os_terminate(u32 error_code)
    {
        s_error_code = error_code;
        s_pen_terminate_app = true;
    }

    u32 window_init(void* params)
    {
        return 0;
    }

    void* window_get_primary_display_handle()
    {
        return (void*)(intptr_t)_window;
    }

    void window_get_size(s32& width, s32& height)
    {
        width = pen_window.width;
        height = pen_window.height;
    }

    const c8* window_get_title()
    {
        return pen_window.window_title;
    }

    hash_id window_get_id()
    {
        static hash_id window_id = PEN_HASH(pen_window.window_title);
        return window_id;
    }

    f32 window_get_aspect()
    {
        return (f32)pen_window.width / (f32)pen_window.height;
    }

    void window_get_frame(window_frame& f)
    {
        f = _window_frame;
    }

    void window_set_frame(const window_frame& f)
    {
    }

    void os_set_cursor_pos(u32 client_x, u32 client_y)
    {
    }

    void os_show_cursor(bool show)
    {
    }

    bool input_undo_pressed()
    {
        return pen::input_key(PK_CONTROL) && pen::input_key(PK_Z);
    }

    bool input_redo_pressed()
    {
        return pen::input_key(PK_CONTROL) && pen::input_get_unicode_key('Y');
    }

    const user_info& os_get_user_info()
    {
        return pen_user_info;
    }
} // namespace pen