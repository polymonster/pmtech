#include "audio.h"
#include "pen.h"
#include "fmod.hpp"
#include <math.h>

namespace pen
{
#define MAX_CHANNELS 32
#define NUM_AUDIO_STATE_BUFFERS 2
#define INVALID_SOUND (u32)-1

    FMOD::System*			g_sound_system;

    enum audio_resource_type : s32
    {
        AUDIO_RESOURCE_VIRTUAL,
        AUDIO_RESOURCE_SOUND,
        AUDIO_RESOURCE_CHANNEL,
        AUDIO_RESOURCE_GROUP,
        AUDIO_RESOURCE_DSP_FFT,
        AUDIO_RESOURCE_DSP_EQ,
        AUDIO_RESOURCE_DSP_GAIN,
        AUDIO_RESOURCE_DSP
    };
    
    struct audio_resource_allocation
    {
        void* resource;
        u32 num_dsp = 0;
        
        audio_resource_type type;
        
        std::atomic<u8> assigned_flag;
    };
    
    struct resource_state
    {
        union
        {
            audio_channel_state     channel_state;
            audio_group_state       group_state;
            audio_fft_spectrum*     fft_spectrum;
            audio_eq_state          eq_state;
            f32                     gain_value;
        };
    };

    std::atomic<bool>           g_sound_file_info_ready[ MAX_AUDIO_RESOURCES ];
    audio_sound_file_info       g_sound_file_info[ MAX_AUDIO_RESOURCES ];
    resource_state              g_resource_states[ MAX_AUDIO_RESOURCES ][ NUM_AUDIO_STATE_BUFFERS ];
    audio_resource_allocation   g_audio_resources[ MAX_AUDIO_RESOURCES ];
    
    std::atomic<u32>            g_current_write_buffer;
    std::atomic<u32>            g_current_read_buffer;
    
    u32 get_next_audio_resource( u32 domain, audio_resource_type type )
    {
        //find next empty resource
        u32 i = 0;
        while( i < MAX_AUDIO_RESOURCES )
        {
            if( !(g_audio_resources[ i ].assigned_flag & domain ) )
            {
                g_audio_resources[ i ].assigned_flag |= domain;
                g_audio_resources[ i ].type = type;
                return i;
            }
            ++i;
        }

        return 0;
    }
    
    u32 get_next_audio_resource( u32 domain )
    {
        return get_next_audio_resource(domain, AUDIO_RESOURCE_VIRTUAL);
    }

    void direct::audio_system_initialise()
    {
        //clear resource array to 0
        pen::memory_zero(g_audio_resources, sizeof(g_audio_resources));

        //resource index 0 is used as a null resource
        g_audio_resources->assigned_flag |= DIRECT_RESOURCE | DEFER_RESOURCE;

        //init fmod
        FMOD_RESULT result;

        FMOD::System_Create(&g_sound_system);

        result = g_sound_system->init( MAX_CHANNELS, FMOD_INIT_NORMAL, NULL );
        
        //set resources to 0
        pen::memory_set(g_resource_states, 0, sizeof(g_resource_states));
        pen::memory_set(g_audio_resources, 0, sizeof(g_audio_resources));
        pen::memory_set(g_sound_file_info_ready, 0, sizeof(g_sound_file_info_ready));
        
        //reserve index 0 to use as null object
        g_audio_resources[0].assigned_flag = 0xff;
        
        //initialise double buffer
        g_current_write_buffer = 0;
        g_current_read_buffer = 1;

        PEN_ASSERT( result == FMOD_OK );
    }
    
    void update_channel_state( u32 resource_index )
    {
        audio_resource_allocation& res = g_audio_resources[ resource_index ];
        
        audio_channel_state* state = &g_resource_states[ resource_index ][ g_current_write_buffer ].channel_state;
        
        FMOD::Channel* channel = (FMOD::Channel*)res.resource;
        
        channel->getPosition(&state->position_ms, FMOD_TIMEUNIT_MS);
        channel->getPitch(&state->pitch);
        channel->getFrequency(&state->frequency);
        
        bool paused = false;
        channel->getPaused(&paused);
        
        bool playing = false;
        channel->isPlaying(&playing);
        
        if( !playing )
        {
            state->play_state = NOT_PLAYING;
        }
        else
        {
            state->play_state = PLAYING;
            
            if( paused )
            {
                state->play_state = PAUSED;
            }
        }
    }
    
