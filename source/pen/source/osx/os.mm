#import <AppKit/NSPasteboard.h>
#import <Cocoa/Cocoa.h>

#include "console.h"
#include "input.h"
#include "pen.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"
#include "str/Str.h"
#include "str_utilities.h"
#include "data_struct.h"
#include "os.h"

// This file contains gl and metal context creation, input handling and os events.

// global stuff for window graphics api sync
extern pen::window_creation_params pen_window;
extern a_u8                        g_window_resize;
int                                g_rs = 0;

// pen required externs
pen::user_info pen_user_info;
extern PEN_TRV pen::user_entry(void* params);

namespace pen
{
    void renderer_init(void*, bool);
}

namespace
{
    NSWindow* _window;
    bool pen_terminate_app = false;
}

@interface app_delegate : NSObject <NSApplicationDelegate>
{
    bool terminated;
}

+ (app_delegate*)shared_delegate;
- (id)init;
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender;
- (bool)applicationHasTerminated;

@end

@interface window_delegate : NSObject <NSWindowDelegate>
{
    uint32_t window_count;
}

+ (window_delegate*)shared_delegate;
- (id)init;
- (void)windowCreated:(NSWindow*)window;
- (void)windowWillClose:(NSNotification*)notification;
- (BOOL)windowShouldClose:(NSWindow*)window;
- (void)windowDidResize:(NSNotification*)notification;
- (void)windowDidBecomeKey:(NSNotification*)notification;
- (void)windowDidResignKey:(NSNotification*)notification;
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender;
- (NSDragOperation)draggingEnded:(id<NSDraggingInfo>)sender;
- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender;

@end

#ifdef PEN_RENDERER_METAL // Metal Context
#import <MetalKit/MetalKit.h>
#define create_renderer_context create_metal_context

@interface metal_delegate : NSObject<MTKViewDelegate>

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)mtkView;

@end

namespace
{
    MTKView* _metal_view;
}

@implementation metal_delegate

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)mtkView
{
    self = [super init];
    return self;
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    // todo resize
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    if (!pen::renderer_dispatch())
        pen::thread_sleep_us(100);
}

@end

void create_metal_context()
{
    NSRect frame = [[_window contentView] bounds];
    _metal_view = [[MTKView alloc] initWithFrame:frame device:MTLCreateSystemDefaultDevice()];
    
    metal_delegate* dg = [[metal_delegate alloc] initWithMetalKitView: _metal_view];
    _metal_view.delegate = dg;
    
    //assign metal view to window sub view
    [_window.contentView addSubview:_metal_view];
}

void pen_window_resize()
{
    g_rs = 10;
    
    NSRect view_rect = [[_window contentView] bounds];
    
    if (_metal_view.frame.size.width == view_rect.size.width && _metal_view.frame.size.height == view_rect.size.height)
        return;
    
    [_metal_view setFrameSize:view_rect.size];
    
    pen_window.width = view_rect.size.width;
    pen_window.height = view_rect.size.height;
}

void run()
{
    // passes metal view to renderer, renderer dispatch and os update will be called from drawInMTKView
    pen::renderer_init(_metal_view, false);
    
    for (;;)
    {
        if (!pen::os_update())
            break;
    }
}

#else  // OpenGL Context
#import <OpenGL/gl3.h>
#define PEN_GL_PROFILE_VERSION NSOpenGLProfileVersion3_2Core
#define create_renderer_context create_gl_context
namespace
{
    NSOpenGLView*        _gl_view;
    NSOpenGLContext*     _gl_context;
    NSOpenGLPixelFormat* _pixel_format;
}

void create_gl_context()
{
    u32 sample_buffers = pen_window.sample_count ? 1 : 0;

    NSOpenGLPixelFormatAttribute pixel_format_attribs[] = {
        // gl 3.2
        NSOpenGLPFAOpenGLProfile,
        PEN_GL_PROFILE_VERSION,

        // msaa
        NSOpenGLPFAMultisample,
        NSOpenGLPFASampleBuffers,
        sample_buffers,
        NSOpenGLPFASamples,
        pen_window.sample_count,

        // RGBA D24S8
        NSOpenGLPFAColorSize,
        24,
        NSOpenGLPFAAlphaSize,
        8,
        NSOpenGLPFADepthSize,
        24,
        NSOpenGLPFAStencilSize,
        8,

        // double buffered, HAL
        NSOpenGLPFADoubleBuffer,
        true,
        NSOpenGLPFAAccelerated,
        true,
        NSOpenGLPFANoRecovery,
        true,
        0,
        0,
    };

    _pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:pixel_format_attribs];

    NSRect glViewRect = [[_window contentView] bounds];

    NSOpenGLView* glView = [[NSOpenGLView alloc] initWithFrame:glViewRect pixelFormat:_pixel_format];

    [[glView superview] setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    [_pixel_format release];

    [_window.contentView addSubview:glView];

    NSOpenGLContext* glContext = [glView openGLContext];

    [glContext makeCurrentContext];
    GLint interval = 1;
    [glContext setValues:&interval forParameter:NSOpenGLCPSwapInterval];

    _gl_view = glView;
    _gl_context = glContext;
}

