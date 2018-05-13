#include "os.h"
#include "pen.h"
#include "console.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

using namespace pen;

//pen required externs
extern window_creation_params pen_window;
pen::user_info pen_user_info;

void pen_make_gl_context_current( )
{
}

void pen_gl_swap_buffers( )
{
}

int main (int argc, char *argv[])
{
    Display                 *display;
    Visual                  *visual;
    int                     depth;
    XSetWindowAttributes    frame_attributes;
    Window                  frame_window;
    XEvent               	event;

    display = XOpenDisplay(NULL);
    visual = DefaultVisual(display, 0);
    depth  = DefaultDepth(display, 0);
    
    frame_window = XCreateWindow(display, XRootWindow(display, 0),
                                 0, 0, pen_window.width, pen_window.height, 5, depth,
                                 InputOutput, visual, CWBackPixel,
                                 &frame_attributes);

    XStoreName(display, frame_window, pen_window.window_title);
    XSelectInput(display, frame_window, ExposureMask | StructureNotifyMask);

    XMapWindow(display, frame_window);

    while ( 1 ) 
	{
		XNextEvent(display, (XEvent *)&event);
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