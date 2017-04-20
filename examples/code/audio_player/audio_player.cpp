#include "pen.h"
#include "renderer.h"
#include "timer.h"
#include "file_system.h"
#include "pen_string.h"
#include "loader.h"
#include "debug_render.h"
#include "audio.h"
#include "dev_ui.h"
#include <string>

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

    raster_state_cull_back = pen::defer::renderer_create_rasterizer_state( rcp );
    
    //depth stencil state
    pen::depth_stencil_creation_params depth_stencil_params = { 0 };
    
    // Depth test parameters
    depth_stencil_params.depth_enable = true;
    depth_stencil_params.depth_write_mask = 1;
    depth_stencil_params.depth_func = PEN_COMPARISON_ALWAYS;
    
    default_depth_stencil_state = pen::defer::renderer_create_depth_stencil_state(depth_stencil_params);
}

void audio_player_update();

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job_thread* p_thread_info = job_params->job_thread_info;
    pen::threads_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
    renderer_state_init();

    dev_ui::init();
    
    while( 1 )
    {
        dev_ui::new_frame();
        
        pen::defer::renderer_set_rasterizer_state( raster_state_cull_back );
        
        //bind back buffer and clear
        pen::defer::renderer_set_depth_stencil_state(default_depth_stencil_state);
        
        pen::defer::renderer_set_viewport( vp );
        pen::defer::renderer_set_scissor_rect( rect{ vp.x, vp.y, vp.width, vp.height} );
        pen::defer::renderer_set_targets( PEN_DEFAULT_RT, PEN_DEFAULT_DS );
        pen::defer::renderer_clear( clear_state_grey );
        
        audio_player_update();

        //present
        ImGui::Render();
        
        pen::defer::renderer_present();

        pen::defer::renderer_consume_cmd_buffer();
        
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

const c8* file_browser( bool& dialog_open )
{
    const c8* default_dir[] =
    {
        "/",
        "Users",
        "alex.dixon",
        "Desktop",
        "MP3"
    };
    s32 default_depth = 5;
    
    static bool initialise = true;
    static s32 current_depth = 1;
    static s32 selection_stack[128] = { -1 };
    
    std::string current_path;
    std::string search_path;
    static std::string selected_path;
    
    static pen::fs_tree_node fs_enumeration;
    
    if( initialise )
    {
        pen::filesystem_enum_volumes(fs_enumeration);
        
        pen::fs_tree_node* fs_iter = &fs_enumeration;
        
        for( s32 c = 0; c < default_depth; ++c )
        {
            for( s32 entry = 0; entry < fs_iter->num_children; ++entry )
            {
                if( pen::string_compare( fs_iter->children[entry].name, default_dir[c] ) == 0 )
                {
                    current_path += fs_iter->children[ entry ].name;
                    current_path += "/";
                    
                    pen::filesystem_enum_directory( current_path.c_str(), fs_iter->children[ entry ] );
                    
                    selection_stack[c] = entry;
                    
                    fs_iter = &fs_iter->children[ entry ];
                    
                    current_depth = c + 2;
                    
                    break;
                }
            }
        }
        
        initialise = false;
    }
    
    ImGui::Begin("File Browser");
    
    ImGui::BeginChild("scrolling", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
    
    ImGui::Text("%s", selected_path.c_str());
    
    const c8* return_value = nullptr;
    
    ImGuiButtonFlags button_flags = 0;
    if( selected_path == "" )
    {
        button_flags |= ImGuiButtonFlags_Disabled;
    }
    
    if( ImGui::ButtonEx("OK", ImVec2(0,0), button_flags ) )
    {
        return_value = selected_path.c_str();
        dialog_open = false;
    }
    
    ImGui::SameLine();
    if( ImGui::Button("Cancel") )
    {
        dialog_open = false;
    }
    
    ImGui::Columns(current_depth, "directories");
    ImGui::Separator();
    pen::fs_tree_node* fs_iter = &fs_enumeration;
    
    s32 frame_depth = current_depth;
    for( s32 d = 0; d < frame_depth; ++d )
    {
        ImGui::Text("%s", fs_iter->name); ImGui::NextColumn();
        fs_iter = &fs_iter->children[ selection_stack[ d ] ];
    }
    
    ImGui::Separator();
    
    current_path = "";
    search_path = "";
    
    fs_iter = &fs_enumeration;
    
    for( s32 c = 0; c < frame_depth; ++c )
    {
        for( s32 entry = 0; entry < fs_iter->num_children; ++entry )
        {
            if( ImGui::Selectable( fs_iter->children[entry].name) )
            {
                search_path = current_path;
                search_path += fs_iter->children[entry].name;
                
                pen::filesystem_enum_directory( search_path.c_str(), fs_iter->children[entry] );
                
                if( fs_iter->children[entry].num_children > 0 )
                {
                    current_depth = c + 2;
                    selection_stack[c] = entry;
                }
                else
                {
                    selected_path = "";
                    selected_path = search_path;
                }
            }
        }
        
        fs_iter = &fs_iter->children[ selection_stack[c] ];
        current_path += fs_iter->name;
        current_path += "/";
        
        ImGui::NextColumn();
    }
    
    ImGui::Separator();
    ImGui::EndChild();
    ImGui::Columns(1);
    
    ImGui::End();
    
    if( !dialog_open )
    {
        initialise = true;
        filesystem_enum_free_mem(fs_enumeration);
    }
    
    return return_value;
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
    std::vector<u32> beats[k_num_fft_diff_buckets];
    f32 average_interval[k_num_fft_diff_buckets];

    void show_window( bool& open )
    {
        ImGui::Begin("Beat Grid");
        
        ImGui::BeginChild("scrolling", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        
        for( s32 i = 0; i < k_num_fft_diff_buckets; ++i )
        {
            average_interval[i] = 0.0f;
            
            u32 prev = 0;
            u32 counter = 0;
            for( auto& timestamp : beats[ i ] )
            {
                if( counter > 0 )
                {
                    ImGui::Text("%i", timestamp - prev ); ImGui::SameLine();
                    
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

class playback_deck
{
public:
    s32 flags = NONE;
    
    //audio handles
    u32 group_index;
    u32 sound_index;
    u32 channel_index;
    u32 spectrum_dsp;
    
    beat_grid grid;
    
    //audio states
    pen::audio_channel_state channel_state;
    pen::audio_fft_spectrum spectrum;
    pen::audio_group_state group_state;
    pen::audio_sound_file_info file_info;
    
    //filesystem
    bool open_file = false;
    bool open_beat_grid = false;
    const c8* file = nullptr;
    
    u32 cue_pos;
    s32 pitch_range;
    
    //debug info
    s32 fft_num_samples = 64;
    f32 fft_max = 1.0f;
    
    //analysis
    
    //general fft storage
    static const s32 num_analysis_buffers = 128;
    f32 fft_buffers[num_analysis_buffers][2048] = { 0 };
    f32 fft_timestamp[num_analysis_buffers] = { 0 };
    
    //comparison
    f32 fft_max_vals[2048];
    f32 fft_diff[2048];
    
    //the magic
    f32 fft_combined_diff[k_num_fft_diff_buckets];
    f32 prev_combined_diff[k_num_fft_diff_buckets];
    f32 fft_combined_history[num_analysis_buffers];
    
    //tracking
    s32 current_analysis_buffer_pos = 0;
    s32 plot_lines_diff_range = 0;
    
    void ui_control()
    {
        //file info
        ImGui::PushID(this);
        if( ImGui::Button("Open"))
        {
            open_file = true;
        }
        
        if( open_file )
        {
            const c8* file = file_browser( open_file );
            
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
        if( channel_state.play_state == pen::NOT_PLAYING )
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
        
        //update fft
        if( ImGui::CollapsingHeader("Spectrum Analysis") )
        {
            ImGui::SliderFloat("FFT Max", &fft_max, 0.0f, 1.0f );
            ImGui::InputInt("Num FFT Samples", &fft_num_samples);
            
            if( ImGui::Button("Open Beat Grid") )
            {
                open_beat_grid = true;
            }
            
            if( open_beat_grid )
            {
                grid.show_window(open_beat_grid);
            }
            
            s32 prev_analysis_buffer_pos = current_analysis_buffer_pos - 1;
            
            if( prev_analysis_buffer_pos < 0 )
                prev_analysis_buffer_pos = num_analysis_buffers - 1;
            
            ImGui::PlotHistogram("", &fft_buffers[ current_analysis_buffer_pos ][0], fft_num_samples, 0, NULL, 0.0f, fft_max, ImVec2(0,100));
            
            //statistical analysis of fft
            f32 total_diff = 0.0f;
            
            u32 diff_buffer = 0;
            
            pen::memory_set(fft_combined_diff,0,sizeof(fft_combined_diff));
            
            for( s32 samp = 0; samp < spectrum.length; ++samp )
            {
                //calculate max value over time
                fft_max_vals[samp] = 0.0f;
                for( s32 buf = 0; buf < num_analysis_buffers; ++buf)
                {
                    fft_max_vals[samp] = PEN_FMAX( fft_max_vals[samp], fft_buffers[ buf ][ samp ] );
                }
                
                //calculate differece from frame to frame and scale by the max
                fft_diff[samp] = fft_buffers[ current_analysis_buffer_pos ][ samp ] - fft_buffers[ prev_analysis_buffer_pos ][ samp ];
                
                //scale diff
                static const f32 epsilon = 0.000000001f;
                fft_diff[samp] = (fft_diff[samp] / (fft_max_vals[samp] + epsilon));
                
                //work out diff within ranges
                fft_combined_diff[diff_buffer] += fft_diff[samp];
                
                if( samp >= k_fft_diff_ranges[diff_buffer] )
                {
                    u32 prev_range = 0;
                    if( diff_buffer > 0 )
                    {
                        prev_range = k_fft_diff_ranges[diff_buffer-1];
                    }
                    
                    fft_combined_diff[diff_buffer] /= (f32)k_fft_diff_ranges[diff_buffer] - prev_range;
                    diff_buffer++;
                }
                
                f32 diff_cubed = fft_diff[samp] * fft_diff[samp] * fft_diff[samp];
                total_diff += diff_cubed;
            }
            
            //plot diff buckets
            if( ImGui::CollapsingHeader("Coarse Frequency Diff") )
            {
                ImGui::PlotHistogram("", fft_combined_diff, k_num_fft_diff_buckets, 0, NULL, 0.0f, fft_max, ImVec2(0,100));
                
                ImGui::Combo("Graph Diff Range", &plot_lines_diff_range, diff_range_desriptions, k_num_fft_diff_buckets);
                
                ImGui::PlotLines("", fft_combined_history, num_analysis_buffers, 0, NULL, -fft_max, fft_max, ImVec2(0,100));
                
                ImGui::Text("DISPLAY:");
                
                for( s32 i = 0; i < k_num_fft_diff_buckets; ++i )
                {
                    f32 cur_val = fft_combined_diff[ i ] - prev_combined_diff[ i ];
                    
                    if( i == plot_lines_diff_range )
                    {
                        fft_combined_history[ current_analysis_buffer_pos ] = fft_combined_diff[ i ];
                    }
                    
                    ImGui::SameLine();
                    if( fabs( cur_val ) > 0.2f )
                    {
                        ImGui::Text("[%s]", diff_range_nicknames[ i ] );
                        
                        u32 diff = 129;
                        
                        if( grid.beats[ i ].size() > 0 )
                            diff = channel_state.position_ms - grid.beats[ i ].back();
                        
                        if( diff > 128 )
                        {
                            //cool down of 33ms
                            grid.beats[ i ].push_back( channel_state.position_ms );
                        }
                    }
                    else
                    {
                        ImGui::Text("[    ]");
                    }
                }
            }
            
            if( ImGui::CollapsingHeader("Raw Frequency Diff") )
            {
                //plot raw diff
                ImGui::Text("Diff: %f", total_diff);
                ImGui::PlotHistogram("", fft_diff, fft_num_samples, 0, NULL, 0.0f, fft_max, ImVec2(0,100));
            }
            
            //update fft
            if( !(flags & PAUSE_FFT_UPDATE) )
            {
                err = pen::audio_dsp_get_spectrum(spectrum_dsp, &spectrum );
                
                if( err == PEN_ERR_OK )
                {
                    current_analysis_buffer_pos++;
                    
                    for( s32 samp = 0; samp < spectrum.length; ++samp )
                    {
                        fft_buffers[current_analysis_buffer_pos][samp] = 0.0f;
                        
                        for( s32 chan = 0; chan < spectrum.num_channels; ++chan )
                        {
                            fft_buffers[current_analysis_buffer_pos][samp] += spectrum.spectrum[chan][samp];
                        }
                        
                        fft_buffers[current_analysis_buffer_pos][samp] /= 2.0f;
                    }
                    
                    fft_timestamp[current_analysis_buffer_pos] = channel_state.position_ms;
                }
            }

            if( ImGui::InputInt("Analysis Buffer Pos", &current_analysis_buffer_pos) )
            {
                if( current_analysis_buffer_pos >= num_analysis_buffers )
                {
                    current_analysis_buffer_pos = 0;
                }
                else if( current_analysis_buffer_pos < 0 )
                {
                    current_analysis_buffer_pos = num_analysis_buffers - 1;
                }
                
                channel_state.position_ms = fft_timestamp[current_analysis_buffer_pos];
                
                pen::audio_channel_set_position(channel_index, fft_timestamp[current_analysis_buffer_pos] );
            }
            
            if( current_analysis_buffer_pos >= num_analysis_buffers )
            {
                current_analysis_buffer_pos = 0;
            }
            else if( current_analysis_buffer_pos < 0 )
            {
                current_analysis_buffer_pos = num_analysis_buffers - 1;
            }
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
