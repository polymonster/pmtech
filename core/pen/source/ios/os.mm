// os.mm
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "os.h"
#include "console.h"
#include "data_struct.h"
#include "hash.h"
#include "input.h"
#include "pen.h"
#include "renderer.h"
#include "renderer_shared.h"
#include "threads.h"
#include "timer.h"

#ifdef PEN_RENDERER_METAL
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#else
#define GLES_SILENCE_DEPRECATION
#import <GLKit/GLKit.h>
#endif

#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

// for music api
#import <AVFoundation/AVAudioBuffer.h>
#import <AVFoundation/AVAudioFile.h>
#import <AVFoundation/AVAudioSession.h>
#import <MediaPlayer/MPMediaQuery.h>
#import <MediaPlayer/MPNowPlayingInfoCenter.h>
#import <MediaPlayer/MPRemoteCommandCenter.h>
#import <MediaPlayer/MPRemoteCommand.h>

// the last 2 global externs \o/
pen::user_info              pen_user_info;
pen::window_creation_params pen_window;

// objc interfaces
@interface pen_mtk_renderer : NSObject <MTKViewDelegate>
- (instancetype)initWithView:(nonnull MTKView*)view;
@end

@interface pen_view_controller : UIViewController
- (void)viewWasDoubleTapped:(id)sender;
- (BOOL)prefersHomeIndicatorAutoHidden;
@end

@interface pen_text_field_delegate : NSObject<UITextFieldDelegate>
- (BOOL)textField:(UITextField*)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString*)string;
@end

@interface pen_text_field : UITextField
- (void)deleteBackward;
@end

@interface                                        pen_app_delegate : UIResponder <UIApplicationDelegate>
@property(strong, nonatomic) UIWindow*            window;
@property(strong, nonatomic) MTKView*             mtk_view;
@property(strong, nonatomic) pen_mtk_renderer*    mtk_renderer;
@property(strong, nonatomic) pen_view_controller* view_controller;

// remote commands
-(MPRemoteCommandHandlerStatus)play;
-(MPRemoteCommandHandlerStatus)pause;
-(MPRemoteCommandHandlerStatus)prev;
-(MPRemoteCommandHandlerStatus)next;
@end

namespace
{
    struct os_context
    {
        CGRect                   wframe = {};
        CGSize                   wsize = {};
        f32                      wscale = 1.0f;
        pen_app_delegate*        app_delegate = nullptr;
        pen::pen_creation_params creation_params = {};
        pen_text_field_delegate* text_field_delegate = nullptr;
        pen_text_field*          text_field = nullptr;
        bool                     show_on_screen_keyboard = false;
    };
    os_context s_context;

    void update_pen_window()
    {
        // updates pen window size
        pen::_renderer_resize_backbuffer(s_context.wsize.width, s_context.wsize.height);
    }

    void update_on_screen_keyboard() 
    {
        static bool s_open = false;
        
        if(s_context.show_on_screen_keyboard && !s_open)
        {
            [s_context.text_field becomeFirstResponder];
            s_open = true;
        }
        else if(!s_context.show_on_screen_keyboard && s_open)
        {
            [s_context.text_field resignFirstResponder];
            s_open = false;
        }
    }
}

@implementation pen_app_delegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)launchOptions
{
    // setup base systems
    pen::timer_system_intialise();

    @autoreleasepool
    {
        s_context.app_delegate = self;
        s_context.wframe = [[UIScreen mainScreen] bounds];      // size in "points"
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
        
        // enable support for osk input
        pen::os_init_on_screen_keyboard();

        return YES;
    }
}

-(MPRemoteCommandHandlerStatus)play {
    PEN_LOG("Press Play!");
    return MPRemoteCommandHandlerStatusSuccess;
}
-(MPRemoteCommandHandlerStatus)pause {
    PEN_LOG("Press Pause!");
    return MPRemoteCommandHandlerStatusSuccess;
}
-(MPRemoteCommandHandlerStatus)prev {
    PEN_LOG("Press Prev!");
    return MPRemoteCommandHandlerStatusSuccess;
}
-(MPRemoteCommandHandlerStatus)next {
    PEN_LOG("Press Next!");
    return MPRemoteCommandHandlerStatusSuccess;
}
@end

