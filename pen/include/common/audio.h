#ifndef _audio_h
#define _audio_h

#include "definitions.h"

namespace pen
{    enum audio_play_state : s32
    {
        NOT_PLAYING = 0,
        PLAYING = 2,
        PAUSED = 3
    };

    struct audio_group_state
    {
        u32     play_state;
        f32     pitch;
        f32     volume;
    };

    struct audio_channel_state
    {
        u32     play_state;
        f32     pitch;
        f32     volume;
        f32     frequency;
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

    //manipulation
    void	audio_channel_set_position( const u32 channel_index, const u32 position_ms );
    void	audio_channel_set_frequency( const u32 channel_index, const f32 frequency );

    void	audio_group_set_pause( const u32 group_index, const bool val );
    void	audio_group_set_mute( const u32 group_index, const bool val );
    void	audio_group_set_pitch( const u32 group_index, const f32 pitch );
    void	audio_group_set_volume( const u32 group_index, const f32 volume );

    //accessors
    u32		audio_channel_get_state( const u32 channel_index, audio_channel_state& state );
    u32		audio_channel_get_length( const u32 &sound_index );

    u32		audio_group_get_state( const u32 group_index, audio_group_state& state );
    void	audio_group_get_spectrum( const u32 &channel_group, float *spectrum_array, u32 sample_size, u32 channel_offset );

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
        
        void    audio_add_channel_to_group( const u32 channel_index, const u32 group_index );
        
        //manipulation
        void	audio_channel_set_position( const u32 channel_index, const u32 position_ms );
        void	audio_channel_set_frequency( const u32 channel_index, const f32 frequency );
        
        void	audio_group_set_pause( const u32 group_index, const bool val );
        void	audio_group_set_mute( const u32 group_index, const bool val );
        void	audio_group_set_pitch( const u32 group_index, const f32 pitch );
        void	audio_group_set_volume( const u32 group_index, const f32 volume );
    }
}

#endif

