#include "audio.h"
#include "pen.h"
#include "fmod.hpp"
#include <math.h>

namespace pen
{
#define MAX_CHANNELS 32
#define INVALID_SOUND (u32)-1
#define MAX_RENDERER_RESOURCES 100

    FMOD::System*			g_sound_system;

    enum audio_resource_type : s32
    {
        AUDIO_RESOURCE_SOUND,
        AUDIO_RESOURCE_CHANNEL,
        AUDIO_RESOURCE_GROUP
    };
    
    struct audio_resource_allocation
    {
        audio_resource_type type;
        u8 assigned_flag = 0;
        void* resource;
    };

    audio_resource_allocation   g_audio_resources[ MAX_AUDIO_RESOURCES ];

    u32 get_next_audio_resource( u32 domain )
    {
        //find next empty resource
        u32 i = 0;
        while( i < MAX_AUDIO_RESOURCES )
        {
            if( !(g_audio_resources[ i ].assigned_flag & domain ) )
            {
                g_audio_resources[ i ].assigned_flag |= domain;
                return i;
            }

            ++i;
        }

        //return null
        return 0;
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

        PEN_ASSERT( result == FMOD_OK );
    } 

    void direct::audio_system_update()
    {
        g_sound_system->update();
    }

    u32 direct::audio_create_sound( const c8* filename )
    {
        u32 res_index = get_next_audio_resource( DIRECT_RESOURCE );
        g_audio_resources[res_index].type = AUDIO_RESOURCE_SOUND;
        
        FMOD_RESULT result = g_sound_system->createSound( filename, FMOD_DEFAULT, NULL, (FMOD::Sound**)&g_audio_resources[res_index].resource );

        PEN_ASSERT( result == FMOD_OK );

        return res_index;
    }

    u32 direct::audio_create_stream( const c8* filename )
    {
        u32 res_index = get_next_audio_resource( DIRECT_RESOURCE );
        g_audio_resources[res_index].type = AUDIO_RESOURCE_SOUND;
        
        FMOD_RESULT result = g_sound_system->createStream( filename, FMOD_LOOP_NORMAL | FMOD_2D, 0, (FMOD::Sound**)&g_audio_resources[res_index].resource );

        PEN_ASSERT( result == FMOD_OK );

        return res_index;
    }

    u32 direct::audio_create_channel_group()
    {
        u32 res_index = get_next_audio_resource( DIRECT_RESOURCE );
        g_audio_resources[res_index].type = AUDIO_RESOURCE_GROUP;
        
        FMOD_RESULT result;
        
        result = g_sound_system->createChannelGroup( NULL, (FMOD::ChannelGroup**)&g_audio_resources[res_index].resource );
        
        PEN_ASSERT( result == FMOD_OK );

        return res_index;
    }

    u32 direct::audio_create_channel_for_sound(u32 sound_index)
    {
        u32 res_index = get_next_audio_resource( DIRECT_RESOURCE );
        g_audio_resources[res_index].type = AUDIO_RESOURCE_SOUND;
        
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
}

#if 0
	u32 audio_create_channel_group( )
	{
		u32 group = get_free_channel_group( );

		if( group != INVALID_SOUND )
		{
			FMOD_RESULT result;

			result = g_sound_system->createChannelGroup(NULL, &g_channel_groups[ group ] );

			PEN_ASSERT( result == FMOD_OK );

			return group;
		}

		return INVALID_SOUND;
	}

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

	void audio_channel_set_volume( const u32 &channel_group, const f32 &volume )
	{
		g_channel_groups[ channel_group ]->setVolume( volume );
	}

	void audio_channel_pause( const u32 &channel_group, const u32 &val )
	{
		g_channel_groups[channel_group]->setPaused( val == 0 ? false : true );
	}

	void audio_sound_set_frequency( const u32 &snd )
	{
		f32 freq = 0.0f;
		g_channels[ snd ]->getFrequency( &freq );
		g_channels[ snd ]->setFrequency( -freq );
	}

	void audio_channel_get_spectrum( const u32 &channel_group, float *spectrum_array, u32 sample_size, u32 channel_offset )
	{
		g_channel_groups[channel_group]->getSpectrum( spectrum_array, sample_size, channel_offset, FMOD_DSP_FFT_WINDOW_BLACKMANHARRIS );
	}

	void audio_channel_set_position( const u32 &channel_group, u32 pos_ms )
	{
		s32 num_channels = 0;
		g_channel_groups[channel_group]->getNumChannels( &num_channels );
		for (s32 i = 0; i < num_channels; ++i)
		{
			FMOD::Channel* chan;
			g_channel_groups[channel_group]->getChannel( i, &chan );

			chan->setPosition( pos_ms, FMOD_TIMEUNIT_MS );
		}
	}

	u32 audio_channel_get_position( const u32 &channel_group )
	{
		s32 num_channels = 0;
		g_channel_groups[channel_group]->getNumChannels( &num_channels );

		for (s32 i = 0; i < num_channels; ++i)
		{
			FMOD::Channel* chan;
			g_channel_groups[channel_group]->getChannel( i, &chan );

			u32 pos_out;
			chan->getPosition( &pos_out, FMOD_TIMEUNIT_MS );

			return pos_out;
		}

		return 0;
	}

	u32 audio_get_sound_length( const u32 &snd )
	{
		u32 length_out = 0;
		g_sounds[ snd ]->getLength( &length_out, FMOD_TIMEUNIT_MS );

		return length_out;
	}

	u32 audio_is_channel_playing( const u32 &chan )
	{
		bool ip;
		g_channels[ chan ]->isPlaying( &ip );

		return (u32)ip;
	}
}
#endif
