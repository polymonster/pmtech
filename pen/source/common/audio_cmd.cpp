#include "audio.h"
#include "memory.h"
#include "threads.h"
#include "pen_string.h"
#include "fmod.hpp"
#include "slot_resource.h"
#include <math.h>

namespace pen
{
#define MAX_AUDIO_COMMANDS (1<<12)
#define INC_WRAP( V ) V = (V+1) & (MAX_AUDIO_COMMANDS-1);

    enum commands : u32
    {
        CMD_AUDIO_CREATE_STREAM,
        CMD_AUDIO_CREATE_SOUND,
        CMD_AUDIO_CREATE_GROUP,
        CMD_AUDIO_CREATE_CHANNEL_FOR_SOUND,
        CMD_AUDIO_RELEASE_RESOURCE,

        CMD_AUDIO_ADD_CHANNEL_TO_GROUP,
        CMD_AUDIO_ADD_DSP_TO_GROUP,

        CMD_AUDIO_CHANNEL_SET_POSITION,
        CMD_AUDIO_CHANNEL_SET_FREQUENCY,
        CMD_AUDIO_CHANNEL_STOP,
        
        CMD_AUDIO_GROUP_SET_PAUSE,
        CMD_AUDIO_GROUP_SET_MUTE,
        CMD_AUDIO_GROUP_SET_PITCH,
        CMD_AUDIO_GROUP_SET_VOLUME,
        
        CMD_AUDIO_DSP_SET_THREE_BAND_EQ,
        CMD_AUDIO_DSP_SET_GAIN
    };
    
    struct set_valuei
    {
        u32 resource_index;
        s32 value;
    };
    
    struct set_valuef
    {
        u32 resource_index;
        f32 value;
    };
    
    struct set_value3f
    {
        u32 resource_index;
        f32 value[3];
    };

    struct  audio_cmd
    {
        u32		command_index;
        u32     resource_slot;
        
        union
        {
            c8*         filename;
            u32         resource_index;
            set_valuei  _set_valuei;
            set_valuef  _set_valuef;
            set_value3f _set_value3f;
        };
    };

    audio_cmd audio_cmd_buffer[ MAX_AUDIO_COMMANDS ];
    u32       audio_put_pos = 0;
    u32       audio_get_pos = 0;

    void    audio_exec_command( const audio_cmd& cmd )
    {
        switch( cmd.command_index )
        {
        case CMD_AUDIO_CREATE_STREAM:
            direct::audio_create_stream( cmd.filename, cmd.resource_slot );
            pen::memory_free(cmd.filename);
            break;
        case CMD_AUDIO_CREATE_SOUND:
            direct::audio_create_sound( cmd.filename, cmd.resource_slot );
            pen::memory_free( cmd.filename );
            break;
        case CMD_AUDIO_CREATE_GROUP:
            direct::audio_create_channel_group( cmd.resource_slot );
            pen::memory_free( cmd.filename );
            break;
        case CMD_AUDIO_ADD_CHANNEL_TO_GROUP:
            direct::audio_add_channel_to_group( cmd._set_valuei.resource_index, cmd._set_valuei.value );
            break;
        case CMD_AUDIO_ADD_DSP_TO_GROUP:
            direct::audio_add_dsp_to_group( cmd._set_valuei.resource_index, (dsp_type)cmd._set_valuei.value, cmd.resource_slot );
            break;
        case CMD_AUDIO_CREATE_CHANNEL_FOR_SOUND:
            direct::audio_create_channel_for_sound( cmd.resource_index, cmd.resource_slot );
            break;
        case CMD_AUDIO_CHANNEL_SET_POSITION:
            direct::audio_channel_set_position( cmd._set_valuei.resource_index, cmd._set_valuei.value );
            break;
        case CMD_AUDIO_CHANNEL_SET_FREQUENCY:
            direct::audio_channel_set_frequency( cmd.resource_index, cmd._set_valuef.value );
            break;
        case CMD_AUDIO_CHANNEL_STOP:
            direct::audio_channel_stop( cmd.resource_index );
            break;
        case CMD_AUDIO_GROUP_SET_MUTE:
            direct::audio_group_set_mute( cmd._set_valuei.resource_index, (bool)cmd._set_valuei.value );
            break;
        case CMD_AUDIO_GROUP_SET_PAUSE:
            direct::audio_group_set_pause( cmd._set_valuei.resource_index, (bool)cmd._set_valuei.value );
            break;
        case CMD_AUDIO_GROUP_SET_VOLUME:
            direct::audio_group_set_volume( cmd._set_valuef.resource_index, cmd._set_valuef.value );
            break;
        case CMD_AUDIO_DSP_SET_GAIN:
            direct::audio_dsp_set_gain( cmd._set_valuef.resource_index, cmd._set_valuef.value );
            break;
        case CMD_AUDIO_GROUP_SET_PITCH:
            direct::audio_group_set_pitch( cmd._set_valuef.resource_index, cmd._set_valuef.value );
            break;
        case CMD_AUDIO_RELEASE_RESOURCE:
            direct::audio_release_resource( cmd.resource_index );
            break;
            case CMD_AUDIO_DSP_SET_THREE_BAND_EQ:
            direct::audio_dsp_set_three_band_eq(cmd._set_value3f.resource_index, 
                                                cmd._set_value3f.value[0], cmd._set_value3f.value[1], cmd._set_value3f.value[2] );
                break;
        }
    }

