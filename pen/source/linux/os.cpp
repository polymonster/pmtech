#include "GL/glew.h"

#include "console.h"
#include "input.h"
#include "os.h"
#include "pen.h"
#include "threads.h"
#include "timer.h"
#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <GL/glx.h>
#include <X11/Xlib.h>

using namespace pen;

namespace pen
{
    extern void renderer_init(void* user_data, bool wait_for_jobs);
}

// pen required externs
extern window_creation_params pen_window;
pen::user_info                pen_user_info;

// glx / gl stuff
#define GLX_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB 0x2092
#define GLX_CONTEXT_FLAGS_ARB 0x2094
#define GLX_CONTEXT_PROFILE_MASK_ARB 0x9126
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
GLXContext _gl_context = 0;
Display*   _display;
Window     _window;

void pen_make_gl_context_current()
{
    glXMakeCurrent(_display, _window, _gl_context);
}

void pen_gl_swap_buffers()
{
    glXSwapBuffers(_display, _window);
}

static int visual_attribs[] = {GLX_X_RENDERABLE, True, GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, GLX_RENDER_TYPE, GLX_RGBA_BIT,
                               GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
                               GLX_ALPHA_SIZE, 8, GLX_DEPTH_SIZE, 24, GLX_STENCIL_SIZE, 8, GLX_DOUBLEBUFFER, True,
                               // GLX_SAMPLE_BUFFERS  , 1,
                               // GLX_SAMPLES         , 4,
                               None};

static bool ctx_error_occured = false;
static int  ctx_error_handler(Display* dpy, XErrorEvent* ev)
{
    PEN_PRINTF("context error %i", ev->error_code);
    ctx_error_occured = true;
    return 0;
}

void users()
{
    static struct passwd *pw = getpwuid(getuid());
    const char *homedir = pw->pw_dir;

    pen_user_info.user_name = &homedir[6];:98
    PEN_PRINTF(pen_user_info.user_name);
}

int main(int argc, char* argv[])
{
    Visual*              visual;
    int                  depth;
    XSetWindowAttributes frame_attributes;

    _display = XOpenDisplay(NULL);
    visual   = DefaultVisual(_display, 0);
    depth    = DefaultDepth(_display, 0);

    // Check glx version
    s32 glx_major, glx_minor = 0;
    glXQueryVersion(_display, &glx_major, &glx_minor);

    // glx setup
    const char* glxExts = glXQueryExtensionsString(_display, DefaultScreen(_display));

    ctx_error_occured                         = false;
    int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&ctx_error_handler);

    // find fb with matching samples
    s32          fbcount;
    s32          chosen_fb = 0;
    GLXFBConfig* fbc       = glXChooseFBConfig(_display, DefaultScreen(_display), visual_attribs, &fbcount);
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
    swa.colormap = cmap   = XCreateColormap(_display, RootWindow(_display, vi->screen), vi->visual, AllocNone);
    swa.background_pixmap = None;
    swa.border_pixel      = 0;
    swa.event_mask        = StructureNotifyMask;

    _window = XCreateWindow(_display, RootWindow(_display, vi->screen), 0, 0, pen_window.width, pen_window.height, 0,
                            vi->depth, InputOutput, vi->visual, CWBorderPixel | CWColormap | CWEventMask, &swa);

    XStoreName(_display, _window, pen_window.window_title);
    XSelectInput(_display, _window,
                 ExposureMask | StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask );

    // Create Gl Context
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB =
        (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

    int context_attribs[] = {GLX_CONTEXT_MAJOR_VERSION_ARB, 3, GLX_CONTEXT_MINOR_VERSION_ARB, 1, None };

    _gl_context = glXCreateContextAttribsARB(_display, best_fbc, 0, True, context_attribs);

    if (ctx_error_occured || !_gl_context)
    {
        PEN_PRINTF("Error: OpenGL 3.1 Context Failed to create");
        return 1;
    }

    XMapWindow(_display, _window);

    // Make current and init glew
    pen_make_gl_context_current();

    glewExperimental = true;
    GLenum err       = glewInit();
    if (err != GLEW_OK)
    {
        PEN_PRINTF("Error: glewInit failed: %s\n", glewGetErrorString(err));
        return 1;
    }

    // initilaise any generic systems
    users();
    pen::timer_system_intialise();

    // inits renderer and loops in wait for jobs, calling os update
    renderer_init(nullptr, true);

    // exit, kill other threads and wait
    pen::thread_terminate_jobs();

    XDestroyWindow(_display, _window);
    XCloseDisplay(_display);

    return 0;
}

namespace pen
{
    const c8* os_path_for_resource(const c8* filename)
    {
        return filename;
    }

