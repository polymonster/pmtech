#import <Cocoa/Cocoa.h>
int main(int argc, char **argv)
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
    NSLog(@"NSApp=%@", NSApp);
    [NSApplication sharedApplication];
    NSLog(@"NSApp=%@", NSApp);

    NSRect frame = NSMakeRect(100, 100, 1280, 720);
    NSUInteger styleMask = NSWindowStyleMaskTitled;
    NSRect rect = [NSWindow contentRectForFrameRect:frame styleMask:styleMask];
    NSWindow * window = [[NSWindow alloc] initWithContentRect:rect styleMask:styleMask
                                                      backing: NSBackingStoreBuffered defer:false];
    [window setBackgroundColor:[NSColor blueColor]];
    [window makeKeyAndOrderFront: window];
    [pool drain];
    [NSApp run];
}