    //thread sync
    pen::job*         p_audio_job_thread_info;
    pen::slot_resources      k_audio_slot_resources;
    
    void audio_consume_command_buffer()
    {
        pen::thread_semaphore_signal( p_audio_job_thread_info->p_sem_consume, 1 );
        pen::thread_semaphore_wait( p_audio_job_thread_info->p_sem_continue );
    }

    PEN_TRV audio_thread_function( void* params )
    {
        job_thread_params* job_params = (job_thread_params*)params;
        p_audio_job_thread_info = job_params->job_info;
        
        //create resource slots
        pen::slot_resources_init(&k_audio_slot_resources, MAX_AUDIO_RESOURCES);
        
		direct::audio_system_initialise();
        
		//allow main thread to continue now we are initialised
		pen::thread_semaphore_signal(p_audio_job_thread_info->p_sem_continue, 1);

        for(;;)
        {
            if( pen::thread_semaphore_try_wait( p_audio_job_thread_info->p_sem_consume ) )
            {
                u32 end_pos = audio_put_pos;
                
                pen::thread_semaphore_signal( p_audio_job_thread_info->p_sem_continue, 1 );

                while( audio_get_pos != end_pos )
                {
                    audio_exec_command( audio_cmd_buffer[ audio_get_pos ] );

                    INC_WRAP( audio_get_pos );
                }

                direct::audio_system_update();
            }
            else
            {
                pen::thread_sleep_ms(1);
            }
            
            if( pen::thread_semaphore_try_wait(p_audio_job_thread_info->p_sem_exit) )
            {
                break;
            }
        }
        
        direct::audio_system_shutdown();
        
        pen::thread_semaphore_signal( p_audio_job_thread_info->p_sem_continue, 1 );
        pen::thread_semaphore_signal( p_audio_job_thread_info->p_sem_terminated, 1 );

		return PEN_THREAD_OK;
    }

    void    create_file_command( const c8* filename, u32 command, u32 resource_slot )
    {
        //allocate filename and copy the buffer and null terminate it
        u32 filename_length = pen::string_length( filename );
        audio_cmd_buffer[ audio_put_pos ].filename = ( c8* ) pen::memory_alloc( filename_length + 1 );
        audio_cmd_buffer[ audio_put_pos ].filename[filename_length] = 0x00;
        audio_cmd_buffer[ audio_put_pos ].resource_slot = resource_slot;
        
        pen::memory_cpy( audio_cmd_buffer[ audio_put_pos ].filename, filename, filename_length );

        //set command (create stream or sound)
        audio_cmd_buffer[ audio_put_pos ].command_index = command;

        INC_WRAP( audio_put_pos );
    }

    u32		audio_create_stream( const c8* filename )
    {
        u32 res = pen::slot_resources_get_next(&k_audio_slot_resources);

        create_file_command( filename, CMD_AUDIO_CREATE_STREAM, res );

        return res;
    }

    u32		audio_create_sound( const c8* filename )
    {
        u32 res = pen::slot_resources_get_next(&k_audio_slot_resources);

        create_file_command( filename, CMD_AUDIO_CREATE_SOUND, res );

        return res;
    }

    u32     audio_create_channel_group()
    {
        u32 res = pen::slot_resources_get_next(&k_audio_slot_resources);

        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_CREATE_GROUP;
        audio_cmd_buffer[ audio_put_pos ].resource_slot = res;

        INC_WRAP( audio_put_pos );

        return res;
    }

    u32	    audio_create_channel_for_sound( const u32 sound_index )
    {
        if( sound_index == 0 )
        {
            return 0;
        }
        
        u32 res = pen::slot_resources_get_next(&k_audio_slot_resources);

        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_CREATE_CHANNEL_FOR_SOUND;
        audio_cmd_buffer[ audio_put_pos ].resource_index = sound_index;
        audio_cmd_buffer[ audio_put_pos ].resource_slot = res;

        INC_WRAP( audio_put_pos );

        return res;
    }
    
