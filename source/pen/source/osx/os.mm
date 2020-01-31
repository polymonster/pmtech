// os.mm
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

// This file contains gl and metal context creation, input handling and os events.

#define GL_SILENCE_DEPRECATION

#import <AppKit/NSPasteboard.h>
#import <Cocoa/Cocoa.h>

#include "renderer_shared.h"
#include "console.h"
#include "data_struct.h"
#include "input.h"
#include "os.h"
#include "pen.h"
#include "renderer.h"
#include "str/Str.h"
#include "str_utilities.h"
#include "threads.h"
#include "timer.h"

#include <map>

// pen required externs / globals.. trying to remove these
pen::user_info                      pen_user_info;
extern PEN_TRV                      pen::user_entry(void* params);
extern pen::window_creation_params  pen_window;

a_u64                               g_frame_index = { 0 };

namespace pen
{
    void renderer_init(void*, bool);
}

namespace
{
    struct os_context
    {
        pen::window_frame frame;
        u32               return_code = 0;
    };
    os_context s_ctx;
    
    // todo.. move into context
    NSWindow* _window;
    bool      pen_terminate_app = false;
    
    void _update_window_frame()
    {
        NSRect rect = [_window frame];

        s_ctx.frame.x = rect.origin.x;
        s_ctx.frame.y = rect.origin.y;
        s_ctx.frame.width = rect.size.width;
        s_ctx.frame.height = rect.size.height;
    }
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

//
// Metal Context
//

#ifdef PEN_RENDERER_METAL
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

id<CAMetalDrawable> g_current_drawable;

#define create_renderer_context create_metal_context

@interface metal_delegate : NSObject <MTKViewDelegate>

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView*)mtkView;

@end

namespace
{
    MTKView* _metal_view;
}

@implementation metal_delegate

- (nonnull instancetype)initWithMetalKitView:(nonnull MTKView*)mtkView
{
    self = [super init];
    return self;
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size
{
    pen::_renderer_resize_backbuffer(size.width, size.height);
    _update_window_frame();
}

- (void)drawInMTKView:(nonnull MTKView*)view
{
    @autoreleasepool {
        if (!pen::renderer_dispatch())
            pen::thread_sleep_us(100);
            g_frame_index++;
    }
}

@end

void create_metal_context()
{
    NSRect frame = [[_window contentView] bounds];
    _metal_view = [[MTKView alloc] initWithFrame:frame device:MTLCreateSystemDefaultDevice()];
    _metal_view.depthStencilPixelFormat = MTLPixelFormatDepth24Unorm_Stencil8;
    _metal_view.sampleCount = pen_window.sample_count;

    metal_delegate* dg = [[metal_delegate alloc] initWithMetalKitView:_metal_view];
    _metal_view.delegate = dg;

    //assign metal view to window sub view
    [_window.contentView addSubview:_metal_view];
}

void pen_window_resize()
{
    NSRect view_rect = [[_window contentView] bounds];
    [_metal_view setFrameSize:view_rect.size];
    pen::_renderer_resize_backbuffer(view_rect.size.width, view_rect.size.height);
    
    // cancel clicks to prevent incorrect interactions
    pen::input_set_mouse_up(PEN_MOUSE_L);
    pen::input_set_mouse_up(PEN_MOUSE_R);
    pen::input_set_mouse_up(PEN_MOUSE_M);
    
    _update_window_frame();
}

void run()
{
    pen::renderer_init(_metal_view, false);
    
    for (;;)
    {
        if (!pen::os_update())
            break;

        pen::thread_sleep_us(100);
    }
}
#else

//
// OpenGL Context
//

#import <OpenGL/gl3.h>
#define PEN_GL_PROFILE_VERSION NSOpenGLProfileVersion4_1Core
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
        false,
        NSOpenGLPFANoRecovery,
        false,
        NSOpenGLPFAAllowOfflineRenderers,
        true,
        // end
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
    [_gl_context flushBuffer];
}

void pen_window_resize()
{
    NSRect view_rect = [[_window contentView] bounds];
    [_gl_view setFrameSize:view_rect.size];
    pen::_renderer_resize_backbuffer(view_rect.size.width, view_rect.size.height);
    _update_window_frame();
}

void run()
{
    // enters render loop and wait for jobs, will call os_update
    pen::renderer_init(nullptr, true);
}
#endif

