#include "audio.h"
#include "threads.h"
#include "fmod.hpp"
#include <math.h>

namespace pen
{
#define MAX_AUDIO_COMMANDS 10000
#define INC_WRAP( V ) V = (V+1)%MAX_AUDIO_COMMANDS

    extern u32 get_next_audio_resource( u32 domain );

    enum commands : u32
    {
        CMD_AUDIO_CREATE_STREAM,
        CMD_AUDIO_CREATE_SOUND,
        CMD_AUDIO_CREATE_GROUP,
        CMD_AUDIO_CREATE_CHANNEL_FOR_SOUND,
        CMD_AUDIO_RELEASE_RESOURCE,

        CMD_AUDIO_ADD_CHANNEL_TO_GROUP,

        CMD_AUDIO_CHANNEL_SET_POSITION,
        CMD_AUDIO_CHANNEL_SET_FREQUENCY,
        
        CMD_AUDIO_GROUP_SET_PAUSE,
        CMD_AUDIO_GROUP_SET_MUTE,
        CMD_AUDIO_GROUP_SET_PITCH,
        CMD_AUDIO_GROUP_SET_VOLUME,
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

    struct  audio_cmd
    {
        u32		command_index;

        union
        {
            c8*         filename;
            u32         resource_index;
            set_valuei  set_valuei;
            set_valuef  set_valuef;
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
            direct::audio_create_stream( cmd.filename );
            pen::memory_free(cmd.filename);
            break;
        case CMD_AUDIO_CREATE_SOUND:
            direct::audio_create_sound( cmd.filename );
            pen::memory_free( cmd.filename );
            break;
        case CMD_AUDIO_CREATE_GROUP:
            direct::audio_create_channel_group( );
            pen::memory_free( cmd.filename );
            break;
        case CMD_AUDIO_ADD_CHANNEL_TO_GROUP:
            direct::audio_add_channel_to_group( cmd.set_valuei.resource_index, cmd.set_valuei.value );
            break;
        case CMD_AUDIO_CREATE_CHANNEL_FOR_SOUND:
            direct::audio_create_channel_for_sound( cmd.resource_index );
            break;
        case CMD_AUDIO_CHANNEL_SET_POSITION:
            direct::audio_channel_set_position( cmd.set_valuei.resource_index, cmd.set_valuei.value );
            break;
        case CMD_AUDIO_CHANNEL_SET_FREQUENCY:
            direct::audio_channel_set_frequency( cmd.set_valuef.resource_index, cmd.set_valuef.value );
            break;
        case CMD_AUDIO_GROUP_SET_MUTE:
            direct::audio_group_set_mute( cmd.set_valuei.resource_index, (bool)cmd.set_valuei.value );
            break;
        case CMD_AUDIO_GROUP_SET_VOLUME:
            direct::audio_group_set_volume( cmd.set_valuef.resource_index, cmd.set_valuef.value );
            break;
        case CMD_AUDIO_GROUP_SET_PITCH:
            direct::audio_group_set_pitch( cmd.set_valuef.resource_index, cmd.set_valuef.value );
            break;
        case CMD_AUDIO_RELEASE_RESOURCE:
            direct::audio_release_resource( cmd.resource_index );
            break;
        }
    }

    //thread sync
    pen::job_thread*         p_audio_job_thread_info;

    void audio_consume_command_buffer()
    {
        pen::threads_semaphore_signal( p_audio_job_thread_info->p_sem_consume, 1 );
        pen::threads_semaphore_wait( p_audio_job_thread_info->p_sem_continue );
    }

    PEN_THREAD_RETURN audio_thread_function( void* params )
    {
        job_thread_params* job_params = (job_thread_params*)params;
        p_audio_job_thread_info = job_params->job_thread_info;
        
        direct::audio_system_initialise();

        //allow main thread to continue now we are initialised
        pen::threads_semaphore_signal( p_audio_job_thread_info->p_sem_continue, 1 );

        while( 1 )
        {
            if( pen::threads_semaphore_try_wait( p_audio_job_thread_info->p_sem_consume ) )
            {
                u32 end_pos = audio_put_pos;
                
                pen::threads_semaphore_signal( p_audio_job_thread_info->p_sem_continue, 1 );

                while( audio_get_pos != end_pos )
                {
                    audio_exec_command( audio_cmd_buffer[ audio_get_pos ] );

                    INC_WRAP( audio_get_pos );
                }

                direct::audio_system_update();
            }
            else
            {
                pen::threads_sleep_ms(1);
            }
            
            if( pen::threads_semaphore_try_wait(p_audio_job_thread_info->p_sem_exit) )
            {
                break;
            }
        }
        
        pen::threads_semaphore_signal( p_audio_job_thread_info->p_sem_continue, 1 );
        pen::threads_semaphore_signal( p_audio_job_thread_info->p_sem_terminated, 1 );

		return PEN_THREAD_OK;
    }

