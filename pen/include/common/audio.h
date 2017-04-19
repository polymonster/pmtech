#ifndef _audio_h
#define _audio_h

#include "definitions.h"

namespace pen
{
    enum audio_play_state : s32
    {
        NOT_PLAYING = 0,
        PLAYING = 2,
        PAUSED = 3
    };
    
    enum dsp_type : s32
    {
        DSP_FFT,
        DSP_THREE_BAND_EQ,
        DSP_GAIN
    };
    
    struct audio_eq_state
    {
        f32 low, med, high;
    };

    struct audio_group_state
    {
        u32     play_state;
        f32     pitch;
        f32     volume;
    };
    
    struct audio_sound_file_info
    {
        u32 length_ms;
    };

    struct audio_channel_state
    {
        u32     play_state;
        u32     position_ms;
        f32     pitch;
        f32     volume;
        f32     frequency;
    };
    
    struct audio_fft_spectrum
    {
        s32 length;
        s32 num_channels;
        f32 *spectrum[32];
    };
    
    //threading
    PEN_THREAD_RETURN	audio_thread_function( void* params );
    void                audio_consume_command_buffer();
         
    //creation
    u32		audio_create_stream( const c8* filename );
    u32		audio_create_sound( const c8* filename );
    u32	    audio_create_channel_for_sound( const u32 sound_index );
    u32		audio_create_channel_group( );
    void    audio_release_resource( u32 index );

    //binding
    void	audio_add_channel_to_group( const u32 channel_index, const u32 group_index );
    u32     audio_add_dsp_to_group( const u32 group_index, dsp_type type );

    //manipulation
    void	audio_channel_set_position( const u32 channel_index, const u32 position_ms );
    void	audio_channel_set_frequency( const u32 channel_index, const f32 frequency );

    void	audio_group_set_pause( const u32 group_index, const bool val );
    void	audio_group_set_mute( const u32 group_index, const bool val );
    void	audio_group_set_pitch( const u32 group_index, const f32 pitch );
    void	audio_group_set_volume( const u32 group_index, const f32 volume );
    
    void    audio_dsp_set_three_band_eq( const u32 eq_index, const f32 low, const f32 med, const f32 high );
    void    audio_dsp_set_gain( const u32 dsp_index, const f32 gain );
    
    //accessors
    pen_error   audio_channel_get_state( const u32 channel_index, audio_channel_state* state );
    pen_error   audio_channel_get_sound_file_info( const u32 sound_index, audio_sound_file_info* info );
    pen_error   audio_group_get_state( const u32 group_index, audio_group_state* state );
    pen_error   audio_dsp_get_spectrum( const u32 spectrum_dsp, audio_fft_spectrum* spectrum );
    pen_error   audio_dsp_get_three_band_eq( const u32 eq_dsp, audio_eq_state* eq_state );
    pen_error   audio_dsp_get_gain( const u32 dsp_index, f32* gain );

    namespace direct
    {
        //system
        void	audio_system_initialise();
        void	audio_system_update();

        //creation
        u32		audio_create_stream( const c8* filename );
        u32		audio_create_sound( const c8* filename );
        u32     audio_create_channel_for_sound( u32 sound_index );
        u32     audio_create_channel_group();
        u32     audio_release_resource( u32 index );
        
        //binding
        void    audio_add_channel_to_group( const u32 channel_index, const u32 group_index );
        u32     audio_add_dsp_to_group( const u32 group_index, dsp_type type );
        
        //manipulation
        void	audio_channel_set_position( const u32 channel_index, const u32 position_ms );
        void	audio_channel_set_frequency( const u32 channel_index, const f32 frequency );
        
        void	audio_group_set_pause( const u32 group_index, const bool val );
        void	audio_group_set_mute( const u32 group_index, const bool val );
        void	audio_group_set_pitch( const u32 group_index, const f32 pitch );
        void	audio_group_set_volume( const u32 group_index, const f32 volume );
        
        void    audio_dsp_set_three_band_eq( const u32 eq_index, const f32 low, const f32 med, const f32 high );
        void    audio_dsp_set_gain( const u32 dsp_index, const f32 gain );
    }
}

#endif

