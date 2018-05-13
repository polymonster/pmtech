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
    int                     text_x;
    int                     text_y;
    XSetWindowAttributes    frame_attributes;
    Window                  frame_window;
    XFontStruct             *fontinfo;
    XGCValues               gr_values;
    GC                      graphical_context;
    XEvent               	event;

    display = XOpenDisplay(NULL);
    visual = DefaultVisual(display, 0);
    depth  = DefaultDepth(display, 0);
    
    frame_window = XCreateWindow(display, XRootWindow(display, 0),
                                 0, 0, 400, 400, 5, depth,
                                 InputOutput, visual, CWBackPixel,
                                 &frame_attributes);

    XStoreName(display, frame_window, pen_window.window_title);
    XSelectInput(display, frame_window, ExposureMask | StructureNotifyMask);

    //fontinfo = XLoadQueryFont(display, "10x20");
    //gr_values.font = fontinfo->fid;
    //gr_values.foreground = XBlackPixel(display, 0);
    //graphical_context = XCreateGC(display, frame_window, 
    //                              GCFont+GCForeground, &gr_values);

    XMapWindow(display, frame_window);

    while ( 1 ) 
	{
		XNextEvent(display, (XEvent *)&event);

		#if 0
        
        switch ( event.type ) {
            case Expose:
            {
                XWindowAttributes windnamespace pen
{

}ow_attributes;
                int font_direction, font_ascent, font_descent;
                XCharStruct text_structure;
                XTextExtents(fontinfo, hello_string, hello_string_length, 
                             &font_direction, &font_ascent, &font_descent, 
                             &text_structure);
                XGetWindowAttributes(display, frame_window, &window_attributes);
                text_x = (window_attributes.width - text_structure.width)/2;
                text_y = (window_attributes.height - 
                          (text_structure.ascent+text_structure.descent))/2;
                XDrawString(display, frame_window, graphical_context,
                            text_x, text_y, hello_string, hello_string_length);
                break;
            }
            default:
                break;
        }
		#endif

    }
    return(0);
}