    void	audio_channel_set_position( const u32 channel_index, const u32 position_ms )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_CHANNEL_SET_POSITION;
        audio_cmd_buffer[ audio_put_pos ]._set_valuei.resource_index = channel_index;
        audio_cmd_buffer[ audio_put_pos ]._set_valuei.value = position_ms;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_channel_set_frequency( const u32 channel_index, const f32 frequency )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_CHANNEL_SET_FREQUENCY;
        audio_cmd_buffer[ audio_put_pos ]._set_valuef.resource_index = channel_index;
        audio_cmd_buffer[ audio_put_pos ]._set_valuef.value = frequency;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_group_set_pause( const u32 group_index, const bool val )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_GROUP_SET_PAUSE;
        audio_cmd_buffer[ audio_put_pos ]._set_valuei.resource_index = group_index;
        audio_cmd_buffer[ audio_put_pos ]._set_valuei.value = (s32)val;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_group_set_mute( const u32 group_index, const bool val )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_GROUP_SET_MUTE;
        audio_cmd_buffer[ audio_put_pos ]._set_valuei.resource_index = group_index;
        audio_cmd_buffer[ audio_put_pos ]._set_valuei.value = (s32)val;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_group_set_pitch( const u32 group_index, const f32 pitch )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_GROUP_SET_PITCH;
        audio_cmd_buffer[ audio_put_pos ]._set_valuef.resource_index = group_index;
        audio_cmd_buffer[ audio_put_pos ]._set_valuef.value = pitch;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_group_set_volume( const u32 group_index, const f32 volume )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_GROUP_SET_VOLUME;
        audio_cmd_buffer[ audio_put_pos ]._set_valuef.resource_index = group_index;
        audio_cmd_buffer[ audio_put_pos ]._set_valuef.value = volume;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_add_channel_to_group( const u32 channel_index, const u32 group_index )
    {
        if( group_index == 0 || channel_index == 0 )
        {
            return;
        }
        
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_ADD_CHANNEL_TO_GROUP;
        audio_cmd_buffer[ audio_put_pos ]._set_valuei.resource_index = channel_index;
        audio_cmd_buffer[ audio_put_pos ]._set_valuei.value = group_index;
        
        INC_WRAP( audio_put_pos );
    }
    
    void     audio_release_resource( u32 index )
    {
        if (!pen::slot_resources_free( &k_audio_slot_resources, index ))
            return;

        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_RELEASE_RESOURCE;
        audio_cmd_buffer[ audio_put_pos ].resource_index = index;
                
        INC_WRAP( audio_put_pos );
    }
    
    u32     audio_add_dsp_to_group( const u32 group_index, dsp_type type )
    {
        u32 res = pen::slot_resources_get_next(&k_audio_slot_resources);
        
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_ADD_DSP_TO_GROUP;
        audio_cmd_buffer[ audio_put_pos ]._set_valuei.resource_index = group_index;
        audio_cmd_buffer[ audio_put_pos ]._set_valuei.value = type;
        audio_cmd_buffer[ audio_put_pos ].resource_slot = res;
        
        INC_WRAP( audio_put_pos );

        return res;
    }
    
    void audio_dsp_set_three_band_eq( const u32 eq_index, const f32 low, const f32 med, const f32 high )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_DSP_SET_THREE_BAND_EQ;
        audio_cmd_buffer[ audio_put_pos ]._set_value3f.resource_index = eq_index;
        audio_cmd_buffer[ audio_put_pos ]._set_value3f.value[0] = low;
        audio_cmd_buffer[ audio_put_pos ]._set_value3f.value[1] = med;
        audio_cmd_buffer[ audio_put_pos ]._set_value3f.value[2] = high;
        
        INC_WRAP( audio_put_pos );
    }
    
    void audio_dsp_set_gain( const u32 dsp_index, const f32 gain )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_DSP_SET_GAIN;
        audio_cmd_buffer[ audio_put_pos ]._set_valuef.resource_index = dsp_index;
        audio_cmd_buffer[ audio_put_pos ]._set_valuef.value = gain;
        
        INC_WRAP( audio_put_pos );
    }
    
    void audio_channel_stop( const u32 channel_index )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_CHANNEL_STOP;
        audio_cmd_buffer[ audio_put_pos ].resource_index = channel_index;
        
        INC_WRAP( audio_put_pos );
    }
}
