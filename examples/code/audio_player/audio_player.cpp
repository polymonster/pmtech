#include "pen.h"
#include "threads.h"
#include "memory.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "debug_render.h"
#include "audio.h"
#include "dev_ui.h"

pen::window_creation_params pen_window
{
    1280,					//width
    720,					//height
    4,						//MSAA samples
    "audio_player"		    //window title / process name
};

u32 clear_state_grey;
u32 raster_state_cull_back;

pen::viewport vp =
{
    0.0f, 0.0f,
    1280.0f, 720.0f,
    0.0f, 1.0f
};

u32 default_depth_stencil_state;

void renderer_state_init( )
{
    //create 2 clear states one for the render target and one for the main screen, so we can see the difference
    static pen::clear_state cs =
    {
        0.5f, 0.5f, 0.5f, 0.5f, 1.0f, PEN_CLEAR_COLOUR_BUFFER | PEN_CLEAR_DEPTH_BUFFER,
    };

    clear_state_grey = pen::renderer_create_clear_state( cs );

    //raster state
    pen::rasteriser_state_creation_params rcp;
    pen::memory_zero( &rcp, sizeof( pen::rasteriser_state_creation_params ) );
    rcp.fill_mode = PEN_FILL_SOLID;
    rcp.cull_mode = PEN_CULL_BACK;
    rcp.depth_bias_clamp = 0.0f;
    rcp.sloped_scale_depth_bias = 0.0f;
    rcp.depth_clip_enable = true;

    raster_state_cull_back = pen::renderer_create_rasterizer_state( rcp );
    
    //depth stencil state
    pen::depth_stencil_creation_params depth_stencil_params = { 0 };
    
    // Depth test parameters
    depth_stencil_params.depth_enable = true;
    depth_stencil_params.depth_write_mask = 1;
    depth_stencil_params.depth_func = PEN_COMPARISON_ALWAYS;
    
    default_depth_stencil_state = pen::renderer_create_depth_stencil_state(depth_stencil_params);
}

void audio_player_update();

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job_thread* p_thread_info = job_params->job_thread_info;
    pen::threads_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
    renderer_state_init();

	put::dev_ui::init();
    
    while( 1 )
    {
		put::dev_ui::new_frame();
        
        pen::renderer_set_rasterizer_state( raster_state_cull_back );
        
        //bind back buffer and clear
        pen::renderer_set_depth_stencil_state(default_depth_stencil_state);
        
        pen::renderer_set_viewport( vp );
        pen::renderer_set_scissor_rect( rect{ vp.x, vp.y, vp.width, vp.height} );
        pen::renderer_set_targets( PEN_DEFAULT_RT, PEN_DEFAULT_DS );
        pen::renderer_clear( clear_state_grey );
        
        audio_player_update();

        //present
		put::dev_ui::render();
        
        pen::renderer_present();

        pen::renderer_consume_cmd_buffer();
        
        pen::audio_consume_command_buffer();
        
        //msg from the engine we want to terminate
        if( pen::threads_semaphore_try_wait( p_thread_info->p_sem_exit ) )
        {
            break;
        }
    }
    
    //clean up mem here
    
    //signal to the engine the thread has finished
    pen::threads_semaphore_signal( p_thread_info->p_sem_terminated, 1);

    return PEN_THREAD_OK;
}

enum playback_deck_flags : s32
{
    NONE = 0,
    GROUP_STATE_VALID = 1<<0,
    CHANNEL_STATE_VALID = 1<<1,
    FILE_INFO_INVALID = 1<<3,
    FILE_INFO_VALID = 1<<4,
    PAUSE_FFT_UPDATE = 1<<5,
    CUE = 1<<6,
    CUE_DOWN = 1<<7
};

f32 pitch_range_options[4] =
{
    8.0f,
    16.0f,
    50.0f,
    100.0f
};

const c8* pitch_range_desriptions[4] =
{
    "8%",
    "16%",
    "50%",
    "100%"
};

static const u32 k_num_fft_diff_buckets = 5;
u32 k_fft_diff_ranges[k_num_fft_diff_buckets] = { 16, 32, 64, 256, 1024 };
const c8* diff_range_desriptions[k_num_fft_diff_buckets] =
{
    "0-16",
    "16-32",
    "32-64",
    "64-256",
    "256-1024"
};