void pen_make_gl_context_current()
{
    [_gl_context makeCurrentContext];
}

void pen_gl_swap_buffers()
{
    if (g_rs <= 0)
        [_gl_context flushBuffer];
}

void pen_window_resize()
{
    g_rs = 10;
    
    NSRect view_rect = [[_window contentView] bounds];
    
    if (_gl_view.frame.size.width == view_rect.size.width && _gl_view.frame.size.height == view_rect.size.height)
        return;
    
    [_gl_view setFrameSize:view_rect.size];
    
    pen_window.width = view_rect.size.width;
    pen_window.height = view_rect.size.height;
}

void run()
{
    // enters render loop and wait for jobs, will call os_update
    pen::renderer_init(nullptr, true);
}
#endif

namespace
{
    enum os_cmd_id
    {
        OS_CMD_NULL = 0,
        OS_CMD_SET_WINDOW_FRAME
    };

    struct os_cmd
    {
        u32 cmd_index;

        union {
            struct
            {
                pen::window_frame frame;
            };
        };
    };

    pen_ring_buffer<os_cmd> s_cmd_buffer;
    
    void users()
    {
        NSString* ns_full_user_name = NSFullUserName();
        pen_user_info.full_user_name = [ns_full_user_name UTF8String];
        
        NSString* ns_user_name = NSUserName();
        pen_user_info.user_name = [ns_user_name UTF8String];
    }
    
    void get_mouse_pos(f32& x, f32& y)
    {
        NSRect  original_frame = [_window frame];
        NSPoint location = [_window mouseLocationOutsideOfEventStream];
        NSRect  adjust_frame = [_window contentRectForFrameRect:original_frame];
        
        x = location.x;
        y = (int)adjust_frame.size.height - location.y;
    }
    
    void handle_modifiers(NSEvent* event)
    {
        u32 flags = [event modifierFlags];
        
        if (flags & NSEventModifierFlagShift)
        {
            pen::input_set_key_down(PK_SHIFT);
        }
        else
        {
            pen::input_set_key_up(PK_SHIFT);
        }
        
        if (flags & NSEventModifierFlagOption)
        {
            pen::input_set_key_down(PK_MENU);
        }
        else
        {
            pen::input_set_key_up(PK_MENU);
        }
        
        if (flags & NSEventModifierFlagControl)
        {
            pen::input_set_key_down(PK_CONTROL);
        }
        else
        {
            pen::input_set_key_up(PK_CONTROL);
        }
        
        if (flags & NSEventModifierFlagCommand)
        {
            pen::input_set_key_down(PK_COMMAND);
        }
        else
        {
            pen::input_set_key_up(PK_COMMAND);
        }
    }
    
