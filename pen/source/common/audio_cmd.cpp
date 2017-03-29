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

        CMD_AUDIO_ADD_CHANNEL_TO_GROUP,

        CMD_AUDIO_CHANNEL_SET_PAUSE,
        CMD_AUDIO_CHANNEL_SET_PITCH,
        CMD_AUDIO_CHANNEL_SET_VOLUME,
        CMD_AUDIO_CHANNEL_SET_POSITION,
        CMD_AUDIO_CHANNEL_SET_FREQUENCY,
    };

    typedef struct  audio_cmd
    {
        u32		command_index;

        union
        {
            c8* filename;
            u32 resource_index;
        };

    } audio_cmd;

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
        case CMD_AUDIO_CREATE_CHANNEL_FOR_SOUND:
            direct::audio_create_channel_for_sound( cmd.resource_index );
            break;
        }
    }

    //thread sync
    pen::semaphore*			 p_audio_thread_consume_semaphore;
    pen::semaphore*			 p_audio_thread_continue_semaphore;

    void audio_init_thread_primitives()
    {
        //create thread sync primitives
        p_audio_thread_consume_semaphore = pen::threads_semaphore_create( 0, 1 );
        p_audio_thread_continue_semaphore = pen::threads_semaphore_create( 0, 1 );
    }

    void audio_wait_for_init()
    {
        pen::threads_semaphore_wait( p_audio_thread_continue_semaphore );
    }

    void audio_consume_command_buffer()
    {
        pen::threads_semaphore_signal( p_audio_thread_consume_semaphore, 1 );
        pen::threads_semaphore_wait( p_audio_thread_continue_semaphore );
    }

    PEN_THREAD_RETURN audio_thread_function( void* params )
    {
        direct::audio_system_initialise();

        //allow main thread to continue now we are initialised
        pen::threads_semaphore_signal( p_audio_thread_continue_semaphore, 1 );

        while( 1 )
        {
            if( pen::threads_semaphore_wait( p_audio_thread_consume_semaphore ) == 1 )
            {
                u32 end_pos = audio_put_pos;

                while( audio_get_pos != end_pos )
                {
                    audio_exec_command( audio_cmd_buffer[ audio_get_pos ] );

                    INC_WRAP( audio_get_pos );
                }

                direct::audio_system_update();
            }
        }
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



}