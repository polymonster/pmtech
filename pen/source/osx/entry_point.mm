#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>
#import <GameController/GameController.h>

#include "pen.h"
#include "threads.h"
#include "renderer.h"
#include "timer.h"
#include "audio.h"
#include "input.h"

NSOpenGLView* _gl_view;
NSWindow * _window;
NSOpenGLContext* _gl_context;
NSOpenGLPixelFormat* _pixel_format;

extern pen::window_creation_params pen_window;
extern a_u8 g_window_resize;
int g_rs = 0;

pen::user_info pen_user_info;

extern PEN_THREAD_RETURN pen::game_entry( void* params );

@interface app_delegate : NSObject<NSApplicationDelegate>
{
    bool terminated;
}

+ (app_delegate *)shared_delegate;
- (id)init;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender;
- (bool)applicationHasTerminated;

@end

@interface i_window : NSObject<NSWindowDelegate>
{
    uint32_t window_count;
}

+ (i_window*)shared_delegate;
- (id)init;
- (void)windowCreated:(NSWindow*)window;
- (void)windowWillClose:(NSNotification*)notification;
- (BOOL)windowShouldClose:(NSWindow*)window;
- (void)windowDidResize:(NSNotification*)notification;
- (void)windowDidBecomeKey:(NSNotification *)notification;
- (void)windowDidResignKey:(NSNotification *)notification;

@end

void pen_make_gl_context_current( )
{
    [_gl_context makeCurrentContext];
}

void pen_gl_swap_buffers( )
{
    if(g_rs<=0)
        [_gl_context flushBuffer];
}

void pen_window_resize( )
{
    g_rs = 10;
    
    NSRect view_rect = [[_window contentView] bounds];
    
    if( _gl_view.frame.size.width == view_rect.size.width &&
        _gl_view.frame.size.height == view_rect.size.height )
        return;
    
    [_gl_view setFrameSize:view_rect.size];
    
    pen_window.width = view_rect.size.width;
    pen_window.height = view_rect.size.height;
}

void create_gl_context()
{
    u32 sample_buffers = pen_window.sample_count ? 1 : 0;
    
    NSOpenGLPixelFormatAttribute pixel_format_attribs[] =
    {
        //gl 3.2
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core,
        
        //msaa
        NSOpenGLPFAMultisample,
        NSOpenGLPFASampleBuffers,   sample_buffers,
        NSOpenGLPFASamples,         pen_window.sample_count,
        
        //RGBA D24S8
        NSOpenGLPFAColorSize,     24,
        NSOpenGLPFAAlphaSize,     8,
        NSOpenGLPFADepthSize,     24,
        NSOpenGLPFAStencilSize,   8,
        
        //double buffered, HAL
        NSOpenGLPFADoubleBuffer,  true,
        NSOpenGLPFAAccelerated,   true,
        NSOpenGLPFANoRecovery,    true,
        0,                        0,
    };
    
    _pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixel_format_attribs];
    
    NSRect glViewRect = [[_window contentView] bounds];
    
    NSOpenGLView* glView = [[NSOpenGLView alloc] initWithFrame:glViewRect pixelFormat:_pixel_format];
    
    [[glView superview]setAutoresizingMask:NSViewWidthSizable|NSViewHeightSizable];
    
    [_pixel_format release];
    
    [_window.contentView addSubview:glView];
    
    NSOpenGLContext* glContext = [glView openGLContext];
    
    [glContext makeCurrentContext];
    GLint interval = 1;
    [glContext setValues:&interval forParameter:NSOpenGLCPSwapInterval];
    
    _gl_view    = glView;
    _gl_context = glContext;
}