namespace
{
    std::map<u16, virtual_key> k_key_map = {
        { 0x1D, PK_0 },
        { 0x12, PK_1 },
        { 0x13, PK_2 },
        { 0x14, PK_3 },
        { 0x15, PK_4 },
        { 0x17, PK_5 },
        { 0x16, PK_6 },
        { 0x1A, PK_7 },
        { 0x1C, PK_8 },
        { 0x19, PK_9 },
        { 0x00, PK_A },
        { 0x0B, PK_B },
        { 0x08, PK_C },
        { 0x02, PK_D },
        { 0x0E, PK_E },
        { 0x03, PK_F },
        { 0x05, PK_G },
        { 0x04, PK_H },
        { 0x22, PK_I },
        { 0x26, PK_J },
        { 0x28, PK_K },
        { 0x25, PK_L },
        { 0x2E, PK_M },
        { 0x2D, PK_N },
        { 0x1F, PK_O },
        { 0x23, PK_P },
        { 0x0C, PK_Q },
        { 0x0F, PK_R },
        { 0x01, PK_S },
        { 0x11, PK_T },
        { 0x20, PK_U },
        { 0x09, PK_V },
        { 0x0D, PK_W },
        { 0x07, PK_X },
        { 0x10, PK_Y },
        { 0x06, PK_Z },
        { 0x52, PK_NUMPAD0 },
        { 0x53, PK_NUMPAD1 },
        { 0x54, PK_NUMPAD2 },
        { 0x55, PK_NUMPAD3 },
        { 0x56, PK_NUMPAD4 },
        { 0x57, PK_NUMPAD5 },
        { 0x58, PK_NUMPAD6 },
        { 0x59, PK_NUMPAD7 },
        { 0x5B, PK_NUMPAD8 },
        { 0x5C, PK_NUMPAD9 },
        { 0x43, PK_MULTIPLY },
        { 0x45, PK_ADD },
        { 0x4E, PK_SUBTRACT },
        { 0x41, PK_DECIMAL },
        { 0x4B, PK_DIVIDE },
        { 0x7A, PK_F1 },
        { 0x78, PK_F2 },
        { 0x63, PK_F3 },
        { 0x76, PK_F4 },
        { 0x60, PK_F5 },
        { 0x61, PK_F6 },
        { 0x62, PK_F7 },
        { 0x64, PK_F8 },
        { 0x65, PK_F9 },
        { 0x6D, PK_F10 },
        { 0x67, PK_F11 },
        { 0x6F, PK_F12 },
        { 0x03, PK_CANCEL },
        { 0x33, PK_BACK },
        { 0x30, PK_TAB },
        { 0x0C, PK_CLEAR},
        { 0x24, PK_RETURN },
        { 0x10, PK_SHIFT },
        { 0x11, PK_CONTROL },
        { 0x6E, PK_MENU },
        { 0x39, PK_CAPITAL },
        { 0x35, PK_ESCAPE },
        { 0x31, PK_SPACE },
        { 0x79, PK_PRIOR },
        { 0x79, PK_NEXT },
        { 0x77, PK_END },
        { 0x73, PK_HOME },
        { 0x7B, PK_LEFT },
        { 0x7E, PK_UP },
        { 0x7C, PK_RIGHT },
        { 0x7D, PK_DOWN },
        { 0x72, PK_INSERT },
        { 0x75, PK_DELETE },
        { 0x36, PK_COMMAND },
        { 0x21, PK_OPEN_BRACKET },
        { 0x1E, PK_CLOSE_BRACKET },
        { 0x29, PK_SEMICOLON },
        { 0x27, PK_APOSTRAPHE },
        { 0x2A, PK_BACK_SLASH },
        { 0x2C, PK_FORWARD_SLASH },
        { 0x2B, PK_COMMA },
        { 0x2F, PK_PERIOD },
        { 0x1B, PK_MINUS },
        { 0x18, PK_EQUAL },
        { 0x32, PK_TILDE },
        { 0x36, PK_GRAVE }
    };
    
    enum os_cmd_id
    {
        OS_CMD_NULL = 0,
        OS_CMD_SET_WINDOW_FRAME,
        OS_CMD_SET_WINDOW_SIZE
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

    pen::ring_buffer<os_cmd> s_cmd_buffer;

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
        
        if (flags & NSEventModifierFlagCapsLock)
        {
            pen::input_set_key_down(PK_CAPITAL);
        }
        else
        {
            pen::input_set_key_up(PK_CAPITAL);
        }
    }

    void handle_key_event(NSEvent* event, bool down)
    {
        handle_modifiers(event);

        NSString* key = [event charactersIgnoringModifiers];

        u16 key_code = [event keyCode];
        u32 pen_key_code = k_key_map[key_code];
        
        // text input
        if([key length] > 0)
        {
            if(down)
                pen::input_add_unicode_input([key UTF8String]);
        }
        
        // vks
        if(k_key_map.find(key_code) != k_key_map.end())
        {
            if (down)
            {
                pen::input_set_key_down(pen_key_code);
            }
            else
            {
                pen::input_set_key_up(pen_key_code);
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
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
    
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

    // args
    if (argc > 1)
    {
        if (strcmp(argv[1], "-test") == 0)
        {
            // enter test
            pen::renderer_test_enable();
        }
    }

    // window creation
    [NSApplication sharedApplication];

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
    _update_window_frame();

    // creates an opengl or metal rendering context
    create_renderer_context();

    // os stuff
    users();

    // init systems
    pen::timer_system_intialise();
    pen::input_gamepad_init();
    
    [pool drain];

    // invoke renderer specific update for main thread
    run();
        
    return s_ctx.return_code;
}

//
// pen os api
//

namespace pen
{
    bool os_update()
    {
        NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];
        
        static bool thread_started = false;
        if (!thread_started)
        {
            // audio, user thread etc
            pen::default_thread_info thread_info;
            thread_info.flags = pen::PEN_CREATE_AUDIO_THREAD;
            pen::jobs_create_default(thread_info);
            thread_started = true;
        }

        [NSApp updateWindows];
        
        for(;;)
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
        }

        // input update
        f32 x, y;
        get_mouse_pos(x, y);
        pen::input_set_mouse_pos(x, y);
        input_gamepad_update();

        // process commands
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
                case OS_CMD_SET_WINDOW_SIZE:
                {
                    NSRect frame = [_window frame];
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
        
        [pool drain];
        
        if (pen_terminate_app)
        {
            if (pen::jobs_terminate_all())
                return false;
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

    void os_terminate(u32 error_code)
    {
        s_ctx.return_code = error_code;
        pen_terminate_app = true;
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
        width = s_ctx.frame.width;
        height = s_ctx.frame.height;
    }

    void window_set_size(s32 width, s32 height)
    {
        os_cmd cmd;
        cmd.cmd_index = OS_CMD_SET_WINDOW_SIZE;
        cmd.frame.width = width;
        cmd.frame.height = height;

        s_cmd_buffer.put(cmd);
    }

    void window_get_frame(window_frame& f)
    {
        f = s_ctx.frame;
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

//
// Objective-C
//

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
