#include "os.h"
#include "pen.h"
#include "console.h"
#include "input.h"
#include "threads.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <GL/glx.h>

using namespace pen;

//pen required externs
extern window_creation_params pen_window;
pen::user_info pen_user_info;

//glx
#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
GLXContext  _gl_context = 0;
Display     *_display;
Window      _window;

void pen_make_gl_context_current( )
{
    glXMakeCurrent( _display, _window, _gl_context );
}

void pen_gl_swap_buffers( )
{
    glXSwapBuffers ( _display, _window );
}

static int visual_attribs[] =
{
    GLX_X_RENDERABLE    , True,
    GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
    GLX_RENDER_TYPE     , GLX_RGBA_BIT,
    GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
    GLX_RED_SIZE        , 8,
    GLX_GREEN_SIZE      , 8,
    GLX_BLUE_SIZE       , 8,
    GLX_ALPHA_SIZE      , 8,
    GLX_DEPTH_SIZE      , 24,
    GLX_STENCIL_SIZE    , 8,
    GLX_DOUBLEBUFFER    , True,
    //GLX_SAMPLE_BUFFERS  , 1,
    //GLX_SAMPLES         , 4,
    None
};

static bool ctx_error_occured = false;
static int ctx_error_handler( Display *dpy, XErrorEvent *ev )
{
    ctx_error_occured = true;
    return 0;
}

int main (int argc, char *argv[])
{
    Visual                  *visual;
    int                     depth;
    XSetWindowAttributes    frame_attributes;
    XEvent               	event;

    _display = XOpenDisplay(NULL);
    visual = DefaultVisual(_display, 0);
    depth  = DefaultDepth(_display, 0);

    // Check glx version
    s32 glx_major, glx_minor = 0;
	glXQueryVersion(_display, &glx_major, &glx_minor);

    PEN_PRINTF("glx version %i.%i", glx_major, glx_minor);

    // glx setup
    const char *glxExts = glXQueryExtensionsString( _display, DefaultScreen( _display ) );

    ctx_error_occured = false;
    int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&ctx_error_handler);

    s32 fbcount;
    GLXFBConfig* fbc = glXChooseFBConfig(_display, DefaultScreen(_display), visual_attribs, &fbcount);
    for (int i = 0; i < fbcount; ++i)
    {
        XVisualInfo *vi = glXGetVisualFromFBConfig( _display, fbc[i] );
    }

    GLXFBConfig best_fbc = fbc[1];

    // Create window
    XVisualInfo *vi = glXGetVisualFromFBConfig( _display, best_fbc );

    XSetWindowAttributes swa;
    Colormap cmap;
    swa.colormap = cmap = XCreateColormap( _display,
                                            RootWindow( _display, vi->screen ), 
                                            vi->visual, AllocNone );
    swa.background_pixmap = None ;
    swa.border_pixel      = 0;
    swa.event_mask        = StructureNotifyMask;

    printf( "Creating window\n" );
    _window = XCreateWindow( _display, RootWindow( _display, vi->screen ), 
                                0, 0, pen_window.width, pen_window.height, 0, vi->depth, InputOutput, 
                                vi->visual, 
                                CWBorderPixel|CWColormap|CWEventMask, &swa );
    
    XStoreName(_display, _window, pen_window.window_title);
    XSelectInput(_display, _window, ExposureMask | StructureNotifyMask);
    XMapWindow(_display, _window);

    // Create Gl Context
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
    glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)
           glXGetProcAddressARB( (const GLubyte *) "glXCreateContextAttribsARB" );

    int context_attribs[] =
    {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 0,
        None
    };

    _gl_context = glXCreateContextAttribsARB( _display, best_fbc, 0, True, context_attribs );

    XSync( _display, False );
    if ( !ctx_error_occured && _gl_context )
    {
        PEN_PRINTF("openGL 3.0 context created");
    }

    // Verifying that context is a direct context
    if ( ! glXIsDirect ( _display, _gl_context ) )
    {
        PEN_PRINTF( "Indirect GLX rendering context obtained\n" );
    }
    else
    {
        PEN_PRINTF( "Direct GLX rendering context obtained\n" );
    }

    /*
    //initilaise any generic systems
	pen::timer_system_intialise( );
	HWND hwnd = (HWND)pen::window_get_primary_display_handle();

	pen::default_thread_info thread_info;
	thread_info.flags = pen::PEN_CREATE_AUDIO_THREAD | pen::PEN_CREATE_RENDER_THREAD;
	thread_info.render_thread_params = &hwnd;

	pen::thread_create_default_jobs( thread_info );
    */

    while ( 1 ) 
	{
        XNextEvent(_display, (XEvent *)&event);

        pen_make_gl_context_current();

        glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        pen_gl_swap_buffers();

        pen::thread_sleep_ms(1);
    }

    return(0);
}

namespace pen
{
    u32 window_init( void* params )
	{
		return 0;
	}

    void* window_get_primary_display_handle()
	{
		return nullptr;
	}

    void window_get_size( s32& width, s32& height )
	{
		width = pen_window.width;
		height = pen_window.height;
	}
    
    void os_set_cursor_pos( u32 client_x, u32 client_y )
	{

	}

    void os_show_cursor( bool show )
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
}