void get_mouse_pos( f32& x, f32& y )
{
    NSRect original_frame = [_window frame];
    NSPoint location = [_window mouseLocationOutsideOfEventStream];
    NSRect adjust_frame = [_window contentRectForFrameRect: original_frame];
    
    x = location.x;
    y = (int)adjust_frame.size.height - location.y;
    
    // clamp within the range of the window
    /*
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > (int)adjust_frame.size.width) x = (int)adjust_frame.size.width;
    if (y > (int)adjust_frame.size.height) y = (int)adjust_frame.size.height;
    */
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
        
        u32 vk = 511;
        
        if( mapped_key_char >= 'a' && mapped_key_char <= 'z' )
        {
            vk = PENK_A + (mapped_key_char - 'a');
        }
        
        if( mapped_key_char >= '0' && mapped_key_char <= '9' )
        {
            vk = PENK_0 + (mapped_key_char - '0');
        }
        
        if( mapped_key_char == 127 )
        {
            mapped_key_char = 8;
            vk = PENK_BACK;
        }
        
        if( down )
        {
            pen::input_set_unicode_key_down( mapped_key_char );
            
            pen::input_set_key_down(vk);
        }
        else
        {
            pen::input_set_unicode_key_up( mapped_key_char );
            
            pen::input_set_key_up(vk);
        }
    }
    else
    {
        u32 penk = 0;
        
        switch (key_char)
        {
            case NSF1FunctionKey:           penk = PENK_F1; break;
            case NSF2FunctionKey:           penk = PENK_F2; break;
            case NSF3FunctionKey:           penk = PENK_F3; break;
            case NSF4FunctionKey:           penk = PENK_F4; break;
            case NSF5FunctionKey:           penk = PENK_F5; break;
            case NSF6FunctionKey:           penk = PENK_F6; break;
            case NSF7FunctionKey:           penk = PENK_F7; break;
            case NSF8FunctionKey:           penk = PENK_F8; break;
            case NSF9FunctionKey:           penk = PENK_F9; break;
            case NSF10FunctionKey:          penk = PENK_F10; break;
            case NSF11FunctionKey:          penk = PENK_F11; break;
            case NSF12FunctionKey:          penk = PENK_F12; break;
                
            case NSLeftArrowFunctionKey:    penk = PENK_LEFT; break;
            case NSRightArrowFunctionKey:   penk = PENK_RIGHT; break;
            case NSUpArrowFunctionKey:      penk = PENK_UP; break;
            case NSDownArrowFunctionKey:    penk = PENK_DOWN; break;
                
            case NSPageUpFunctionKey:       penk = PENK_NEXT; break;
            case NSPageDownFunctionKey:     penk = PENK_PRIOR; break;
            case NSHomeFunctionKey:         penk = PENK_HOME; break;
            case NSEndFunctionKey:          penk = PENK_END; break;
                
            case NSDeleteFunctionKey:       penk = PENK_BACK; break;
            case NSDeleteCharFunctionKey:   penk = PENK_BACK; break;
                
            case NSPrintScreenFunctionKey:  penk = PENK_SNAPSHOT; break;
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
                f32 x, y;
                get_mouse_pos( x, y );
                pen::input_set_mouse_pos( x, y );
                return true;
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
                pen::input_set_mouse_wheel(scroll_delta);
                return true;
            }
                
            case NSEventTypeFlagsChanged:
            {
                handle_modifiers(event);
                return true;
            }
                
            case NSEventTypeKeyDown:
            {
                handle_key_event(event, true);
                return true;
            }
                
            case NSEventTypeKeyUp:
            {
                handle_key_event(event, false);
                return true;
            }
                
            default:
                return false;
        }
    }
    
    return false;
}

static bool pen_terminate_app = false;

void users()
{
    NSString* ns_full_user_name = NSFullUserName();
    pen_user_info.full_user_name =[ns_full_user_name UTF8String];
    
    NSString* ns_user_name = NSUserName();
    pen_user_info.user_name =[ns_user_name UTF8String];
}

void __main_update( )
{
    
}

