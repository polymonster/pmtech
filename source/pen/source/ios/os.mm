// os.mm
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#define GLES_SILENCE_DEPRECATION

#import <GLKit/GLKit.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

#include "os.h"
#include "pen.h"
#include "renderer.h"
#include "threads.h"
#include "timer.h"

namespace
{
    EAGLContext* _gl_context;
}

pen::user_info pen_user_info;

namespace pen
{
    void renderer_init(void*, bool);
    bool renderer_dispatch();
}

#ifdef PEN_RENDERER_METAL

#else
void pen_gl_swap_buffers()
{
    [_gl_context presentRenderbuffer:GL_RENDERBUFFER];
}

void pen_make_gl_context_current()
{
    [EAGLContext setCurrentContext:_gl_context];
}

extern pen::window_creation_params pen_window;
extern PEN_TRV                     pen::user_entry(void* params);

@interface gl_app_delegate : UIResponder <UIApplicationDelegate, GLKViewDelegate, GLKViewControllerDelegate>

@property(strong, nonatomic) UIWindow* window;

@end

@interface gl_app_delegate ()

@end

@implementation gl_app_delegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    CGRect window_rect = [[UIScreen mainScreen] bounds];

    self.window = [[UIWindow alloc] initWithFrame:window_rect];

    self.window.backgroundColor = [UIColor whiteColor];
    [self.window makeKeyAndVisible];

    _gl_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    GLKView* view = [[GLKView alloc] initWithFrame:window_rect];
    view.context = _gl_context;
    view.delegate = self;
    view.drawableDepthFormat = GLKViewDrawableDepthFormat24;
    view.drawableStencilFormat = GLKViewDrawableStencilFormat8;

    [self.window addSubview:view];

    view.enableSetNeedsDisplay = NO;
    CADisplayLink* displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(render:)];
    [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];

    GLKViewController* viewController = [[GLKViewController alloc] initWithNibName:nil bundle:nil]; // 1
    viewController.view = view;
    viewController.delegate = self;
    viewController.preferredFramesPerSecond = 60;
    self.window.rootViewController = viewController;

    // hardcoded for iphone7 right now, as the correct dimension relies on an asset catalog
    pen_window.width = 750;   // window_rect.size.width;
    pen_window.height = 1334; // window_rect.size.height;

    pen::timer_system_intialise();

    pen_make_gl_context_current();
    pen_gl_swap_buffers();

    // Override point for customization after application launch.
    return YES;
}

- (void)glkView:(GLKView*)view drawInRect:(CGRect)rect
{
    // render thread
    static bool init = true;
    if (init)
    {
        init = false;
        pen::renderer_init(nullptr, false);
    }

    pen::renderer_dispatch();
    pen::os_update();
}

- (void)render:(CADisplayLink*)displayLink
{
    GLKView* view = [self.window.subviews objectAtIndex:0];
    [view display];
}

- (void)applicationWillResignActive:(UIApplication*)application
{
    // stub
}

- (void)applicationDidEnterBackground:(UIApplication*)application
{
    // stub
}

- (void)applicationWillEnterForeground:(UIApplication*)application
{
    // stub
}

- (void)applicationDidBecomeActive:(UIApplication*)application
{
    // stub
}

- (void)applicationWillTerminate:(UIApplication*)application
{
    // stub
}

@end
#endif

int main(int argc, char* argv[])
{
    // NSString* str = NSStringFromClass([gl_app_delegate class]);
    
    NSString* str = nil;
    
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