    void update_group_state( u32 resource_index )
    {
        audio_resource_allocation& res = g_audio_resources[ resource_index ];
        
        audio_group_state* state = &g_resource_states[ resource_index ][ g_current_write_buffer ].group_state;
        
        FMOD::ChannelGroup* channel = (FMOD::ChannelGroup*)res.resource;
        
        channel->getPitch(&state->pitch);
        
        channel->getVolume(&state->volume);
        
        bool paused = false;
        channel->getPaused(&paused);
        
        bool playing = false;
        channel->isPlaying(&playing);
        
        if( !playing )
        {
            state->play_state = NOT_PLAYING;
        }
        else
        {
            state->play_state = PLAYING;
            
            if( paused )
            {
                state->play_state = PAUSED;
            }
        }
    }
    
    void update_fft( u32 resource_index )
    {
        audio_resource_allocation& res = g_audio_resources[ resource_index ];

        audio_fft_spectrum** fft = &g_resource_states[ resource_index ][ g_current_write_buffer ].fft_spectrum;
        
        FMOD::DSP* fft_dsp = (FMOD::DSP*)res.resource;
        
        FMOD_RESULT result = fft_dsp->getParameterData( FMOD_DSP_FFT_SPECTRUMDATA, (void**)fft, 0, 0, 0);
        
        PEN_ASSERT( result == FMOD_OK );
    }
    
    void update_three_band_eq( u32 resource_index )
    {
        FMOD::DSP* eq_dsp = (FMOD::DSP*)g_audio_resources[ resource_index ].resource;
        
        eq_dsp->getParameterFloat( FMOD_DSP_THREE_EQ_LOWGAIN, &g_resource_states[ resource_index ][ g_current_write_buffer ].eq_state.low, nullptr, 0 );
        eq_dsp->getParameterFloat( FMOD_DSP_THREE_EQ_MIDGAIN, &g_resource_states[ resource_index ][ g_current_write_buffer ].eq_state.med, nullptr, 0 );
        eq_dsp->getParameterFloat( FMOD_DSP_THREE_EQ_HIGHGAIN, &g_resource_states[ resource_index ][ g_current_write_buffer ].eq_state.high, nullptr, 0 );
    }
    
    void update_gain( u32 resource_index )
    {
        FMOD::DSP* gain_dsp = (FMOD::DSP*)g_audio_resources[ resource_index ].resource;
                                          
        gain_dsp->getParameterFloat( FMOD_DSP_CHANNELMIX_GAIN_CH0, &g_resource_states[ resource_index ][ g_current_write_buffer ].gain_value, nullptr, 0 );
    }

    void direct::audio_system_update()
    {
        g_sound_system->update();
        
        for( s32 i = 0; i < MAX_AUDIO_RESOURCES; ++i )
        {
            if( g_audio_resources[ i ].assigned_flag & DIRECT_RESOURCE )
            {
                switch (g_audio_resources[ i ].type)
                {
                    case AUDIO_RESOURCE_CHANNEL:
                    {
                        update_channel_state( i );
                    }
                    break;
                        
                    case AUDIO_RESOURCE_GROUP:
                    {
                        update_group_state( i );
                    }
                    break;
                        
                    case AUDIO_RESOURCE_DSP_FFT:
                    {
                        update_fft( i );
                    }
                    break;
                        
                    case AUDIO_RESOURCE_DSP_EQ:
                    {
                        update_three_band_eq( i );
                    }
                    break;
                        
                    case AUDIO_RESOURCE_DSP_GAIN:
                    {
                        update_gain( i );
                    }
                    break;
                        
                    default:
                        break;
                }
            }
        }
        
        //swap buffers
        u32 prev_read = g_current_read_buffer;
        s32 prev_write = g_current_write_buffer;
        
        g_current_read_buffer = prev_write;
        g_current_write_buffer = prev_read;
    }
    
