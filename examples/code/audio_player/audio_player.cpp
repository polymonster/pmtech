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

namespace
{
    pen::fs_tree_node fs_enumeration;
}

PEN_THREAD_RETURN pen::game_entry( void* params )
{
    //unpack the params passed to the thread and signal to the engine it ok to proceed
    pen::job_thread_params* job_params = (pen::job_thread_params*)params;
    pen::job_thread* p_thread_info = job_params->job_thread_info;
    pen::threads_semaphore_signal(p_thread_info->p_sem_continue, 1);
    
    renderer_state_init();

    dev_ui::init();

    pen::filesystem_enum_volumes(fs_enumeration);
    
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
    
    static bool goto_default = true;
    static s32 current_depth = 1;
    static s32 selection_stack[128] = { -1 };
    
    std::string current_path;
    std::string search_path;
    static std::string selected_path;
    
    if( goto_default )
    {
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
        
        goto_default = false;
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
    
    return return_value;
}

enum playback_deck_flags : s32
{
    NONE = 0,
    GROUP_STATE_VALID = 1<<0,
    CHANNEL_STATE_VALID = 1<<1,
    FILE_INFO_INVALID = 1<<3,
    FILE_INFO_VALID = 1<<4,
    PAUSE_FFT_UPDATE = 1<<5
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
    u32 three_band_eq_dsp;
    
    //audio states
    pen::audio_channel_state channel_state;
    pen::audio_fft_spectrum spectrum;
    pen::audio_group_state group_state;
    pen::audio_sound_file_info file_info;
    
    //filesystem
    bool open_file = false;
    const c8* file = nullptr;
    
    //debug info
    s32 fft_num_samples = 64;
    f32 fft_max = 1.0f;
    s32 fft_channel = 0;
    
    //analysis
    static const s32 num_analysis_buffers = 60;
    f32 fft_buffers[2][num_analysis_buffers][2048] = { 0 };
    s32 current_analysis_buffer_pos = 0;
    
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
        
        //transport controls
        if( channel_state.play_state == pen::NOT_PLAYING )
        {
            if( ImGui::Button("Play") )
            {
                channel_index = pen::audio_create_channel_for_sound( sound_index );
                pen::audio_add_channel_to_group(channel_index, group_index);
            }
        }
        else if( group_state.play_state == pen::PLAYING )
        {
            if( ImGui::Button("Pause") )
            {
                pen::audio_group_set_pause( group_index, true );
            }
        }
        else if( group_state.play_state == pen::PAUSED )
        {
            if( ImGui::Button("Play") )
            {
                pen::audio_group_set_pause( group_index, false );
            }
        }
        
        ImGui::SameLine();
        if( ImGui::Button("Stop") )
        {
        }
        
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
        
        //file info valid
        if( flags & FILE_INFO_VALID )
        {
            if( ImGui::SliderInt("Track Pos", (s32*)&channel_state.position_ms, 0, file_info.length_ms) )
            {
                pen::audio_channel_set_position(channel_index, channel_state.position_ms);
            }
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
        
        //update fft
        ImGui::SliderFloat("FFT Max", &fft_max, 0.0f, 1.0f );
        ImGui::InputInt("Num FFT Samples", &fft_num_samples);
        ImGui::InputInt("FFT Channel", &fft_channel);
        
        if( ImGui::Button("Pause FFT") )
        {
            if( flags & PAUSE_FFT_UPDATE )
            {
                flags &= ~(PAUSE_FFT_UPDATE);
            }
            else
            {
                flags |= PAUSE_FFT_UPDATE;
            }
        }

        ImGui::PlotHistogram("", &fft_buffers[ fft_channel ][ current_analysis_buffer_pos ][0], fft_num_samples, 0, NULL, 0.0f, fft_max, ImVec2(0,100));
        
        if( !(flags & PAUSE_FFT_UPDATE) )
        {
            err = pen::audio_dsp_get_spectrum(spectrum_dsp, &spectrum );
            
            if( err == PEN_ERR_OK )
            {
                for( s32 chan = 0; chan < spectrum.num_channels; ++chan )
                {
                    pen::memory_cpy( fft_buffers[ chan ][ current_analysis_buffer_pos ], &spectrum.spectrum[chan][0], sizeof(f32)*spectrum.length );
                }
                
                current_analysis_buffer_pos++;
            }
        }
        else
        {
            ImGui::InputInt("Analysis Buffer Pos", &current_analysis_buffer_pos);
        }
        
        if( current_analysis_buffer_pos >= num_analysis_buffers )
        {
            current_analysis_buffer_pos = 0;
        }
        else if( current_analysis_buffer_pos < 0 )
        {
            current_analysis_buffer_pos = num_analysis_buffers - 1;
        }
        
        if( fft_channel >= spectrum.num_channels )
        {
            fft_channel = 0;
        }
        
        ImGui::SameLine();
        
        f32 pitch_range = (1.0f / 100.0f ) * 8.0f;
        if( ImGui::VSliderFloat("Pitch", ImVec2( 20.0f, 100.0f ), &group_state.pitch, 1.0f - pitch_range, 1.0f + pitch_range, "" ) )
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
    pen::audio_group_state group_state;
    
    void control_ui()
    {
        pen::audio_group_get_state(group_index, &group_state);
        
        ImGui::PushID(&group_state.volume);
        if( ImGui::VSliderFloat("Vol", ImVec2( 20.0f, 100.0f ), &group_state.volume, 0.0f, 1.0f, "" ) )
        {
            pen::audio_group_set_volume(group_index, group_state.volume );
        }
        ImGui::PopID();
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
            decks[i].three_band_eq_dsp = pen::audio_add_dsp_to_group( decks[i].group_index, pen::DSP_THREE_BAND_EQ );
        }
        
        initialised = true;
    }
    
    ImGui::Begin("Player");
    
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