@implementation pen_mtk_renderer
- (instancetype)initWithView:(nonnull MTKView*)view
{
    [super init];
    pen::renderer_init((void*)view, false, s_context.creation_params.max_renderer_commands);
    return self;
}
- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size
{
    s_context.wsize = size;
    update_pen_window();
}
- (void)drawInMTKView:(nonnull MTKView*)view
{
    @autoreleasepool
    {
        pen::os_update();
        pen::renderer_dispatch();
    }
}
@end

@implementation pen_view_controller
- (void)viewWasTapped:(id)sender
{
}

- (void)viewWasDoubleTapped:(id)sender
{
}

- (BOOL)prefersHomeIndicatorAutoHidden
{
    return YES;
}

- (void)didReceiveMemoryWarning
{
    PEN_LOG("[warning] ios received memory warning.");
}

- (int)getTouchId:(UITouch*)touch
{
    return 0;
}

- (void)handleTouch:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    for (UITouch* t in touches)
    {
        CGPoint touch_point = [t locationInView:t.view];

        pen::input_set_mouse_pos(touch_point.x * s_context.wscale, touch_point.y * s_context.wscale);

        switch (t.phase)
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
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    [self handleTouch:touches withEvent:event];
}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    [self handleTouch:touches withEvent:event];
}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    [self handleTouch:touches withEvent:event];
}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    [self handleTouch:touches withEvent:event];
}
@end

@implementation pen_text_field_delegate
- (BOOL)textField:(UITextField*)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString*)string
{
    // Special case for enter in order to pass to ImGui so it can handle it how it wants.
    if([string isEqualToString: @"\n"]) {
        s_context.show_on_screen_keyboard = false;
        return YES;
    }
    
    pen::input_add_unicode_input([string UTF8String]);
    return YES;
}
- (void)textFieldDidDelete
{
    pen::input_set_key_down(PK_BACK);
}
@end

@implementation pen_text_field
- (void)deleteBackward
{
    [super deleteBackward];
    if([self.delegate respondsToSelector:@selector(textFieldDidDelete)])
    {
        [(pen_text_field_delegate*)self.delegate textFieldDidDelete];
    }
}
@end

int main(int argc, char* argv[])
{
    NSString* str = NSStringFromClass([pen_app_delegate class]);

    pen::pen_creation_params pc = pen::pen_entry(argc, argv);
    pen_window.width = pc.window_width;
    pen_window.height = pc.window_height;
    pen_window.window_title = pc.window_title;
    pen_window.sample_count = pc.window_sample_count;
    s_context.creation_params = pc;

    @autoreleasepool
    {
        return UIApplicationMain(argc, argv, nil, str);
    }
}

namespace pen
{
    void* window_get_primary_display_handle()
    {
        return nullptr;
    }

    const Str os_path_for_resource(const c8* filename)
    {
        NSString* ns_filename = [[NSString alloc] initWithUTF8String:filename];
        NSString* test = [[NSBundle mainBundle] pathForResource:ns_filename ofType:nil];
        [ns_filename release];
        Str res = test.UTF8String;
        return res;
    }