    void handle_key_event(NSEvent* event, bool down)
    {
        handle_modifiers(event);
        
        NSString* key = [event charactersIgnoringModifiers];
        
        if ([key length] == 0)
        {
            return;
        }
        
        unichar key_char = [key characterAtIndex:0];
        
        if (key_char < 256)
        {
            u32 mapped_key_char = key_char;
            
            u32 vk = 511;
            
            if (mapped_key_char >= 'a' && mapped_key_char <= 'z')
            {
                vk = PK_A + (mapped_key_char - 'a');
            }
            
            if (mapped_key_char >= '0' && mapped_key_char <= '9')
            {
                vk = PK_0 + (mapped_key_char - '0');
            }
            
            if (mapped_key_char == 127)
            {
                mapped_key_char = 8;
                vk = PK_BACK;
            }
            
            if (mapped_key_char == 32)
                vk = PK_SPACE;
            
            if (down)
            {
                pen::input_set_unicode_key_down(mapped_key_char);
                
                pen::input_set_key_down(vk);
            }
            else
            {
                pen::input_set_unicode_key_up(mapped_key_char);
                
                pen::input_set_key_up(vk);
            }
        }
        else
        {
            u32 penk = 0;
            
            switch (key_char)
            {
                case NSF1FunctionKey:
                    penk = PK_F1;
                    break;
                case NSF2FunctionKey:
                    penk = PK_F2;
                    break;
                case NSF3FunctionKey:
                    penk = PK_F3;
                    break;
                case NSF4FunctionKey:
                    penk = PK_F4;
                    break;
                case NSF5FunctionKey:
                    penk = PK_F5;
                    break;
                case NSF6FunctionKey:
                    penk = PK_F6;
                    break;
                case NSF7FunctionKey:
                    penk = PK_F7;
                    break;
                case NSF8FunctionKey:
                    penk = PK_F8;
                    break;
                case NSF9FunctionKey:
                    penk = PK_F9;
                    break;
                case NSF10FunctionKey:
                    penk = PK_F10;
                    break;
                case NSF11FunctionKey:
                    penk = PK_F11;
                    break;
                case NSF12FunctionKey:
                    penk = PK_F12;
                    break;
                    
                case NSLeftArrowFunctionKey:
                    penk = PK_LEFT;
                    break;
                case NSRightArrowFunctionKey:
                    penk = PK_RIGHT;
                    break;
                case NSUpArrowFunctionKey:
                    penk = PK_UP;
                    break;
                case NSDownArrowFunctionKey:
                    penk = PK_DOWN;
                    break;
                    
                case NSPageUpFunctionKey:
                    penk = PK_NEXT;
                    break;
                case NSPageDownFunctionKey:
                    penk = PK_PRIOR;
                    break;
                case NSHomeFunctionKey:
                    penk = PK_HOME;
                    break;
                case NSEndFunctionKey:
                    penk = PK_END;
                    break;
                    
                case NSDeleteFunctionKey:
                    penk = PK_BACK;
                    break;
                case NSDeleteCharFunctionKey:
                    penk = PK_BACK;
                    break;
                    
                case NSPrintScreenFunctionKey:
                    penk = PK_SNAPSHOT;
                    break;
            }
            
            if (down)
            {
                pen::input_set_key_down(penk);
            }
            else
            {
                pen::input_set_key_up(penk);
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
                    get_mouse_pos(x, y);
                    pen::input_set_mouse_pos(x, y);
                    return true;
                }
                    
                case NSEventTypeLeftMouseDown:
                    pen::input_set_mouse_down(PEN_MOUSE_L);
                    break;
                case NSEventTypeRightMouseDown:
                    pen::input_set_mouse_down(PEN_MOUSE_R);
                    break;
                case NSEventTypeOtherMouseDown:
                    pen::input_set_mouse_down(PEN_MOUSE_M);
                    break;
                    
                case NSEventTypeLeftMouseUp:
                    pen::input_set_mouse_up(PEN_MOUSE_L);
                    break;
                case NSEventTypeRightMouseUp:
                    pen::input_set_mouse_up(PEN_MOUSE_R);
                    break;
                case NSEventTypeOtherMouseUp:
                    pen::input_set_mouse_up(PEN_MOUSE_M);
                    break;
                    
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
}

int main(int argc, char** argv)
{
    // get working dir
    Str working_dir = argv[0];
    working_dir = pen::str_normalise_filepath(working_dir);

    s_cmd_buffer.create(32);

    // strip exe and go back 2 \contents\macos\exe
    for (u32 i = 0, pos = 0; i < 4; ++i)
    {
        pos = pen::str_find_reverse(working_dir, "/", pos - 1);

        if (i == 3)
            working_dir = pen::str_substr(working_dir, 0, pos + 1);
    }

    pen_user_info.working_directory = working_dir.c_str();

    // window creation
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    NSLog(@"NSApp=%@", NSApp);
    [NSApplication sharedApplication];
    NSLog(@"NSApp=%@", NSApp);

    id dg = [app_delegate shared_delegate];
    [NSApp setDelegate:dg];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp finishLaunching];

    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationWillFinishLaunchingNotification object:NSApp];

    [[NSNotificationCenter defaultCenter] postNotificationName:NSApplicationDidFinishLaunchingNotification object:NSApp];

    NSRect frame = NSMakeRect(0, 0, pen_window.width, pen_window.height);

    NSUInteger style_mask =
        NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

    _window = [[NSWindow alloc] initWithContentRect:frame styleMask:style_mask backing:NSBackingStoreBuffered defer:NO];

    [_window makeKeyAndOrderFront:_window];

    id wd = [window_delegate shared_delegate];
    [_window setDelegate:wd];
    [_window setTitle:[NSString stringWithUTF8String:pen_window.window_title]];
    [_window setAcceptsMouseMovedEvents:YES];
    [_window center];