int main(int argc, char **argv)
{
    //window creation
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSLog(@"NSApp=%@", NSApp);
    [NSApplication sharedApplication];
    NSLog(@"NSApp=%@", NSApp);
    
    id dg = [app_delegate shared_delegate];
    [NSApp setDelegate:dg];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp finishLaunching];
    
    [[NSNotificationCenter defaultCenter]
     postNotificationName:NSApplicationWillFinishLaunchingNotification
     object:NSApp];
    
    [[NSNotificationCenter defaultCenter]
     postNotificationName:NSApplicationDidFinishLaunchingNotification
     object:NSApp];
    
    NSRect frame = NSMakeRect(0, 0, pen_window.width, pen_window.height);
    
    NSUInteger style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    
    _window = [[NSWindow alloc] initWithContentRect:frame styleMask:style_mask backing: NSBackingStoreBuffered defer:NO];

    [_window makeKeyAndOrderFront: _window];
    
    id wd = [i_window shared_delegate];
    [_window setDelegate:wd];
    
    [_window setTitle:[NSString stringWithUTF8String:pen_window.window_title]];
    [_window setAcceptsMouseMovedEvents:YES];
    
    [_window center];
    
    create_gl_context();
    
    [pool drain];
    
    //os stuff
    users();
    
    //init systems
    pen::timer_system_intialise();
    
    //audio, renderer, game
    pen::default_thread_info thread_info;
    thread_info.flags = pen::PEN_CREATE_AUDIO_THREAD | pen::PEN_CREATE_RENDER_THREAD;
    
    pen::threads_create_default_jobs( thread_info );
    
    //main thread loop
    while( !pen_terminate_app )
    {
        NSAutoreleasePool * _pool = [[NSAutoreleasePool alloc] init];
        
        [NSApp updateWindows];
        
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
            
            if( !handle_event(peek_event) )
            {
                [NSApp sendEvent:peek_event];
            }
            
            break;
        }
        
        f32 x, y;
        get_mouse_pos( x, y );
        pen::input_set_mouse_pos( x, y );
        
        if( g_rs > 0 )
        {
            pen::input_set_mouse_up( PEN_MOUSE_L );
            pen::input_set_mouse_up( PEN_MOUSE_R );
            pen::input_set_mouse_up( PEN_MOUSE_M );
        }

        //sleep a bit
        [_pool drain];
        pen::threads_sleep_ms( 4 );
        
        g_rs--;
    }
    
    //shutdown
    pen::threads_terminate_jobs();
}

namespace pen
{
    void os_set_cursor_pos( u32 client_x, u32 client_y )
    {
        
    }
    
    void os_show_cursor( bool show )
    {
        
    }
    
    void window_get_size( s32& width, s32& height )
    {
        NSScreen* screen = [_window screen];
        NSRect rect = [screen frame];
        
        width = rect.size.width;
        height = rect.size.height;
    }
    
    void* window_get_primary_display_handle( )
    {
        return (void*)_window;
    }
}

@implementation app_delegate

+ (app_delegate *)shared_delegate
{
    static id delegate = [app_delegate new];
    return delegate;
}

- (id)init
{
    self = [super init];
    
    if (nil == self)
    {
        return nil;
    }
    
    self->terminated = false;
    return self;
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    self->terminated = true;
    pen_terminate_app = true;
    return NSTerminateCancel;
}

- (bool)applicationHasTerminated
{
    return self->terminated;
}

@end

@implementation i_window

+ (i_window*)shared_delegate
{
    static id window_delegate = [i_window new];
    return window_delegate;
}

- (id)init
{
    self = [super init];
    if (nil == self)
    {
        return nil;
    }
    
    self->window_count = 0;
    return self;
}

- (void)windowCreated:(NSWindow*)window
{
    assert(window);
    
    [window setDelegate:self];
    
    assert(self->window_count < ~0u);
    self->window_count += 1;
    
    [window registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, nil]] ;
}

- (void)windowWillClose:(NSNotification*)notification
{
}

- (BOOL)windowShouldClose:(NSWindow*)window
{
    [window setDelegate:nil];
    
    if (self->window_count == 0)
    {
        [NSApp terminate:self];
        return false;
    }
    
    return true;
}

- (void)windowDidResize:(NSNotification*)notification
{
    g_window_resize = true;
    pen_window_resize();
}

- (void)windowDidBecomeKey:(NSNotification*)notification
{
}

- (void)windowDidResignKey:(NSNotification*)notification
{
}

@end
