#import <UIKit/UIKit.h>
#import <GLKit/GLKit.h>
#import <QuartzCore/QuartzCore.h>

#include "os.h"
#include "pen.h"
#include "threads.h"
#include "renderer.h"
#include "timer.h"

namespace
{
    CAEAGLLayer* _eaglLayer;
    EAGLContext* _gl_context;
}

namespace pen
{
    void renderer_init(void* user_data);
    bool renderer_dispatch();
}

void pen_gl_swap_buffers()
{
    [_gl_context presentRenderbuffer:GL_RENDERBUFFER];
}

void pen_make_gl_context_current()
{
    [EAGLContext setCurrentContext:_gl_context];
}

extern pen::window_creation_params pen_window;
extern PEN_TRV pen::user_entry( void* params );

@interface gl_app_delegate : UIResponder <UIApplicationDelegate, GLKViewDelegate, GLKViewControllerDelegate>

@property (strong, nonatomic) UIWindow *window;

@end

@interface gl_app_delegate ()

@end

@implementation gl_app_delegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    CGRect window_rect = [[UIScreen mainScreen] bounds];
    
    self.window = [[UIWindow alloc] initWithFrame:window_rect];
    
    self.window.backgroundColor = [UIColor whiteColor];
    [self.window makeKeyAndVisible];
    
    
    _gl_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
    GLKView *view = [[GLKView alloc] initWithFrame:window_rect];
    view.context = _gl_context;
    view.delegate = self;
    view.drawableDepthFormat = GLKViewDrawableDepthFormat24;
    view.drawableStencilFormat = GLKViewDrawableStencilFormat8;
    
    [self.window addSubview:view];
    
    view.enableSetNeedsDisplay = NO;
    CADisplayLink* displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(render:)];
    [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    
    GLKViewController * viewController = [[GLKViewController alloc] initWithNibName:nil bundle:nil]; // 1
    viewController.view = view;
    viewController.delegate = self;
    viewController.preferredFramesPerSecond = 60;
    self.window.rootViewController = viewController;
    
    pen::timer_system_intialise();
    
    // Override point for customization after application launch.
    return YES;
}

- (void)glkView:(GLKView *)view drawInRect:(CGRect)rect
{
    // render thread
    static bool init = true;
    if(init)
    {
        init = false;
        pen::renderer_init(nullptr);
    }
    
    pen::renderer_dispatch();
    pen::os_update();
}

- (void)render:(CADisplayLink*)displayLink {
    GLKView * view = [self.window.subviews objectAtIndex:0];
    [view display];
}

- (void)applicationWillResignActive:(UIApplication *)application
{
    // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
    // Use this method to pause ongoing tasks, disable timers, and invalidate graphics rendering callbacks. Games should use this method to pause the game.
}


- (void)applicationDidEnterBackground:(UIApplication *)application {
    // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
    // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
}


- (void)applicationWillEnterForeground:(UIApplication *)application {
    // Called as part of the transition from the background to the active state; here you can undo many of the changes made on entering the background.
}


- (void)applicationDidBecomeActive:(UIApplication *)application {
    // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
}


- (void)applicationWillTerminate:(UIApplication *)application {
    // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
}

@end

int main(int argc, char * argv[])
{
    NSString* str = NSStringFromClass([gl_app_delegate class]);
    
    @autoreleasepool
    {
        return UIApplicationMain(argc, argv, nil, str );
    }
}

namespace pen
{
    const c8* os_path_for_resource( const c8* filename )
    {
        NSString *ns_filename = [[NSString alloc] initWithUTF8String:filename];
        
        NSString* test = [[NSBundle mainBundle] pathForResource: ns_filename ofType: nil];
        
        [ns_filename release];
        
        return test.UTF8String;
    }
    
    bool os_update( )
    {
        static bool thread_started = false;
        if(!thread_started)
        {
            //audio, user thread etc
            pen::default_thread_info thread_info;
            thread_info.flags = pen::PEN_CREATE_AUDIO_THREAD;
            pen::thread_create_default_jobs( thread_info );
            thread_started = true;
        }

        return true;
    }
}