    [_window registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, nil]];

    // creates an opengl or metal rendering context
    create_renderer_context();

    [pool drain];

    // os stuff
    users();

    // init systems
    pen::timer_system_intialise();
    pen::input_gamepad_init();
    
    // invoke renderer specific update for main thread
    run();
}

namespace pen
{
    bool os_update()
    {
        static bool thread_started = false;
        if (!thread_started)
        {
            // audio, user thread etc
            pen::default_thread_info thread_info;
            thread_info.flags = pen::PEN_CREATE_AUDIO_THREAD;
            pen::thread_create_default_jobs(thread_info);
            thread_started = true;
        }

        // Window / event loop
        NSAutoreleasePool* _pool = [[NSAutoreleasePool alloc] init];

        [NSApp updateWindows];

        while (1)
        {
            NSEvent* peek_event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                     untilDate:[NSDate distantPast] // do not wait for event
                                                        inMode:NSDefaultRunLoopMode
                                                       dequeue:YES];

            if (!peek_event)
            {
                break;
            }

            if (!handle_event(peek_event))
            {
                [NSApp sendEvent:peek_event];
            }

            break;
        }

        // input update
        f32 x, y;
        get_mouse_pos(x, y);
        pen::input_set_mouse_pos(x, y);

        if (g_rs > 0)
        {
            pen::input_set_mouse_up(PEN_MOUSE_L);
            pen::input_set_mouse_up(PEN_MOUSE_R);
            pen::input_set_mouse_up(PEN_MOUSE_M);
        }
        
        input_gamepad_update();

        [_pool drain];
        g_rs--;

        if (pen_terminate_app)
        {
            if (pen::thread_terminate_jobs())
                return false;
        }

        os_cmd* cmd = s_cmd_buffer.get();
        while (cmd)
        {
            // process cmd
            switch (cmd->cmd_index)
            {
                case OS_CMD_SET_WINDOW_FRAME:
                {
                    NSRect frame = [_window frame];

                    frame.origin.x = cmd->frame.x;
                    frame.origin.y = cmd->frame.y;
                    frame.size.width = cmd->frame.width;
                    frame.size.height = cmd->frame.height;

                    [_window setFrame:frame display:YES animate:NO];
                }
                break;
                default:
                    break;
            }

            // get next
            cmd = s_cmd_buffer.get();
        }

        return true;
    }

    const c8* os_path_for_resource(const c8* filename)
    {
        return filename;
    }

    void os_set_cursor_pos(u32 client_x, u32 client_y)
    {
    }

    void os_show_cursor(bool show)
    {
    }

    bool input_undo_pressed()
    {
        return input_key(PK_COMMAND) && input_key(PK_Z);
    }

    bool input_redo_pressed()
    {
        return input_key(PK_COMMAND) && input_key(PK_SHIFT) && input_get_unicode_key('Z');
    }

    void window_get_size(s32& width, s32& height)
    {
        NSScreen* screen = [_window screen];
        NSRect    rect = [screen frame];

        width = rect.size.width;
        height = rect.size.height;
    }

    void window_set_size(s32 width, s32 height)
    {
        NSRect frame = [_window frame];
        frame.size.width = width;
        frame.size.height = height;

        [_window setFrame:frame display:YES animate:NO];
    }

    void window_get_frame(window_frame& f)
    {
        NSRect rect = [_window frame];

        f.x = rect.origin.x;
        f.y = rect.origin.y;
        f.width = rect.size.width;
        f.height = rect.size.height;
    }

    void window_set_frame(const window_frame& f)
    {
        os_cmd cmd;
        cmd.cmd_index = OS_CMD_SET_WINDOW_FRAME;
        cmd.frame = f;

        s_cmd_buffer.put(cmd);
    }

    void* window_get_primary_display_handle()
    {
        return (void*)_window;
    }
}

// Objective C Stuff

@implementation app_delegate

+ (app_delegate*)shared_delegate
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

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
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

@implementation window_delegate

+ (window_delegate*)shared_delegate
{
    static id d = [window_delegate new];
    return d;
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

    [window registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, nil]];
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

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
    return NSDragOperationCopy;
}

- (NSDragOperation)draggingEnded:(id<NSDraggingInfo>)sender
{
    return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    NSPasteboard* pboard = [sender draggingPasteboard];
    NSArray*      files = [pboard propertyListForType:NSFilenamesPboardType];

    for (u32 i = 0; i < files.count; ++i)
    {
        NSString* newString = [files objectAtIndex:i];
        PEN_LOG("%s", newString.UTF8String);
    }

    return NO;
}

@end
