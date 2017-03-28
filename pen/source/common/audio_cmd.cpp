#include "audio.h"
#include "fmod.hpp"
#include <math.h>

namespace pen
{
#define MAX_COMMANDS 10000
#define INC_WRAP( V ) V = (V+1)%MAX_COMMANDS

    enum commands : u32
    {
        CMD_AUDIO_CREATE_STREAM,
        CMD_AUDIO_CREATE_SOUND,

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
        };

    } audio_cmd;

    audio_cmd cmd_buffer[ MAX_COMMANDS ];
}