const c8* diff_range_nicknames[k_num_fft_diff_buckets] =
{
    "KICK",
    "LOW ",
    "MID ",
    "HAT ",
    "HIGH"
};

class beat_grid
{
public:
    std::vector<f32> beats[k_num_fft_diff_buckets];
    f32 average_interval[k_num_fft_diff_buckets];

    void show_window( bool& open )
    {
        ImGui::Begin("Beat Grid");
        
        ImGui::BeginChild("scrolling", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        
        for( s32 i = 0; i < k_num_fft_diff_buckets; ++i )
        {
            average_interval[i] = 0.0f;
            
            f32 prev = 0.0f;
            u32 counter = 0;
            for( auto& timestamp : beats[ i ] )
            {
                if( counter > 0 )
                {
                    ImGui::Text("%f", timestamp - prev ); ImGui::SameLine();
                    
                    average_interval[ i ] += ( timestamp - prev );
                }
                
                counter++;
                prev = timestamp;
            }
            
            average_interval[ i ] /= (f32)beats[ i ].size();
            
            if( beats[ i ].size() > 64 )
            {
                beats[ i ].erase( beats[ i ].begin() );
            }
            
            ImGui::Text("end");
        }
        
        ImGui::Text( "%f", average_interval[ 0 ] );
        
        ImGui::EndChild();
        
        ImGui::End();
    }
};

struct spectrum_history_stats
{
    f32 min, max, average;
};

struct beat
{
    u32 start = 0;
    u32 end = 0;
    f32 val = 0.0f;
};

class spectrum_analyser
{
public:
    pen::audio_fft_spectrum frame_spectrum;
    u32                     spectrum_dsp;
    
    static const u32        max_fft_length = 1024;
    static const u32        num_analysis_buffers = 256;
    static const u32        new_anlysis_loc = 0;
    f32                     spectrum_history[num_analysis_buffers][max_fft_length];
    spectrum_history_stats  spectrum_stats[max_fft_length];
    f32                     coarse_spectrum_history[num_analysis_buffers][k_num_fft_diff_buckets];
    
    f32                     raw_diff[num_analysis_buffers][max_fft_length];
    f32                     coarse_diff[k_num_fft_diff_buckets][num_analysis_buffers];
    f32                     second_order_diff[k_num_fft_diff_buckets][num_analysis_buffers];
    f32                     timestamp[num_analysis_buffers];
    
    f32                     beat_hueristic[1024];
    u32                     h_pos = 0;
    u32                     frame_counter = 0;
    
    s32                     h_pos_min[ 4 ] = { 0, 256, 512, 768 };
    s32                     h_pos_max[ 4 ] = { 255, 255, 255, 255 };
    
    std::vector<beat>       beats;
    beat                    new_beat;
    
