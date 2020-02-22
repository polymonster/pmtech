// os.mm
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "os.h"
#include "pen.h"
#include "renderer.h"
#include "renderer_shared.h"
#include "threads.h"
#include "timer.h"
#include "console.h"
#include "input.h"

#ifdef PEN_RENDERER_METAL
#import <MetalKit/MetalKit.h>
#import <Metal/Metal.h>
#else
#define GLES_SILENCE_DEPRECATION
#import <GLKit/GLKit.h>
#endif

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

// the last 2 global externs \o/
pen::user_info               pen_user_info;
pen::window_creation_params  pen_window;

// objc interfaces
@interface pen_mtk_renderer : NSObject<MTKViewDelegate>
- (instancetype)initWithView:(nonnull MTKView*)view;
@end

@interface pen_view_controller : UIViewController
- (void) viewWasDoubleTapped:(id)sender;
- (BOOL) prefersHomeIndicatorAutoHidden;
@end

@interface pen_app_delegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow*             window;
@property (strong, nonatomic) MTKView*              mtk_view;
@property (strong, nonatomic) pen_mtk_renderer*     mtk_renderer;
@property (strong, nonatomic) pen_view_controller*  view_controller;
@end

namespace
{
    struct os_context
    {
        CGRect              wframe;
        CGSize              wsize;
        f32                 wscale;
        pen_app_delegate*   app_delegate;
    };
    os_context s_context;
    
    void update_pen_window()
    {
        // updates pen window size
        pen::_renderer_resize_backbuffer(s_context.wsize.width, s_context.wsize.height);
    }
}

namespace pen
{
    void renderer_init(void*, bool);
    bool renderer_dispatch();
}

@implementation pen_app_delegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    // setup base systems
    pen::timer_system_intialise();
    
    @autoreleasepool {
        s_context.app_delegate = self;
        s_context.wframe = [[UIScreen mainScreen] bounds]; // size in "points"
        s_context.wscale = [[UIScreen mainScreen] nativeScale]; // scale for retina dimensions
        
        s_context.wsize.width = s_context.wframe.size.width * s_context.wscale;
        s_context.wsize.height = s_context.wframe.size.height * s_context.wscale;
        
        update_pen_window();

        self.window = [[UIWindow alloc] initWithFrame:s_context.wframe];
        [self.window setBackgroundColor:[UIColor blackColor]];
        [self.window makeKeyAndVisible];
        
        // create metal view
        self.mtk_view = [[MTKView alloc] initWithFrame:s_context.wframe];
        [self.mtk_view setDevice:MTLCreateSystemDefaultDevice()];
        [self.mtk_view setPreferredFramesPerSecond:60];
        [self.mtk_view setColorPixelFormat:MTLPixelFormatBGRA8Unorm];
        [self.mtk_view setDepthStencilPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
        [self.mtk_view setUserInteractionEnabled:YES];
        //[self.mtk_view setSampleCount:pen_window.sample_count];

        // create metal delegate
        self.mtk_renderer = [[pen_mtk_renderer alloc] initWithView:self.mtk_view];
        [self.mtk_view setDelegate:self.mtk_renderer];
        [self.mtk_view setFramebufferOnly:NO];
        
        // create view controller
        self.view_controller = [[pen_view_controller alloc] initWithNibName:nil bundle:nil];
         
        // hook up
        [self.view_controller setView:self.mtk_view];
        [self.window setRootViewController:self.view_controller];
        self.view_controller.view.multipleTouchEnabled = YES;
        
        return YES;
    }
}
@end

@implementation pen_mtk_renderer
- (instancetype)initWithView:(nonnull MTKView *)view
{
    [super init];
    pen::renderer_init((void*)view, false);
    return self;
}
- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    s_context.wsize = size;
    update_pen_window();
}
- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool {
        pen::renderer_dispatch();
        pen::os_update();
    }
}
@end

@implementation pen_view_controller
- (void) viewWasTapped:(id)sender
{
}

- (void) viewWasDoubleTapped:(id)sender
{
}

- (BOOL) prefersHomeIndicatorAutoHidden
{
    return YES;
}

- (void)didReceiveMemoryWarning
{
    PEN_LOG("[warning] ios received memory warning.");
}

- (int)getTouchId:(UITouch*) touch
{
    return 0;
}

- (void)handleTouch:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    for(UITouch* t in touches)
    {
        CGPoint touch_point = [t locationInView:t.view];
        
        pen::input_set_mouse_pos(touch_point.x * s_context.wscale, touch_point.y * s_context.wscale);
        
        switch(t.phase)
        {
            case UITouchPhaseBegan:
            {
                pen::input_set_mouse_down(PEN_MOUSE_L);
                break;
            }
            case UITouchPhaseMoved:
            {
                pen::input_set_mouse_down(PEN_MOUSE_L);
                break;
            }
            case UITouchPhaseEnded:
            {
                pen::input_set_mouse_up(PEN_MOUSE_L);
                break;
            }
            case UITouchPhaseCancelled:
            {
                pen::input_set_mouse_up(PEN_MOUSE_L);
                break;
            }
            case UITouchPhaseStationary:
                break;
            default:
                break;
        }
    }
}
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self handleTouch:touches withEvent:event];
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self handleTouch:touches withEvent:event];
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self handleTouch:touches withEvent:event];
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self handleTouch:touches withEvent:event];
}
@end

int main(int argc, char* argv[])
{
    NSString* str = NSStringFromClass([pen_app_delegate class]);

    pen::pen_creation_params pc  = pen::pen_entry(argc, argv);
    pen_window.width = pc.window_width;
    pen_window.height = pc.window_height;
    pen_window.window_title = pc.window_title;
    pen_window.sample_count = pc.window_sample_count;

    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, str);
    }
}

namespace pen
{
    void* window_get_primary_display_handle()
    {
        return nullptr;
    }

    const c8* os_path_for_resource(const c8* filename)
    {
        NSString* ns_filename = [[NSString alloc] initWithUTF8String:filename];
        NSString* test = [[NSBundle mainBundle] pathForResource:ns_filename ofType:nil];
        [ns_filename release];
        return test.UTF8String;
    }

    bool os_update()
    {
        static bool thread_started = false;
        if (!thread_started)
        {
            // audio, user thread etc
            pen::default_thread_info thread_info;
            pen::jobs_create_default(thread_info);
            thread_started = true;
        }

        return true;
    }

    void os_terminate(u32 error_code)
    {
        // stub
    }

    bool input_undo_pressed()
    {
        return false;
    }

    bool input_redo_pressed()
    {
        return false;
    }

    void window_get_frame(window_frame& f)
    {
        f = {
            (u32)s_context.wframe.origin.x,
            (u32)s_context.wframe.origin.y,
            (u32)s_context.wframe.size.width,
            (u32)s_context.wframe.size.height
        };
    }

    void window_set_frame(const window_frame& f)
    {
        // not possible on ios
    }

    const user_info& os_get_user_info()
    {
        return pen_user_info;
    }

    void window_get_size(s32& width, s32& height)
    {
        width = pen_window.width;
        height = pen_window.height;
    }

    f32 window_get_aspect()
    {
        return (f32)pen_window.width / (f32)pen_window.height;
    }

    const c8* window_get_title()
    {
        return pen_window.window_title;
    }
    
    hash_id window_get_id()
    {
        static hash_id window_id = PEN_HASH(pen_window.window_title);
        return window_id;
    }
}
