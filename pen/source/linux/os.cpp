#include "GL/glew.h"

#include "console.h"
#include "input.h"
#include "os.h"
#include "pen.h"
#include "threads.h"
#include "timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <GL/glx.h>
#include <X11/Xlib.h>

using namespace pen;

namespace pen
{
    extern void renderer_init(void* user_data);
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
    ctx_error_occured = true;
    return 0;
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
    //PEN_PRINTF("glx version %i.%i", glx_major, glx_minor);

    // glx setup
    const char* glxExts = glXQueryExtensionsString(_display, DefaultScreen(_display));

    //PEN_PRINTF("%s", glxExts);

    ctx_error_occured                         = false;
    int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&ctx_error_handler);

    // find fb with matching samples
    s32          fbcount;
    s32          chosen_fb = 0;
    GLXFBConfig* fbc = glXChooseFBConfig(_display, DefaultScreen(_display), visual_attribs, &fbcount);
    for (int i = 0; i < fbcount; ++i)
    {
        XVisualInfo* vi = glXGetVisualFromFBConfig(_display, fbc[i]);

        int samp_buf, samples;
        glXGetFBConfigAttrib( _display, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf );
        glXGetFBConfigAttrib( _display, fbc[i], GLX_SAMPLES       , &samples  );

        if(samples == pen_window.sample_count)
        {
            chosen_fb = i;
            break;
        }
    }

    GLXFBConfig best_fbc = fbc[chosen_fb];

    // Create window
    XVisualInfo* vi = glXGetVisualFromFBConfig(_display, best_fbc);

    //PEN_PRINTF("visual info %i", vi);

    XSetWindowAttributes swa;
    Colormap             cmap;
    swa.colormap = cmap   = XCreateColormap(_display, RootWindow(_display, vi->screen), vi->visual, AllocNone);
    swa.background_pixmap = None;
    swa.border_pixel      = 0;
    swa.event_mask        = StructureNotifyMask;

    _window = XCreateWindow(_display, RootWindow(_display, vi->screen), 0, 0, pen_window.width, pen_window.height, 0,
                            vi->depth, InputOutput, vi->visual, CWBorderPixel | CWColormap | CWEventMask, &swa);

    XStoreName(_display, _window, pen_window.window_title);
    XSelectInput(_display, _window, ExposureMask | 
                                    StructureNotifyMask | 
                                    ButtonPressMask | ButtonReleaseMask |
                                    KeyPressMask | KeyReleaseMask |
                                    PointerMotionMask | PointerMotionHintMask | ButtonMotionMask | Button1MotionMask | Button2MotionMask | Button3MotionMask );
    XMapWindow(_display, _window);

    // Create Gl Context
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB =
        (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

    int context_attribs[] = {GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                             GLX_CONTEXT_MINOR_VERSION_ARB, 1,
                             None};

    _gl_context = glXCreateContextAttribsARB(_display, best_fbc, 0, True, context_attribs);

    XSync(_display, False);

    if (ctx_error_occured || !_gl_context)
    {
        PEN_PRINTF("Error: OpenGL 3.1 Context Failed to create");
        return 1;
    }

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
    pen::timer_system_intialise();

    //inits renderer and loops in wait for jobs, calling os update
    renderer_init(nullptr);

    //exit, kill other threads and wait
    pen::thread_terminate_jobs();

    XDestroyWindow(_display, _window);
	XCloseDisplay(_display);

    return 0;
}

namespace pen
{
    bool os_update( )
    {
        while(XPending(_display) > 0)
        {
            XEvent event;
            XNextEvent(_display, (XEvent*)&event);
            switch(event.type) 
            {
                case Expose:
                {
                    XWindowAttributes attribs;
                    XGetWindowAttributes(_display, _window, &attribs);
                    pen_window.width = attribs.width;
                    pen_window.height = attribs.height;
                }
                break;
                case KeyPress:
                {
                    static c8 buf[255];
                    KeySym k;
                    XLookupString(&event.xkey, buf, 255, &k, 0);
                    pen::input_set_unicode_key_down(buf[0]);
                    pen::input_set_key_down(event.xkey.keycode);
                }
                break;
                case KeyRelease:
                {
                    static c8 buf[255];
                    KeySym k;
                    XLookupString(&event.xkey, buf, 255, &k, 0);
                    pen::input_set_unicode_key_up(buf[0]);
                    pen::input_set_key_up(event.xkey.keycode);
                }
                break;
                case ButtonPress:
                {
                    // event.xbutton.x, event.xbutton.y
                }
                break;
                case MotionNotify:
                {
                    pen::input_set_mouse_pos(event.xmotion.x, event.xmotion.y);
                }
                break;
            }
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