    s32                     current_display_pos = 0;
    s32                     current_display_samples = 256;
    bool                    pause_display = false;
    
    
    void update( s32 cur_track_pos )
    {
        f32 cur_time = (f32)cur_track_pos;
        
        if( pause_display )
        {
            return;
        }
        
        //update fft spectrum history
        pen_error err = pen::audio_dsp_get_spectrum( spectrum_dsp, &frame_spectrum );
        
        if( err == PEN_ERR_OK )
        {
            //shift the history along.
            for( s32 i = num_analysis_buffers-1; i > 0 ; --i )
            {
                pen::memory_cpy( &spectrum_history[ i ][ 0 ], &spectrum_history[ i - 1 ][ 0 ], sizeof(f32) * max_fft_length);
                pen::memory_cpy( &raw_diff[ i ][ 0 ], &raw_diff[ i - 1 ][ 0 ], sizeof(f32) * max_fft_length);
                
                timestamp[ i ] = timestamp[ i - 1 ];
            }
            
            timestamp[ 0 ] = cur_time;
            
            //add current frame spectrum to the history buffer
            for( s32 bin = 0; bin < frame_spectrum.length; ++bin )
            {
                if( bin < max_fft_length )
                {
                    //combine stereo channels and avaerge into 1 to simply things
                    spectrum_history[new_anlysis_loc][bin] = 0.0f;
                    
                    for( s32 chan = 0; chan < frame_spectrum.num_channels; ++chan )
                    {
                        spectrum_history[new_anlysis_loc][bin] += frame_spectrum.spectrum[chan][bin];
                    }
                    
                    spectrum_history[new_anlysis_loc][bin] /= (f32)(frame_spectrum.num_channels);
                }
            }
            
            //calculate statistics over time
            for( s32 i = 0; i < max_fft_length; ++i )
            {
                spectrum_stats[i].min = 1.0f;
                spectrum_stats[i].max = 0.0f;
                spectrum_stats[i].average = 0.0f;
                
                for( s32 j = 0; j < num_analysis_buffers; ++j )
                {
                    f32 val = spectrum_history[j][i];
                    spectrum_stats[i].average += val;
                    
                    spectrum_stats[i].max = fmax( spectrum_stats[i].max, val);
                    spectrum_stats[i].min = fmin( spectrum_stats[i].min, val);
                }
                
                spectrum_stats[i].average /= (f32)num_analysis_buffers;
            }
            
            //shift along coarse diff
            for( s32 i = 0; i < k_num_fft_diff_buckets; ++i )
            {
                for( s32 j = num_analysis_buffers - 1; j > 0; --j )
                {
                    coarse_diff[ i ][ j ] = coarse_diff[ i ][ j - 1 ];
                }
            }
            
            //calculate change in frequency from frame to frame
            u32 coarse_buffer = 0;
            f32 coarse_average = 0.0f;
            for( u32 i = 0; i < max_fft_length; ++i )
            {
                raw_diff[new_anlysis_loc][ i ] = ( spectrum_history[new_anlysis_loc][i] - spectrum_history[new_anlysis_loc + 1][i] );
                
                static const f32 epsilon = 0.000000001f;
                raw_diff[new_anlysis_loc][ i ] /= (spectrum_stats[i].max + epsilon);
                
                coarse_average += raw_diff[new_anlysis_loc][i];
                
                //average spectrum into coarser bands
                if( i >= k_fft_diff_ranges[ coarse_buffer ] || i == max_fft_length-1 )
                {
                    coarse_diff[ coarse_buffer ][ new_anlysis_loc ] = coarse_average / (f32)k_fft_diff_ranges[coarse_buffer];
                    
                    coarse_buffer++;
                    coarse_average = 0.0f;
                }
            }
            
            //isolate peaks
            for( s32 b = 0; b < k_num_fft_diff_buckets; ++b )
            {
                f32 cur_max = -1.0f;
                f32 cur_min = 1.0f;
                f32 cur_dir = -1.0f;
                
                for( s32 i = num_analysis_buffers - 1; i > 0; --i )
                {
                    second_order_diff[ b ][ i ] = 0.0f;
                    second_order_diff[ b ][ i - 1 ] = 0.0f;
                    
                    if( coarse_diff[ b ][ i - 1 ] >= coarse_diff[ b ][ i ] )
                    {
                        cur_max = coarse_diff[ b ][ i ];
                        
                        if( cur_dir != 1.0f )
                        {
                            second_order_diff[ b ][ i ] = (cur_max - cur_min) * 0.5f + 0.5f;
                        }
                        
                        //upward
                        cur_dir = 1.0f;
                    }
                    else
                    {
                        cur_min = coarse_diff[ b ][ i ];
                        
                        if( cur_dir != -1.0f )
                        {
                            second_order_diff[ b ][ i ] = (cur_min - cur_max) * 0.5f + 0.5f;
                        }
                        
                        //downward
                        cur_dir = -1.0f;
                    }
                    
                    second_order_diff[ b ][ i ] -= 0.5f;
                    second_order_diff[ b ][ i ] *= 2.0f;
                    second_order_diff[ b ][ i ] = fmax( second_order_diff[ b ][ i ], 0.0f );
                }
            }
            
            //quantize peaks into beats
            f32 cur_score = 0.0f;
            s32 beat_start = -1;
            s32 cool_down = 0;
            
            for( s32 i = num_analysis_buffers - 1; i > 0 ; i-- )
            {
                f32 cur_val = second_order_diff[ 0 ][ i ];
                
                if( cur_val > 0.01f && beat_start == -1 )
                {
                    cur_score = cur_val;
                    beat_hueristic[ h_pos + i ] = cur_val;
                    beat_start = i;
                    cool_down = 4;
                }
                else
                {
                    if( cool_down > 0 )
                    {
                        beat_hueristic[ h_pos + i ] = cur_score;
                        
                        if( cur_val > cur_score )
                        {
                            cur_score = cur_val;
                            
                            u32 j = i;
                            while( (s32)j <= beat_start )
                            {
                                beat_hueristic[ h_pos + j ] = cur_score;
                                
                                j++;
                            }
                        }
                    }
                    else
                    {
                        beat_hueristic[ h_pos + i ] = 0.0f;
                        beat_start = -1;
                        cur_score = 0.0f;
                    }
                }
                
                cool_down--;
            }
            
            if( second_order_diff[ 0 ][ 4 ] > new_beat.val )
            {
                if( new_beat.val == 0.0f )
                {
                    new_beat.start = frame_counter;
                }
                
                new_beat.val = second_order_diff[ 0 ][ 4 ];
            }
            else if( second_order_diff[ 0 ][ 0 ] < 0.01f )
            {
                if( new_beat.val > 0.0f )
                {
                    new_beat.end = frame_counter;
                    beats.insert(beats.begin() + 0, new_beat);
                }
                
                new_beat.val = 0.0f;
                new_beat.start = 0;
                new_beat.end = 0;
            }
            
            ++frame_counter;
        }
    }
    
