#ifndef _audio_h
#define _audio_h

#include "definitions.h"

namespace pen
{
	//system
	void	audio_system_initialise( );

	void	audio_system_update( );

	//creation
	u32		audio_create_stream( const c8* filename );

	u32		audio_create_sound( const c8* filename );

	void	audio_create_sound_channel( const u32 &snd );

	u32		audio_create_channel_group( );

	void	audio_add_sound_to_channel_group( const u32 &snd, const u32 &channel_group );

	//manipulation
	void	audio_channel_pause( const u32 &channel_group, const u32 &val );

	void	audio_channel_set_pitch( const u32 &channel_group, const f32 &pitch );

	void	audio_sound_set_frequency( const u32 &snd );

	void	audio_channel_set_volume( const u32 &channel_group, const f32 &volume );

	void	audio_channel_set_position( const u32 &channel_group, u32 pos_ms );

	u32		audio_channel_get_position( const u32 &channel_group );

	u32		audio_get_sound_length( const u32 &snd );
	
	u32		audio_is_channel_playing( const u32 &chan );

	void	audio_channel_get_spectrum( const u32 &channel_group, float *spectrum_array, u32 sample_size, u32 channel_offset );
}

#endif