    s32 translate_mouse_button(s32 b)
    {
        static f32 mw = 0.0f;
        switch(b)
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
        // numerical keys
        if(k >= XK_0 && k <= XK_9)
            return PK_0 + (k - XK_0);

        if(k >= XK_a && k <= XK_z)
            return PK_A + (k - XK_a);

        if(k >= XK_A && k <= XK_Z)
            return PK_A + (k - XK_A);

        switch(k)
        {
            // Misc
            case XK_KP_Space: return PK_SPACE;
            case XK_BackSpace: return PK_BACK;
            case XK_Tab: return PK_TAB;
            case XK_Linefeed: return 0; //?
            case XK_Clear: return PK_CLEAR;
            case XK_Return: return PK_RETURN;
            case XK_Pause: return PK_PAUSE;
            case XK_Scroll_Lock: return PK_SCROLL;
            case XK_Sys_Req: return 0; //?
            case XK_Escape: return PK_ESCAPE;
            case XK_Delete: return PK_DELETE;
            case XK_Home: return PK_HOME;
            case XK_Left: return PK_LEFT;
            case XK_Up: return PK_UP;
            case XK_Right: return PK_RIGHT;
            case XK_Down: return PK_DOWN;
            case XK_Prior: return PK_PRIOR;
            case XK_Next: return PK_NEXT;

            // F
            case XK_F1: return PK_F1;
            case XK_F2: return PK_F2;
            case XK_F3: return PK_F3;
            case XK_F4: return PK_F4;
            case XK_F5: return PK_F5;
            case XK_F6: return PK_F6;
            case XK_F7: return PK_F7;
            case XK_F8: return PK_F8;
            case XK_F9: return PK_F9;
            case XK_F10: return PK_F10;
            case XK_F11: return PK_F11;
            case XK_F12: return PK_F12;

            // Modifiers
            case XK_Shift_L: return PK_SHIFT;
            case XK_Shift_R: return PK_SHIFT;
            case XK_Control_L: return PK_CONTROL;
            case XK_Control_R: return PK_CONTROL;
            case XK_Caps_Lock: return PK_CAPITAL;
            case XK_Alt_L: return PK_MENU;
            case XK_Alt_R: return PK_MENU;

            // Keypad
            case XK_KP_Equal: return PK_ADD;
            case XK_KP_Multiply: return PK_MULTIPLY;
            case XK_KP_Add: return PK_ADD;
            case XK_KP_Separator: return PK_SEPARATOR;
            case XK_KP_Subtract: return PK_SUBTRACT;
            case XK_KP_Decimal: return PK_DECIMAL;
            case XK_KP_Divide: return PK_DIVIDE;
        };

        return 0;
    }

    bool os_update()
    {
        static bool init_jobs = false;
        if(!init_jobs)
        {
            //audio, user thread etc
            pen::default_thread_info thread_info;
            thread_info.flags = pen::PEN_CREATE_AUDIO_THREAD;
            pen::thread_create_default_jobs( thread_info );
            init_jobs = true;
        }

        while (XPending(_display) > 0)
        {
            XEvent event;
            XNextEvent(_display, (XEvent*)&event);
            switch (event.type)
            {
                case Expose:
                {
                    XWindowAttributes attribs;
                    XGetWindowAttributes(_display, _window, &attribs);
                    pen_window.width  = attribs.width;
                    pen_window.height = attribs.height;
                }
                break;
                case KeyPress:
                {
                    static c8 buf[255];
                    KeySym    k;

                    // todo for utf8
                    //Xutf8LookupString(xic, &event.xkey, buf, sizeof(buf) - 1, &keysym, &status);

                    XLookupString(&event.xkey, buf, 255, &k, 0);
                    pen::input_set_unicode_key_down(buf[0]);

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
                    if(mb != -1)
                        pen::input_set_mouse_down(mb);
                }
                break;
                case ButtonRelease:
                {
                    s32 mb = translate_mouse_button(event.xbutton.button);
                    if(mb != -1)
                        pen::input_set_mouse_up(mb);
                }
                break;
            }
        }

        // Update cursor
        int root_x, root_y;
        int win_x, win_y;
        unsigned int mask_return;
        Window window_returned;

        int result = XQueryPointer(_display, _window, &window_returned, &window_returned, &root_x, &root_y, &win_x, &win_y, &mask_return);
        pen::input_set_mouse_pos((f32)win_x, (f32)win_y);

        // Check for terminate and poll terminated jobs
        static bool pen_terminate_app = false;
        if(pen_terminate_app)
        {
            if(pen::thread_terminate_jobs())
                return false;
        }

        return true;
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
        width  = pen_window.width;
        height = pen_window.height;
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
} // namespace pen