    void show_window( )
    {
        ImGui::Begin("Spectrum Analyser");
        
        ImGui::InputInt("Num Display Samples", &current_display_samples);
        
        u32 display_analysis_loc = new_anlysis_loc;
        pause_display |= ImGui::InputInt("Analysis Display Pos", &current_display_pos);
        ImGui::SameLine();
        if( pause_display )
        {
            if( ImGui::Button("Play") )
            {
                pause_display = false;
            }
            
            display_analysis_loc = current_display_pos;
        }
        else
        {
            if( ImGui::Button("Play") )
            {
                pause_display = true;
            }
        }
        
        if( ImGui::CollapsingHeader("Raw Data") )
        {
            ImGui::PlotHistogram("Raw Spectrum", &spectrum_history[ display_analysis_loc ][0], current_display_samples, 0, NULL, 0.0f, 1.0f, ImVec2(530,100));
            ImGui::PlotLines("Per Sample Differentiation", &raw_diff[ display_analysis_loc ][0], current_display_samples, 0, NULL, -1.0f, 1.0f, ImVec2(530,100));
            
            for( s32 i = 0; i < k_num_fft_diff_buckets; ++i )
            {
                if( i < k_num_fft_diff_buckets - 1 )
                {
                    ImGui::PlotLines("", &coarse_diff[i][0], num_analysis_buffers, 0, NULL, -1.0f, 1.0f, ImVec2(100,100));
                    ImGui::SameLine();
                }
                else
                {
                    ImGui::PlotLines("Coarse Differentiation", &coarse_diff[i][0], num_analysis_buffers, 0, NULL, -1.0f, 1.0f, ImVec2(100,100));
                }
            }
        }
        
        if( ImGui::CollapsingHeader("Differentiation Graphs") )
        {
            for( s32 i = 0; i < k_num_fft_diff_buckets; ++i )
            {
                ImGui::PlotLines(diff_range_nicknames[i], &coarse_diff[i][0], num_analysis_buffers, 0, NULL, -1.0f, 1.0f, ImVec2(530,100));
            }
        }

        if( ImGui::CollapsingHeader("Second Order Differentiation Graphs") )
        {
            for( s32 i = 0; i < k_num_fft_diff_buckets; ++i )
            {
                ImGui::PlotLines(diff_range_nicknames[i], &second_order_diff[i][0], num_analysis_buffers, 0, NULL, 0.0f, 1.0f, ImVec2(530,100));
            }
        }
        
        if( ImGui::CollapsingHeader("Peak Isolation") )
        {
            ImGui::PlotHistogram("", &beat_hueristic[ 0 ], num_analysis_buffers, 0, NULL, 0.0f, 1.0f, ImVec2(530,100));
        }
        
        if( ImGui::CollapsingHeader("Beat History") )
        {
            std::vector<f32> bv;
            
            u32 num_beats = beats.size() > 256 ? 256 : beats.size();
            
            u32 base_frame = 0;
            
            for( u32 i = 0; i < num_beats; ++i )
            {
                if( i == 0 )
                {
                    base_frame = beats[ i ].start;
                }
                else if( i > 0 )
                {
                    s32 beat_interval = beats[ i ].start - beats[ i - 1 ].end;
                    
                    for( s32 b = 0; b < beat_interval; ++b )
                    {
                        bv.push_back( 0.0f );
                    }
                }
                
                u32 beat_len = beats[ i ].end - beats[ i ].start;
                
                for( u32 b = 0; b < beat_len; ++b )
                {
                    bv.push_back( beats[ i ].val );
                }
            }
            
            ImGui::PlotHistogram("", (f32*)&bv[ 0 ], bv.size(), 0, NULL, 0.0f, 1.0f, ImVec2(530,100));
        }
        
        if( ImGui::CollapsingHeader("Intervals") )
        {
            ImGui::BeginChild("scrolling", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
            
            for( s32 i = 0; i < k_num_fft_diff_buckets; ++i )
            {
                u32 interval = 0;
                u32 last_beat = 0;
                bool first_beat = true;
                s32 cooldown = 0;
                
                for( s32 j = num_analysis_buffers - 1; j > 0; --j )
                {
                    if( beat_hueristic[ j ] > 0.1f && cooldown <= 0)
                    {
                        interval = (last_beat - j);
                        
                        if(!first_beat && interval > 3)
                        {
                            ImGui::Text("[%02d]", interval ); ImGui::SameLine();
                            ImGui::Text("[%.1f]", timestamp[ j ] - timestamp[ last_beat ] );
                        }
                        
                        last_beat = j;
                        
                        first_beat = false;
                        
                        cooldown = 4;
                    }
                    
                    --cooldown;
                }
                
                ImGui::Text(".");
            }
            
            ImGui::End();
        }
        
        ImGui::End();
    }
};

class playback_deck
{
public:
    s32 flags = NONE;
    