    u32 direct::audio_create_sound( const c8* filename )
    {
        u32 res_index = get_next_audio_resource( DIRECT_RESOURCE, AUDIO_RESOURCE_SOUND );
        
        FMOD_RESULT result = g_sound_system->createSound( filename, FMOD_DEFAULT, NULL, (FMOD::Sound**)&g_audio_resources[res_index].resource );

        PEN_ASSERT( result == FMOD_OK );
        
        //populate sound info
        FMOD::Sound* new_sound = (FMOD::Sound*)g_audio_resources[res_index].resource;
        
        new_sound->getLength( &g_sound_file_info[ res_index ].length_ms, FMOD_TIMEUNIT_MS );
        
        g_sound_file_info_ready[ res_index ] = true;
        
        return res_index;
    }

    u32 direct::audio_create_stream( const c8* filename )
    {
        u32 res_index = get_next_audio_resource( DIRECT_RESOURCE, AUDIO_RESOURCE_SOUND );
        
        FMOD_RESULT result = g_sound_system->createStream( filename, FMOD_LOOP_NORMAL | FMOD_2D, 0, (FMOD::Sound**)&g_audio_resources[res_index].resource );

        PEN_ASSERT( result == FMOD_OK );

        return res_index;
    }

    u32 direct::audio_create_channel_group()
    {
        u32 res_index = get_next_audio_resource( DIRECT_RESOURCE, AUDIO_RESOURCE_GROUP );
        
        FMOD_RESULT result;
        
        result = g_sound_system->createChannelGroup( NULL, (FMOD::ChannelGroup**)&g_audio_resources[res_index].resource );
        
        PEN_ASSERT( result == FMOD_OK );

        return res_index;
    }

    u32 direct::audio_create_channel_for_sound(u32 sound_index)
    {
        u32 res_index = get_next_audio_resource( DIRECT_RESOURCE, AUDIO_RESOURCE_CHANNEL );
        
        FMOD_RESULT result;
        
        result = g_sound_system->playSound( 
            (FMOD::Sound*)g_audio_resources[sound_index].resource, 
            0, 
            false, 
            (FMOD::Channel**)&g_audio_resources[res_index].resource );

        PEN_ASSERT( result == FMOD_OK );

        return res_index;
    }
    
    void direct::audio_channel_set_position( const u32 channel_index, const u32 position_ms )
    {
        FMOD::Channel* p_chan = (FMOD::Channel*)g_audio_resources[channel_index].resource;
        
        p_chan->setPosition( position_ms, FMOD_TIMEUNIT_MS );
    }
    
    void direct::audio_channel_set_frequency( const u32 channel_index, const f32 frequency )
    {
        FMOD::Channel* p_chan = (FMOD::Channel*)g_audio_resources[channel_index].resource;
        
        p_chan->setFrequency(frequency);
    }
    
    void direct::audio_group_set_pause( const u32 group_index, const bool val )
    {
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)g_audio_resources[group_index].resource;
        
