#import <Cocoa/Cocoa.h>
#import <OpenGL/gl3.h>

#include "window.h"
#include "pen.h"
#include "threads.h"

NSOpenGLView* _gl_view;
IBOutlet NSWindow * _window;
NSOpenGLContext* _gl_context;

extern pen::window_creation_params pen_window;
extern PEN_THREAD_RETURN pen::game_entry( void* params );

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
    [_window setContentView:glView];
    
    NSOpenGLContext* glContext = [glView openGLContext];
    
    [glContext makeCurrentContext];
    GLint interval = 1;
    [glContext setValues:&interval forParameter:NSOpenGLCPSwapInterval];
    
    _gl_view    = glView;
    _gl_context = glContext;
}

int main(int argc, char **argv)
{
    //window creation
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSLog(@"NSApp=%@", NSApp);
    [NSApplication sharedApplication];
    NSLog(@"NSApp=%@", NSApp);

    NSRect frame = NSMakeRect(100, 100, pen_window.width, pen_window.height);
    
    NSUInteger style_mask = NSWindowStyleMaskTitled;
    
    NSRect rect = [NSWindow contentRectForFrameRect:frame styleMask:style_mask];
    
    _window = [[NSWindow alloc] initWithContentRect:rect styleMask:style_mask backing: NSBackingStoreBuffered defer:NO];

    [_window makeKeyAndOrderFront: _window];
    
    [_window setTitle:[NSString stringWithUTF8String:pen_window.window_title]];
    
    create_gl_context();
    
    [pool drain];
    
    //create game thread
    pen::threads_create( &pen::game_entry, 1024*1024, nullptr, pen::THREAD_START_DETACHED );
    
    //main thread loop
    while( 1 )
    {
        [_gl_context makeCurrentContext];
        
        glUseProgram(0);
        
        glClearColor( 0.8f, 0.8f, 1.0f, 1.0f );
        glClear(GL_COLOR_BUFFER_BIT);
        
        [_gl_context flushBuffer];
        
        NSEvent* peekEvent = [NSApp
                    nextEventMatchingMask:NSEventMaskAny
                    untilDate:[NSDate distantPast] // do not wait for event
                    inMode:NSDefaultRunLoopMode
                    dequeue:YES
                    ];

        [NSApp sendEvent:peekEvent];
        [NSApp updateWindows];
    
        //sleep a bit
        pen::threads_sleep_ms( 16 );
    }
}
