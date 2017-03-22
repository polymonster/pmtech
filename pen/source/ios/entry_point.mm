#include "window.h"
#include "pen.h"
#include "threads.h"
#include "renderer.h"

#import <UIKit/UIKit.h>

CAEAGLLayer* _eaglLayer;
EAGLContext* _gl_context;

extern pen::window_creation_params pen_window;
extern PEN_THREAD_RETURN pen::game_entry( void* params );

void create_context( )
{
    EAGLRenderingAPI api = kEAGLRenderingAPIOpenGLES2;
    _gl_context = [[EAGLContext alloc] initWithAPI:api];
}

void pen_make_gl_context_current( )
{
    //[_gl_context makeCurrentContext];
}

void pen_gl_swap_buffers( )
{
    //[_gl_context flushBuffer];
}

@interface gl_view_controller : UIViewController

@end

@interface gl_view_controller ()

@end

@implementation gl_view_controller

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    create_context();

}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
}

@end


@interface gl_app_delegate : UIResponder <UIApplicationDelegate>

@property (strong, nonatomic) UIWindow *window;

@end

@interface gl_app_delegate ()

@end

@implementation gl_app_delegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions
{
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.rootViewController = [[gl_view_controller alloc] init];
    self.window.backgroundColor = [UIColor whiteColor];
    [self.window makeKeyAndVisible];
    
    return YES;
    
    // Override point for customization after application launch.
    return YES;
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
    NSString* cunt = NSStringFromClass([gl_app_delegate class]);
    
    @autoreleasepool
    {
        return UIApplicationMain(argc, argv, nil, cunt );
    }
}