        p_group->setPaused( val );
    }
    
    void direct::audio_group_set_mute( const u32 group_index, const bool val )
    {
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)g_audio_resources[group_index].resource;
        
        p_group->setPaused( val );
    }
    
    void direct::audio_group_set_pitch( const u32 group_index, const f32 pitch )
    {
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)g_audio_resources[group_index].resource;
        
        p_group->setPitch( pitch );
    }
    
    void direct::audio_group_set_volume( const u32 group_index, const f32 volume )
    {
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)g_audio_resources[group_index].resource;
        
        p_group->setVolume( volume );
    }
    
    u32 direct::audio_release_resource( u32 index )
    {
        return 0;
    }
    
    void direct::audio_add_channel_to_group( const u32 channel_index, const u32 group_index )
    {
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)g_audio_resources[group_index].resource;
        FMOD::Channel* p_chan = (FMOD::Channel*)g_audio_resources[channel_index].resource;
        
        FMOD_RESULT result;
        
        result = p_chan->setChannelGroup( p_group );
        
        PEN_ASSERT( result == FMOD_OK );
    }
    
    FMOD_DSP_TYPE pen_dsp_to_fmod_type( dsp_type type, audio_resource_type& resource_type )
    {
        resource_type = AUDIO_RESOURCE_DSP;
        
        switch( type )
        {
            case pen::DSP_FFT:
                resource_type = AUDIO_RESOURCE_DSP_FFT;
                return FMOD_DSP_TYPE_FFT;
            case pen::DSP_THREE_BAND_EQ:
                resource_type = AUDIO_RESOURCE_DSP_EQ;
                return FMOD_DSP_TYPE_THREE_EQ;
            case pen::DSP_GAIN:
                resource_type = AUDIO_RESOURCE_DSP_GAIN;
                return FMOD_DSP_TYPE_CHANNELMIX;
            default:
                PEN_ERR;
        }
        
        return FMOD_DSP_TYPE_UNKNOWN;
    }
    
    u32 direct::audio_add_dsp_to_group( const u32 group_index, dsp_type type )
    {
        audio_resource_type res_type;
        FMOD_DSP_TYPE fmod_dsp = pen_dsp_to_fmod_type( type, res_type );
        
        u32 res_index = get_next_audio_resource( DIRECT_RESOURCE, res_type );
        
        FMOD_RESULT result;
        
        FMOD::DSP** new_dsp = (FMOD::DSP**)&g_audio_resources[res_index].resource;
        
        result = g_sound_system->createDSPByType( fmod_dsp, new_dsp );
        
        PEN_ASSERT( result == FMOD_OK );
        
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)g_audio_resources[group_index].resource;
        
        p_group->addDSP( g_audio_resources[group_index].num_dsp++, *new_dsp);
        
        return res_index;
    }
    
    void direct::audio_dsp_set_three_band_eq( const u32 eq_index, const f32 low, const f32 med, const f32 high )
    {
        FMOD::DSP* eq_dsp = (FMOD::DSP*)g_audio_resources[eq_index].resource;
        
        eq_dsp->setParameterFloat( 0, low);
        eq_dsp->setParameterFloat( 1, med);
        eq_dsp->setParameterFloat( 2, high);
    }
    
    void direct::audio_dsp_set_gain( const u32 dsp_index, const f32 gain )
    {
        FMOD::DSP* gain_dsp = (FMOD::DSP*)g_audio_resources[dsp_index].resource;
        
        gain_dsp->setParameterFloat(FMOD_DSP_CHANNELMIX_GAIN_CH0, gain);
        gain_dsp->setParameterFloat(FMOD_DSP_CHANNELMIX_GAIN_CH1, gain);
    }
    
    pen_error audio_channel_get_state( const u32 channel_index, audio_channel_state* state )
    {
        if( g_audio_resources[ channel_index ].assigned_flag & DIRECT_RESOURCE )
        {
            if( g_audio_resources[ channel_index ].type == AUDIO_RESOURCE_CHANNEL )
            {
                *state = g_resource_states[ channel_index ][ g_current_read_buffer ].channel_state;
                
                return PEN_ERR_OK;
            }
            
            return PEN_ERR_FAILED;
        }
        
        return PEN_ERR_NOT_READY;
    }
    
    pen_error audio_channel_get_sound_file_info( const u32 sound_index, audio_sound_file_info* info )
    {
        if( g_audio_resources[ sound_index ].assigned_flag & DIRECT_RESOURCE && g_sound_file_info_ready[ sound_index ] )
        {
            if( g_audio_resources[ sound_index ].type == AUDIO_RESOURCE_SOUND )
            {
                *info = g_sound_file_info[ sound_index ];
                
                return PEN_ERR_OK;
            }
            
            return PEN_ERR_FAILED;
        }
        
        return PEN_ERR_NOT_READY;
    }
    
    pen_error audio_group_get_state( const u32 group_index, audio_group_state* state )
    {
        if( g_audio_resources[ group_index ].assigned_flag & DIRECT_RESOURCE )
        {
            if( g_audio_resources[ group_index ].type == AUDIO_RESOURCE_GROUP )
            {
                *state = g_resource_states[ group_index ][ g_current_read_buffer ].group_state;
                
                return PEN_ERR_OK;
            }
            
            return PEN_ERR_FAILED;
        }
        
        return PEN_ERR_NOT_READY;
    }
    
    pen_error audio_dsp_get_spectrum( const u32 spectrum_dsp, audio_fft_spectrum* spectrum )
    {
        if( g_audio_resources[ spectrum_dsp ].assigned_flag & DIRECT_RESOURCE )
        {
            if( g_audio_resources[ spectrum_dsp ].type == AUDIO_RESOURCE_DSP_FFT )
            {
                if( g_resource_states[ spectrum_dsp ][ g_current_read_buffer ].fft_spectrum != nullptr )
                {
                    *spectrum = *g_resource_states[ spectrum_dsp ][ g_current_read_buffer ].fft_spectrum;
                }
                
                return PEN_ERR_OK;
            }
            
            return PEN_ERR_FAILED;
        }
        
        return PEN_ERR_NOT_READY;
    }
    
    pen_error audio_dsp_get_three_band_eq( const u32 eq_dsp, audio_eq_state* eq_state )
    {
        if( g_audio_resources[ eq_dsp ].assigned_flag & DIRECT_RESOURCE )
        {
            if( g_audio_resources[ eq_dsp ].type == AUDIO_RESOURCE_DSP_EQ )
            {
                *eq_state = g_resource_states[ eq_dsp ][ g_current_read_buffer ].eq_state;
                
                return PEN_ERR_OK;
            }
            
            return PEN_ERR_FAILED;
        }
        
        return PEN_ERR_NOT_READY;
    }
    
    pen_error   audio_dsp_get_gain( const u32 dsp_index, f32* gain )
    {
        if( g_audio_resources[ dsp_index ].assigned_flag & DIRECT_RESOURCE )
        {
            if( g_audio_resources[ dsp_index ].type == AUDIO_RESOURCE_DSP_GAIN )
            {
                *gain = g_resource_states[ dsp_index ][ g_current_read_buffer ].gain_value;
                
                return PEN_ERR_OK;
            }
            
            return PEN_ERR_FAILED;
        }
        
        return PEN_ERR_NOT_READY;
    }
}

#if 0
	void audio_channel_set_pitch( const u32 &channel_group, const f32 &pitch )
	{
		f32 abs_pitch = fabs( pitch );
	
		s32 num_channels = 0;
		g_channel_groups[channel_group]->getNumChannels( &num_channels );
		for (s32 i = 0; i < num_channels; ++i)
		{
			FMOD::Channel* chan;
			g_channel_groups[channel_group]->getChannel( i, &chan );

			f32 freq = 0.0f;
			chan->getFrequency( &freq );

			f32 abs_freq = fabs( freq );

			chan->setFrequency( pitch < 0.0f ? -abs_freq : abs_freq );
		}

		g_channel_groups[ channel_group ]->setPitch( abs_pitch );

		//EQ TEST
		/*
         
		FMOD::DSP* dspeqlow;
		g_sound_system->createDSPByType( FMOD_DSP_TYPE_PARAMEQ, &dspeqlow );

		dspeqlow->setParameter( 0, 30.0f );
		dspeqlow->setParameter( 1, 0.2f );
		dspeqlow->setParameter( 2, 2.0f );

		FMOD::DSPConnection* dspcon;
		g_channel_groups[ channel_group ]->addDSP( dspeqlow, &dspcon );
		*/
	}
}
#endif