    //audio handles
    u32 group_index;
    u32 sound_index;
    u32 channel_index;
    u32 spectrum_dsp;
    
    spectrum_analyser sa;
    
    //audio states
    pen::audio_channel_state channel_state;
    pen::audio_fft_spectrum spectrum;
    pen::audio_group_state group_state;
    pen::audio_sound_file_info file_info;
    
    //filesystem
    bool open_file = false;
    bool open_beat_grid = false;
    bool open_spectrum_analyser = false;
    const c8* file = nullptr;
    
    u32 cue_pos;
    s32 pitch_range;
    
    //debug info
    s32 fft_num_samples = 64;
    f32 fft_max = 1.0f;
    
    //analysis
    
    //general fft storage
    static const s32 num_analysis_buffers = 256;
    f32 fft_buffers[num_analysis_buffers][2048] = { 0 };
    u32 fft_timestamp[num_analysis_buffers] = { 0 };
    
    //comparison
    f32 fft_max_vals[2048];
    f32 fft_diff[2048];
    
    //the magic
    f32 fft_combined_diff[k_num_fft_diff_buckets] = { 0.0f };
    f32 prev_combined_diff[k_num_fft_diff_buckets] = { 0.0f };
    f32 fft_combined_history[num_analysis_buffers] = { 0.0f };
    f32 beat_cooldown[ k_num_fft_diff_buckets ] = { 0.0f };
    
    //tracking
    s32 current_analysis_buffer_pos = 0;
    s32 plot_lines_diff_range = 0;
    
    f32 timestamp = 0.0;
    f32 fame_time = 0.0f;
    
