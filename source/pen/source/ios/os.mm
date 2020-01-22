// os.mm
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "os.h"
#include "pen.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"

#ifdef PEN_RENDERER_METAL
#import <MetalKit/MetalKit.h>
#import <Metal/Metal.h>
#else
#define GLES_SILENCE_DEPRECATION
#import <GLKit/GLKit.h>
#endif

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

// global externs
pen::user_info pen_user_info;
a_u64          g_frame_index = { 0 };

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
        f32                 wscale;
        pen_app_delegate*   app_delegate;
    };
    os_context s_context;
}

namespace pen
{
    void renderer_init(void*, bool);
    bool renderer_dispatch();
}

@implementation pen_app_delegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    @autoreleasepool {
        s_context.app_delegate = self;
        s_context.wframe = [[UIScreen mainScreen] bounds]; // size in "points"
        s_context.wscale = [[UIScreen mainScreen] nativeScale]; // scale for retina dimensions

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

        // create metal delegate
        self.mtk_renderer = [[pen_mtk_renderer alloc] initWithView:self.mtk_view];
        [self.mtk_view setDelegate:self.mtk_renderer];
        
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

}
- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool {
        pen::renderer_dispatch();
        pen::os_update();
        g_frame_index++;
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
}

- (int)getTouchId:(UITouch*) touch
{

}
- (void)handleTouch:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{

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
            thread_info.flags = pen::PEN_CREATE_AUDIO_THREAD;
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
    }

    void window_set_frame(const window_frame& f)
    {
    }
}
