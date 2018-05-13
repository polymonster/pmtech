#include "os.h"
#include "pen.h"
#include "console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>

using namespace pen;

extern window_creation_params pen_window;

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