    bool os_update()
    {
        static bool thread_started = false;
        if (!thread_started)
        {
            // creates user thread
            auto& pcp = s_context.creation_params;
            jobs_create_job(pcp.user_thread_function, 1024 * 1024, pcp.user_data, pen::e_thread_start_flags::detached);

            thread_started = true;
        }
        
        update_on_screen_keyboard();

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
        f = {(u32)s_context.wframe.origin.x, (u32)s_context.wframe.origin.y, (u32)s_context.wframe.size.width,
             (u32)s_context.wframe.size.height};
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

    const music_item* music_get_items()
    {
        music_item* out_items = nullptr;

        MPMediaQuery* mp = [[MPMediaQuery alloc] init];
        NSArray*      queryResults = [mp items];
        for (s32 i = 0; i < queryResults.count; ++i)
        {
            MPMediaItem* item = [queryResults objectAtIndex:0];

            music_item mi;
            mi.internal = item;
            mi.album = [item.albumTitle UTF8String];
            mi.artist = [item.albumArtist UTF8String];
            mi.track = [item.title UTF8String];
            mi.duration = [item playbackDuration];

            sb_push(out_items, mi);
        }

        return out_items;
    }

    music_file music_open_file(const music_item& item)
    {
        MPMediaItem* _item = (MPMediaItem*)item.internal;
        NSURL*       url = [_item valueForProperty:MPMediaItemPropertyAssetURL];
        AVAudioFile* file = [AVAudioFile alloc];
        [file initForReading:url error:nil];
        AVAudioFormat* fmt = [file processingFormat];

        music_file out_file;
        out_file.num_channels = [fmt channelCount];
        out_file.len = [file length] * sizeof(f32) * out_file.num_channels;
        out_file.sample_frequency = file.processingFormat.sampleRate;
        out_file.pcm_data = (f32*)pen::memory_alloc(out_file.len);

        AVAudioPCMBuffer* pcm = [[AVAudioPCMBuffer alloc] initWithPCMFormat:[file processingFormat]
                                                              frameCapacity:(u32)out_file.len];
        [file readIntoBuffer:pcm error:nil];

        if (out_file.num_channels > 1)
        {
            // interleave stereo pcm channels
            for (size_t i = 0; i < [file length]; ++i)
            {
                size_t ii = i * 2;
                out_file.pcm_data[ii + 0] = pcm.floatChannelData[0][i];
                out_file.pcm_data[ii + 1] = pcm.floatChannelData[1][i];
            }
        }
        else
        {
            // copy single mono channel
            memcpy(out_file.pcm_data, &pcm.floatChannelData[0][0], out_file.len);
        }

        return out_file;
    }

    void music_close_file(const music_file& item)
    {
        pen::memory_free(item.pcm_data);
    }

    Str os_get_persistent_data_directory()
    {
        @autoreleasepool {
            NSString* dir = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) objectAtIndex:0];
            return dir.UTF8String;
        }
    }

    Str os_get_cache_data_directory()
    {
        @autoreleasepool {
            NSString* dir = [NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) objectAtIndex:0];
            return dir.UTF8String;
        }
    }

    void os_create_directory(const Str& dir)
    {
        @autoreleasepool {
            NSFileManager* manager = [NSFileManager defaultManager];
            NSString* path = [NSString stringWithUTF8String:dir.c_str()];
            NSError* err = nil;
            [manager createDirectoryAtPath:path withIntermediateDirectories:YES attributes:nil error:&err];
        }
    }

    bool os_delete_directory(const Str& filename)
    {
        bool ret = false;
        @autoreleasepool {
            NSFileManager* manager = [NSFileManager defaultManager];
            ret = [manager removeItemAtPath:[NSString stringWithUTF8String:filename.c_str()] error:nil];
        }
        return ret;
    }

    void os_open_url(const Str& url)
    {
        @autoreleasepool {
            NSString* path = [NSString stringWithUTF8String:url.c_str()];
            [[UIApplication sharedApplication] openURL:[NSURL URLWithString:path]];
        }
    }

    void os_ignore_slient()
    {
        @autoreleasepool {
            [[AVAudioSession sharedInstance] setCategory:AVAudioSessionCategoryPlayback error:nil];
            [[AVAudioSession sharedInstance] setActive:YES error:nil];
        }
    }

    void os_enable_background_audio()
    {
        AVAudioSession *session = [AVAudioSession sharedInstance];
        double rate = 24000.0; // This should match System::setSoftwareFormat 'samplerate' which defaults to 24000
        s32 blockSize = 512; // This should match System::setDSPBufferSize 'bufferlength' which defaults to 512

        BOOL success = [session setPreferredSampleRate:rate error:nil];
        PEN_ASSERT(success);

        success = [session setPreferredIOBufferDuration:blockSize / rate error:nil];
        PEN_ASSERT(success);

        success = [session setActive:TRUE error:nil];
        PEN_ASSERT(success);
    }

    f32 os_get_status_bar_portrait_height()
    {
        CGFloat h = [[[UIApplication sharedApplication] windows].firstObject windowScene].statusBarManager.statusBarFrame.size.height;
        CGFloat s = [[UIScreen mainScreen] scale];
        return (f32)h*s;
    }

    void os_haptic_selection_feedback()
    {
        @autoreleasepool {
            auto generator = [[UISelectionFeedbackGenerator alloc] init];
            [generator prepare];
            [generator selectionChanged];
            [generator release];
        }
    }

    void os_init_on_screen_keyboard()
    {
        s_context.text_field_delegate = [[pen_text_field_delegate alloc] init];
        s_context.text_field = [[pen_text_field alloc] init];
        s_context.text_field.hidden = YES;
        s_context.text_field.autocorrectionType = UITextAutocorrectionTypeNo;
        s_context.text_field.autocapitalizationType = UITextAutocapitalizationTypeNone;
        s_context.text_field.spellCheckingType = UITextSpellCheckingTypeNo;
        s_context.text_field.delegate = s_context.text_field_delegate;
        [s_context.app_delegate.window addSubview:s_context.text_field];
    }

    void os_show_on_screen_keyboard(bool show)
    {
        s_context.show_on_screen_keyboard = show;
    }

    void music_enable_remote_control()
    {
        [[UIApplication sharedApplication] beginReceivingRemoteControlEvents];
        
        MPRemoteCommandCenter *commandCenter = [MPRemoteCommandCenter sharedCommandCenter];
        
        [commandCenter.pauseCommand addTarget:s_context.app_delegate action:@selector(pause)];
        //[commandCenter.pauseCommand addTarget:self action:@selector(pauseAudio)];
    }

    void music_set_now_playing(const Str& artist, const Str& album, const Str& track)
    {
        @autoreleasepool {
            NSString* nsartist = [NSString stringWithUTF8String:artist.c_str()];
            NSString* nsalbum = [NSString stringWithUTF8String:album.c_str()];
            NSString* nstrack = [NSString stringWithUTF8String:track.c_str()];
            
            NSMutableDictionary* info = [[NSMutableDictionary alloc] init];
            [info setObject:nstrack forKey:MPMediaItemPropertyTitle];
            [info setObject:nsalbum forKey:MPMediaItemPropertyAlbumTitle];
            [info setObject:nsartist forKey:MPMediaItemPropertyArtist];
            
            [[MPNowPlayingInfoCenter defaultCenter] setNowPlayingInfo:info];
            [info release];
        }
    }

    void music_set_now_playing_artwork(void* data, u32 w, u32 h, u32 bpp, u32 row_pitch)
    {
        @autoreleasepool {
            CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
            auto bmp = CGBitmapContextCreate(
                data, (size_t)w, (size_t)h, (size_t)bpp, (size_t)row_pitch, space, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrderDefault);
            
            CFRelease(space);
            
            CGImageRef cgimg = CGBitmapContextCreateImage(bmp);
            CGContextRelease(bmp);
            
            UIImage* uiimg = [UIImage imageWithCGImage:cgimg];
            CGImageRelease(cgimg);
            
            MPMediaItemArtwork* artwork = [[MPMediaItemArtwork alloc] initWithImage: uiimg];
            
            NSMutableDictionary* info = [NSMutableDictionary dictionaryWithDictionary:
                [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo];
            
            [info setObject:artwork forKey:MPMediaItemPropertyArtwork];
            
            [[MPNowPlayingInfoCenter defaultCenter] setNowPlayingInfo:info];
        }
    }

    void music_set_now_playing_time_info(u32 position_ms, u32 duration_ms)
    {
        @autoreleasepool {
            NSMutableDictionary* info = [NSMutableDictionary dictionaryWithDictionary:
                                         [MPNowPlayingInfoCenter defaultCenter].nowPlayingInfo];
            
            auto duration_s = (double)duration_ms / 1000.0;
            NSString* nsdur = [NSString stringWithUTF8String:std::to_string(duration_s).c_str()];
            [info setObject:nsdur forKey:MPMediaItemPropertyPlaybackDuration];
            
            auto pos_s = (double)position_ms / 1000.0;
            NSString* npos = [NSString stringWithUTF8String:std::to_string(pos_s).c_str()];
            [info setObject:npos forKey:MPNowPlayingInfoPropertyElapsedPlaybackTime];
            
            [[MPNowPlayingInfoCenter defaultCenter] setNowPlayingInfo:info];
        }
    }
}