    void ui_control()
    {
        f32 prev_time = timestamp;
        timestamp = pen::timer_get_time();
        
        fame_time = timestamp - prev_time;
        
        //file info
        ImGui::PushID(this);
        if( ImGui::Button("Open"))
        {
            open_file = true;
        }
        
        if( open_file )
        {
            const c8* file = put::dev_ui::file_browser( open_file, put::dev_ui::FB_OPEN, 2, "**.mp3", "**.wav" );
            
            if( file != nullptr )
            {
                load_new_file(file);
                open_file = false;
                flags |= FILE_INFO_INVALID;
            }
        }
        
        ImGui::SameLine();
        ImGui::Text("%s", file);
        
        //update states
        pen_error err = pen::audio_channel_get_state(channel_index, &channel_state);
        
        if( err == PEN_ERR_OK )
        {
            flags |= CHANNEL_STATE_VALID;
        }
        else
        {
            flags &= ~(CHANNEL_STATE_VALID);
        }
        
        err = pen::audio_group_get_state(group_index, &group_state);
        
        if( err == PEN_ERR_OK )
        {
            flags |= GROUP_STATE_VALID;
        }
        else
        {
            flags &= ~(GROUP_STATE_VALID);
        }
        
        //obtain file info
        if( flags & FILE_INFO_INVALID )
        {
            //get file info
            err = pen::audio_channel_get_sound_file_info(sound_index, &file_info);
            if( err == PEN_ERR_OK )
            {
                flags &= ~(FILE_INFO_INVALID);
                flags |= FILE_INFO_VALID;
            }
        }
        
        //transport controls
        if( channel_state.play_state == pen::NOT_PLAYING  || channel_index == 0 )
        {
            if( ImGui::Button("Play") )
            {
                channel_index = pen::audio_create_channel_for_sound( sound_index );
                pen::audio_add_channel_to_group(channel_index, group_index);
                flags &= ~(PAUSE_FFT_UPDATE|CUE);
            }
        }
        else if( group_state.play_state == pen::PLAYING )
        {
            if( ImGui::Button("Pause") )
            {
                pen::audio_group_set_pause( group_index, true );
                flags |= PAUSE_FFT_UPDATE;
            }
        }
        else if( group_state.play_state == pen::PAUSED )
        {
            if( ImGui::Button("Play") )
            {
                pen::audio_group_set_pause( group_index, false );
                flags &= ~(PAUSE_FFT_UPDATE|CUE);
            }
        }
        
        ImGui::SameLine();
        if( ImGui::Button("Cue") )
        {
            if( !(flags & CUE) )
            {
                cue_pos = channel_state.position_ms;
                
                flags &= ~(PAUSE_FFT_UPDATE);
                pen::audio_group_set_pause( group_index, false );
                
                flags |= CUE;
            }
            else
            {
                flags |= PAUSE_FFT_UPDATE;
                channel_state.position_ms = cue_pos;
                pen::audio_channel_set_position(channel_index, channel_state.position_ms);
                pen::audio_group_set_pause( group_index, true );
            }
        }
        
        ImGui::SameLine();
        if( ImGui::Button("Stop") )
        {
            if( sound_index != 0 )
            {
                pen::audio_channel_stop(channel_index);
                channel_index = 0;
                
                pen::audio_release_resource(sound_index);
                sound_index = 0;
            }
        }
        
        sa.spectrum_dsp = spectrum_dsp;
        sa.update( channel_state.position_ms );
        
        if( ImGui::Button("Spectrum Analyser") )
        {
            open_spectrum_analyser = true;
        }
        
        if( open_spectrum_analyser )
        {
            sa.show_window();
        }
        
        //file info valid
        if( ImGui::SliderInt("Track Pos", (s32*)&channel_state.position_ms, 0, file_info.length_ms) )
        {
            pen::audio_channel_set_position(channel_index, channel_state.position_ms);
        }
        
        switch( group_state.play_state )
        {
            case pen::PLAYING:
                ImGui::Text("%s", "Playing");
                break;
                
            case pen::NOT_PLAYING:
                ImGui::Text("%s", "Not Playing");
                break;
                
            case pen::PAUSED:
                ImGui::Text("%s", "Paused");
                break;
        }
        
        switch( channel_state.play_state )
        {
            case pen::PLAYING:
                ImGui::Text("%s", "Channel Playing");
                break;
                
            case pen::NOT_PLAYING:
                ImGui::Text("%s", "Channel Not Playing");
                break;
                
            case pen::PAUSED:
                ImGui::Text("%s", "Channel Paused");
                break;
        }
        
        ImGui::SameLine();
        ImGui::Text("%i", channel_state.position_ms );
        
        
        ImGui::Combo("Pitch Range", &pitch_range, pitch_range_desriptions, 4);
        
        f32 pitch_range_f = (1.0f / 100.0f ) * pitch_range_options[ pitch_range ];
        if( ImGui::VSliderFloat("Pitch", ImVec2( 20.0f, 100.0f ), &group_state.pitch, 1.0f - pitch_range_f, 1.0f + pitch_range_f, "" ) )
        {
            pen::audio_group_set_pitch(group_index, group_state.pitch );
        }
        
        ImGui::PopID();
    }
    