    void    create_file_command( const c8* filename, u32 command )
    {
        //allocate filename and copy the buffer and null terminate it
        u32 filename_length = pen::string_length( filename );
        audio_cmd_buffer[ audio_put_pos ].filename = ( c8* ) pen::memory_alloc( filename_length + 1 );
        audio_cmd_buffer[ audio_put_pos ].filename[filename_length] = 0x00;

        pen::memory_cpy( audio_cmd_buffer[ audio_put_pos ].filename, filename, filename_length );

        //set command (create stream or sound)
        audio_cmd_buffer[ audio_put_pos ].command_index = command;

        INC_WRAP( audio_put_pos );
    }

    u32		audio_create_stream( const c8* filename )
    {
        u32 res = get_next_audio_resource( DEFER_RESOURCE );

        create_file_command( filename, CMD_AUDIO_CREATE_STREAM );

        return res;
    }

    u32		audio_create_sound( const c8* filename )
    {
        u32 res = get_next_audio_resource( DEFER_RESOURCE );

        create_file_command( filename, CMD_AUDIO_CREATE_SOUND );

        return res;
    }

    u32     audio_create_channel_group()
    {
        u32 res = get_next_audio_resource( DEFER_RESOURCE );

        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_CREATE_GROUP;

        INC_WRAP( audio_put_pos );

        return res;
    }

    u32	    audio_create_channel_for_sound( const u32 sound_index )
    {
        u32 res = get_next_audio_resource( DEFER_RESOURCE );

        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_CREATE_CHANNEL_FOR_SOUND;
        audio_cmd_buffer[ audio_put_pos ].resource_index = sound_index;

        INC_WRAP( audio_put_pos );

        return res;
    }
    
    void	audio_channel_set_position( const u32 channel_index, const u32 position_ms )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_CHANNEL_SET_POSITION;
        audio_cmd_buffer[ audio_put_pos ].set_valuei.resource_index = channel_index;
        audio_cmd_buffer[ audio_put_pos ].set_valuei.value = position_ms;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_channel_set_frequency( const u32 channel_index, const f32 frequency )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_CHANNEL_SET_FREQUENCY;
        audio_cmd_buffer[ audio_put_pos ].set_valuef.resource_index = channel_index;
        audio_cmd_buffer[ audio_put_pos ].set_valuef.value = frequency;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_group_set_pause( const u32 group_index, const bool val )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_GROUP_SET_PAUSE;
        audio_cmd_buffer[ audio_put_pos ].set_valuei.resource_index = group_index;
        audio_cmd_buffer[ audio_put_pos ].set_valuei.value = (s32)val;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_group_set_mute( const u32 group_index, const bool val )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_GROUP_SET_MUTE;
        audio_cmd_buffer[ audio_put_pos ].set_valuei.resource_index = group_index;
        audio_cmd_buffer[ audio_put_pos ].set_valuei.value = (s32)val;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_group_set_pitch( const u32 group_index, const f32 pitch )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_GROUP_SET_PITCH;
        audio_cmd_buffer[ audio_put_pos ].set_valuef.resource_index = group_index;
        audio_cmd_buffer[ audio_put_pos ].set_valuef.value = pitch;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_group_set_volume( const u32 group_index, const f32 volume )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_GROUP_SET_VOLUME;
        audio_cmd_buffer[ audio_put_pos ].set_valuef.resource_index = group_index;
        audio_cmd_buffer[ audio_put_pos ].set_valuef.value = volume;
        
        INC_WRAP( audio_put_pos );
    }
    
    void	audio_add_channel_to_group( const u32 channel_index, const u32 group_index )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_ADD_CHANNEL_TO_GROUP;
        audio_cmd_buffer[ audio_put_pos ].set_valuei.resource_index = channel_index;
        audio_cmd_buffer[ audio_put_pos ].set_valuei.value = group_index;
        
        INC_WRAP( audio_put_pos );
    }
    
    void     audio_release_resource( u32 index )
    {
        audio_cmd_buffer[ audio_put_pos ].command_index = CMD_AUDIO_RELEASE_RESOURCE;
        audio_cmd_buffer[ audio_put_pos ].resource_index = index;
        
        INC_WRAP( audio_put_pos );
    }
}
