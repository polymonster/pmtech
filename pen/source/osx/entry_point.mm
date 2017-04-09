#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#include "window.h"
#include "pen.h"
#include "threads.h"
#include "renderer.h"
#include "timer.h"
#include "audio.h"
#include "input.h"

NSOpenGLView* _gl_view;
NSWindow * _window;
NSOpenGLContext* _gl_context;

extern pen::window_creation_params pen_window;
extern PEN_THREAD_RETURN pen::game_entry( void* params );

void pen_make_gl_context_current( )
{
    [_gl_context makeCurrentContext];
}

void pen_gl_swap_buffers( )
{
    [_gl_context flushBuffer];
}

void create_gl_context()
{
    NSOpenGLPixelFormatAttribute pixel_format_attribs[] =
    {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        NSOpenGLPFAColorSize,     24,
        NSOpenGLPFAAlphaSize,     8,
        NSOpenGLPFADepthSize,     24,
        NSOpenGLPFAStencilSize,   8,
        NSOpenGLPFADoubleBuffer,  true,
        NSOpenGLPFAAccelerated,   true,
        NSOpenGLPFANoRecovery,    true,
        0,                        0,
    };
    
    NSOpenGLPixelFormat *pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixel_format_attribs];
    
    NSRect glViewRect = [[_window contentView] bounds];
    
    NSOpenGLView* glView = [[NSOpenGLView alloc] initWithFrame:glViewRect pixelFormat:pixel_format];
    
    [pixel_format release];
    
    [_window.contentView addSubview:glView];
    
    NSOpenGLContext* glContext = [glView openGLContext];
    
    [glContext makeCurrentContext];
    GLint interval = 1;
    [glContext setValues:&interval forParameter:NSOpenGLCPSwapInterval];
    
    _gl_view    = glView;
    _gl_context = glContext;
}

void get_mouse_pos( int& x, int& y )
{
    NSRect original_frame = [_window frame];
    NSPoint location = [_window mouseLocationOutsideOfEventStream];
    NSRect adjust_frame = [_window contentRectForFrameRect: original_frame];
    
    x = location.x;
    y = (int)location.y;
    
    // clamp within the range of the window
    
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > (int)adjust_frame.size.width) x = (int)adjust_frame.size.width;
    if (y > (int)adjust_frame.size.height) y = (int)adjust_frame.size.height;
}

void handle_modifiers( NSEvent* event )
{
    u32 flags = [event modifierFlags];
    
    if (flags & NSEventModifierFlagShift)
    {
        pen::input_set_key_down(PENK_SHIFT);
    }
    else
    {
        pen::input_set_key_up(PENK_SHIFT);
    }
    
    if (flags & NSEventModifierFlagOption)
    {
        pen::input_set_key_down(PENK_MENU);
    }
    else
    {
        pen::input_set_key_up(PENK_MENU);
    }
    
    if (flags & NSEventModifierFlagControl)
    {
        pen::input_set_key_down(PENK_CONTROL);
    }
    else
    {
        pen::input_set_key_up(PENK_CONTROL);
    }
    
    if (flags & NSEventModifierFlagCommand)
    {
        pen::input_set_key_down(PENK_COMMAND);
    }
    else
    {
        pen::input_set_key_up(PENK_COMMAND);
    }
    
}

void handle_key_event( NSEvent* event, bool down )
{
    handle_modifiers( event );
    
    NSString* key = [event charactersIgnoringModifiers];
    
    if ([key length] == 0)
    {
        return;
    }
    
    unichar key_char = [key characterAtIndex:0];
    
    if( key_char < 256 )
    {
        u32 mapped_key_char = key_char;
        if( mapped_key_char >= 97 && mapped_key_char <= 122 )
        {
            mapped_key_char -= 32;
        }
        else if( mapped_key_char == ',' || mapped_key_char == '.' )
        {
            mapped_key_char += 16;
        }
        else if( mapped_key_char == '\'' )
        {
            mapped_key_char = '@';
        }
        else if( mapped_key_char == '-' )
        {
            mapped_key_char = '_';
        }
        
        if( down )
        {
            pen::input_set_key_down( mapped_key_char );
        }
        else
        {
            pen::input_set_key_up( mapped_key_char );
        }
    }
    else
    {
        u32 penk = 0;
        
        switch (key_char)
        {
            case NSF1FunctionKey:  penk = PENK_F1;
            case NSF2FunctionKey:  penk = PENK_F2;
            case NSF3FunctionKey:  penk = PENK_F3;
            case NSF4FunctionKey:  penk = PENK_F4;
            case NSF5FunctionKey:  penk = PENK_F5;
            case NSF6FunctionKey:  penk = PENK_F6;
            case NSF7FunctionKey:  penk = PENK_F7;
            case NSF8FunctionKey:  penk = PENK_F8;
            case NSF9FunctionKey:  penk = PENK_F9;
            case NSF10FunctionKey: penk = PENK_F10;
            case NSF11FunctionKey: penk = PENK_F11;
            case NSF12FunctionKey: penk = PENK_F12;
                
            case NSLeftArrowFunctionKey:   penk = PENK_LEFT;
            case NSRightArrowFunctionKey:  penk = PENK_RIGHT;
            case NSUpArrowFunctionKey:     penk = PENK_UP;
            case NSDownArrowFunctionKey:   penk = PENK_DOWN;
                
            case NSPageUpFunctionKey:      penk = PENK_NEXT;
            case NSPageDownFunctionKey:    penk = PENK_PRIOR;
            case NSHomeFunctionKey:        penk = PENK_HOME;
            case NSEndFunctionKey:         penk = PENK_END;
                
            case NSPrintScreenFunctionKey: penk = PENK_SNAPSHOT;
        }
        
        if( down )
        {
            pen::input_set_key_down( penk );
        }
        else
        {
            pen::input_set_key_up( penk );
        }
    }
}