    void load_new_file( const c8* _file )
    {
        file = _file;
        
        sound_index = pen::audio_create_sound(file);
    }
};

class mixer_channel
{
public:
    u32 group_index;
    u32 three_band_eq_dsp;
    u32 gain_dsp;
    
    pen::audio_group_state group_state;
    pen::audio_eq_state eq_state;
    
    f32 gain;
    
    void control_ui()
    {
        pen::audio_group_get_state(group_index, &group_state);
        pen::audio_dsp_get_three_band_eq(three_band_eq_dsp, &eq_state);
        pen::audio_dsp_get_gain( gain_dsp, &gain );
        
        ImGui::PushID(this);
        if( ImGui::SliderFloat("Gain", &gain, -10.0f, 10.0f ) )
        {
            pen::audio_dsp_set_gain( gain_dsp, gain );
        }
        
        if( ImGui::SliderFloat("Hi", &eq_state.high, -100.0f, 100.0f ) )
        {
            pen::audio_group_set_volume(group_index, group_state.volume );
        }
        
        if( ImGui::SliderFloat("Med", &eq_state.med, -100.0f, 100.0f ) )
        {
            pen::audio_group_set_volume(group_index, group_state.volume );
        }
        
        if( ImGui::SliderFloat("Low", &eq_state.low, -100.0f, 100.0f ) )
        {
            pen::audio_group_set_volume(group_index, group_state.volume );
        }
        
        if( ImGui::VSliderFloat("Vol", ImVec2( 20.0f, 100.0f ), &group_state.volume, 0.0f, 1.0f, "" ) )
        {
            pen::audio_group_set_volume(group_index, group_state.volume );
        }
        ImGui::PopID();
        
        pen::audio_dsp_set_three_band_eq( three_band_eq_dsp, eq_state.low, eq_state.med, eq_state.high );
    }
};


void audio_player_update()
{
    const s32 num_decks = 2;
    
    static bool initialised = false;
    
    static playback_deck decks[num_decks];
    static mixer_channel mixer_channels[num_decks];
    
    if( !initialised )
    {
        for( s32 i = 0; i < num_decks; ++i )
        {
            decks[i].group_index = pen::audio_create_channel_group();
            mixer_channels[i].group_index = decks[i].group_index;
            
            //create sound spectrum dsp
            decks[i].spectrum_dsp = pen::audio_add_dsp_to_group( decks[i].group_index, pen::DSP_FFT );
            mixer_channels[i].three_band_eq_dsp = pen::audio_add_dsp_to_group( decks[i].group_index, pen::DSP_THREE_BAND_EQ );
            mixer_channels[i].gain_dsp = pen::audio_add_dsp_to_group( decks[i].group_index, pen::DSP_GAIN );
        }
        
        initialised = true;
    }
    
    bool open = true;
    ImGui::Begin("Player", &open );
    
    ImGui::Columns(num_decks+1, "decks");
    ImGui::Separator();
    
    for( s32 i = 0; i < num_decks; ++i )
    {
        decks[i].ui_control();
        
        ImGui::NextColumn();
        
        if( i == 0 )
        {
            ImGui::BeginChild("Mixer");
            
            ImGui::Columns(num_decks, "channels");
            
            for( s32 j = 0; j < num_decks; ++j )
            {
                mixer_channels[ j ].control_ui();
                ImGui::NextColumn();
            }
            
            ImGui::Columns(num_decks+1, "decks");
            
            ImGui::EndChild();
            
            ImGui::NextColumn();
        }
    }
    
    ImGui::Columns(1);
    
    ImGui::End();

}