bool handle_event(NSEvent* event)
{
    if (event)
    {
        NSEventType event_type = [event type];
        
        switch (event_type)
        {
            case NSEventTypeMouseMoved:
            case NSEventTypeLeftMouseDragged:
            case NSEventTypeRightMouseDragged:
            case NSEventTypeOtherMouseDragged:
            {
                int x, y;
                get_mouse_pos( x, y );
                pen::input_set_mouse_pos( x, y );
                break;
            }
                
            case NSEventTypeLeftMouseDown: pen::input_set_mouse_down( PEN_MOUSE_L ); break;
            case NSEventTypeRightMouseDown: pen::input_set_mouse_down( PEN_MOUSE_R ); break;
            case NSEventTypeOtherMouseDown: pen::input_set_mouse_down( PEN_MOUSE_M ); break;

            case NSEventTypeLeftMouseUp: pen::input_set_mouse_up( PEN_MOUSE_L ); break;
            case NSEventTypeRightMouseUp: pen::input_set_mouse_up( PEN_MOUSE_R ); break;
            case NSEventTypeOtherMouseUp: pen::input_set_mouse_up( PEN_MOUSE_M ); break;
                
            case NSEventTypeScrollWheel:
            {
                f32 scroll_delta = [event deltaY];
                pen::input_set_mouse_wheel((s32)scroll_delta);
                break;
            }
                
            case NSEventTypeFlagsChanged:
            {
                handle_modifiers(event);
                break;
            }
                
            case NSEventTypeKeyDown:
            {
                handle_key_event(event, true);
                break;
            }
                
            case NSEventTypeKeyUp:
            {
                handle_key_event(event, false);
                break;
            }
                
            default:
                break;
        }
        
        return true;
    }
    
    return false;
}

int main(int argc, char **argv)
{
    //window creation
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSLog(@"NSApp=%@", NSApp);
    [NSApplication sharedApplication];
    NSLog(@"NSApp=%@", NSApp);

    NSRect frame = NSMakeRect(0, 0, pen_window.width, pen_window.height);
    
    NSUInteger style_mask = NSWindowStyleMaskTitled;
    
    _window = [[NSWindow alloc] initWithContentRect:frame styleMask:style_mask backing: NSBackingStoreBuffered defer:NO];

    [_window makeKeyAndOrderFront: _window];
    
    [_window setTitle:[NSString stringWithUTF8String:pen_window.window_title]];
    
    [_window center];
    
    create_gl_context();
    
    [pool drain];
    
    //init systems
    pen::timer_system_intialise();
    
    //create render thread
    pen::renderer_thread_init();
    pen::threads_create( &pen::renderer_init_thread, 1024*1024, nullptr, pen::THREAD_START_DETACHED );
    pen::renderer_wait_init();
    
    //create audi thread
    pen::audio_init_thread_primitives();
    pen::threads_create( &pen::audio_thread_function, 1024*1024, nullptr, pen::THREAD_START_DETACHED );
    pen::audio_wait_for_init();
    
    //create game thread
    pen::threads_create( &pen::game_entry, 1024*1024, nullptr, pen::THREAD_START_DETACHED );
    
    //main thread loop
    while( 1 )
    {
        while( 1 )
        {
            NSEvent* peek_event = [NSApp
                        nextEventMatchingMask:NSEventMaskAny
                        untilDate:[NSDate distantPast] // do not wait for event
                        inMode:NSDefaultRunLoopMode
                        dequeue:YES
                        ];
            
            if( !peek_event )
            {
                break;
            }
            
            handle_event(peek_event);

            [NSApp sendEvent:peek_event];
            [NSApp updateWindows];
        }
        
        int x, y;
        get_mouse_pos( x, y );
        pen::input_set_mouse_pos( x, y );

        //sleep a bit
        pen::threads_sleep_ms( 16 );
    }
}

namespace pen
{
    void os_set_cursor_pos( u32 client_x, u32 client_y )
    {
        
    }
    
    void os_show_cursor( bool show )
    {
        
    }
    
    void* window_get_primary_display_handle( )
    {
        return (void*)_window;
    }
}
