// audio_cmd.cpp
// Copyright 2014 - 2023 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "audio.h"

#include "data_struct.h"
#include "memory.h"
#include "pen_string.h"
#include "slot_resource.h"
#include "threads.h"

#include "fmod.hpp"

#include <math.h>

using namespace pen;
using namespace put;

namespace
{
    namespace e_cmd
    {
        enum cmd_t
        {
            create_stream,
            create_sound,
            create_sound_music,
            create_group,
            create_channel_for_sound,
            release_resource,
            add_channel_to_group,
            add_dsp_to_group,
            channel_set_position,
            channel_set_frequency,
            channel_stop,
            group_set_pause,
            group_set_mute,
            group_set_pitch,
            group_set_volume,
            dsp_set_three_band_eq,
            dsp_set_gain
        };
    }

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

    struct audio_cmd
    {
        u32 command_index;
        u32 resource_slot;

        union {
            c8*           filename;
            u32           resource_index;
            ::set_valuei  set_valuei;
            ::set_valuef  set_valuef;
            ::set_value3f set_value3f;
            music_file    music;
        };
    };

    pen::job*                   _audio_job_thread_info;
    pen::slot_resources         _audio_slot_resources;
    pen::ring_buffer<audio_cmd> _cmd_buffer;
    a_u8                        _shutdown = {0};
} // namespace

namespace put
{
    void audio_exec_command(const audio_cmd& cmd)
    {
        switch (cmd.command_index)
        {
            case e_cmd::create_stream:
                direct::audio_create_stream(cmd.filename, cmd.resource_slot);
                pen::memory_free(cmd.filename);
                break;
            case e_cmd::create_sound:
                direct::audio_create_sound(cmd.filename, cmd.resource_slot);
                pen::memory_free(cmd.filename);
                break;
            case e_cmd::create_sound_music:
                direct::audio_create_sound(cmd.music, cmd.resource_slot);
                break;
            case e_cmd::create_group:
                direct::audio_create_channel_group(cmd.resource_slot);
                break;
            case e_cmd::add_channel_to_group:
                direct::audio_add_channel_to_group(cmd.set_valuei.resource_index, cmd.set_valuei.value);
                break;
            case e_cmd::add_dsp_to_group:
                direct::audio_add_dsp_to_group(cmd.set_valuei.resource_index, (dsp_type)cmd.set_valuei.value,
                                               cmd.resource_slot);
                break;
            case e_cmd::create_channel_for_sound:
                direct::audio_create_channel_for_sound(cmd.resource_index, cmd.resource_slot);
                break;
            case e_cmd::channel_set_position:
                direct::audio_channel_set_position(cmd.set_valuei.resource_index, cmd.set_valuei.value);
                break;
            case e_cmd::channel_set_frequency:
                direct::audio_channel_set_frequency(cmd.resource_index, cmd.set_valuef.value);
                break;
            case e_cmd::channel_stop:
                direct::audio_channel_stop(cmd.resource_index);
                break;
            case e_cmd::group_set_mute:
                direct::audio_group_set_mute(cmd.set_valuei.resource_index, (bool)cmd.set_valuei.value);
                break;
            case e_cmd::group_set_pause:
                direct::audio_group_set_pause(cmd.set_valuei.resource_index, (bool)cmd.set_valuei.value);
                break;
            case e_cmd::group_set_volume:
                direct::audio_group_set_volume(cmd.set_valuef.resource_index, cmd.set_valuef.value);
                break;
            case e_cmd::dsp_set_gain:
                direct::audio_dsp_set_gain(cmd.set_valuef.resource_index, cmd.set_valuef.value);
                break;
            case e_cmd::group_set_pitch:
                direct::audio_group_set_pitch(cmd.set_valuef.resource_index, cmd.set_valuef.value);
                break;
            case e_cmd::release_resource:
                direct::audio_release_resource(cmd.resource_index);
                break;
            case e_cmd::dsp_set_three_band_eq:
                direct::audio_dsp_set_three_band_eq(cmd.set_value3f.resource_index, cmd.set_value3f.value[0],
                                                    cmd.set_value3f.value[1], cmd.set_value3f.value[2]);
                break;
        }
    }

    void audio_consume_command_buffer()
    {
        if (!_shutdown.load())
        {
            pen::semaphore_post(_audio_job_thread_info->p_sem_consume, 1);
        }
    }

    void* audio_thread_function(void* params)
    {
        job_thread_params* job_params = (job_thread_params*)params;
        _audio_job_thread_info = job_params->job_info;

        // create resource slots
        pen::slot_resources_init(&_audio_slot_resources, 128);
        _cmd_buffer.create(1024);

        direct::audio_system_initialise();

        // allow main thread to continue now we are initialised
        pen::semaphore_post(_audio_job_thread_info->p_sem_continue, 1);

        for (;;)
        {
            if (pen::semaphore_try_wait(_audio_job_thread_info->p_sem_consume))
            {
                pen::semaphore_post(_audio_job_thread_info->p_sem_continue, 1);

                audio_cmd* cmd = _cmd_buffer.get();
                while (cmd)
                {
                    audio_exec_command(*cmd);
                    cmd = _cmd_buffer.get();
                }

                direct::audio_system_update();
            }
            else
            {
                pen::thread_sleep_ms(1);
            }

            if (pen::semaphore_try_wait(_audio_job_thread_info->p_sem_exit))
            {
                _shutdown = 1;
                break;
            }
        }

        direct::audio_system_shutdown();

        pen::semaphore_post(_audio_job_thread_info->p_sem_continue, 1);
        pen::semaphore_post(_audio_job_thread_info->p_sem_terminated, 1);

        return PEN_THREAD_OK;
    }

    void create_file_command(const c8* filename, u32 command, u32 resource_slot)
    {
        audio_cmd ac;

        // allocate filename and copy the buffer and null terminate it
        u32 filename_length = pen::string_length(filename);
        ac.filename = (c8*)pen::memory_alloc(filename_length + 1);
        ac.filename[filename_length] = 0x00;
        ac.resource_slot = resource_slot;

        memcpy(ac.filename, filename, filename_length);

        // set command (create stream or sound)
        ac.command_index = command;

        _cmd_buffer.put(ac);
    }

    u32 audio_create_stream(const c8* filename)
    {
        u32 res = pen::slot_resources_get_next(&_audio_slot_resources);

        create_file_command(filename, e_cmd::create_stream, res);

        return res;
    }

    u32 audio_create_sound(const c8* filename)
    {
        u32 res = pen::slot_resources_get_next(&_audio_slot_resources);

        create_file_command(filename, e_cmd::create_sound, res);

        return res;
    }

    u32 audio_create_sound(const pen::music_file& music)
    {
        audio_cmd ac;

        u32 res = pen::slot_resources_get_next(&_audio_slot_resources);

        ac.command_index = e_cmd::create_sound_music;
        ac.music = music;
        ac.resource_slot = res;

        _cmd_buffer.put(ac);

        return res;
    }

    u32 audio_create_channel_group()
    {
        audio_cmd ac;

        u32 res = pen::slot_resources_get_next(&_audio_slot_resources);

        ac.command_index = e_cmd::create_group;
        ac.resource_slot = res;

        _cmd_buffer.put(ac);

        return res;
    }

    u32 audio_create_channel_for_sound(const u32 sound_index)
    {
        if (sound_index == 0)
        {
            return 0;
        }

        u32 res = pen::slot_resources_get_next(&_audio_slot_resources);

        audio_cmd ac;

        ac.command_index = e_cmd::create_channel_for_sound;
        ac.resource_index = sound_index;
        ac.resource_slot = res;

        _cmd_buffer.put(ac);

        return res;
    }

    void audio_channel_set_position(const u32 channel_index, const u32 position_ms)
    {
        audio_cmd ac;

        ac.command_index = e_cmd::channel_set_position;
        ac.set_valuei.resource_index = channel_index;
        ac.set_valuei.value = position_ms;

        _cmd_buffer.put(ac);
    }

    void audio_channel_set_frequency(const u32 channel_index, const f32 frequency)
    {
        audio_cmd ac;

        ac.command_index = e_cmd::channel_set_frequency;
        ac.set_valuef.resource_index = channel_index;
        ac.set_valuef.value = frequency;

        _cmd_buffer.put(ac);
    }

    void audio_group_set_pause(const u32 group_index, const bool val)
    {
        audio_cmd ac;

        ac.command_index = e_cmd::group_set_pause;
        ac.set_valuei.resource_index = group_index;
        ac.set_valuei.value = (s32)val;

        _cmd_buffer.put(ac);
    }

    void audio_group_set_mute(const u32 group_index, const bool val)
    {
        audio_cmd ac;

        ac.command_index = e_cmd::group_set_mute;
        ac.set_valuei.resource_index = group_index;
        ac.set_valuei.value = (s32)val;

        _cmd_buffer.put(ac);
    }

    void audio_group_set_pitch(const u32 group_index, const f32 pitch)
    {
        audio_cmd ac;

        ac.command_index = e_cmd::group_set_pitch;
        ac.set_valuef.resource_index = group_index;
        ac.set_valuef.value = pitch;

        _cmd_buffer.put(ac);
    }

    void audio_group_set_volume(const u32 group_index, const f32 volume)
    {
        audio_cmd ac;

        ac.command_index = e_cmd::group_set_volume;
        ac.set_valuef.resource_index = group_index;
        ac.set_valuef.value = volume;

        _cmd_buffer.put(ac);
    }

    void audio_add_channel_to_group(const u32 channel_index, const u32 group_index)
    {
        if (group_index == 0 || channel_index == 0)
        {
            return;
        }

        audio_cmd ac;

        ac.command_index = e_cmd::add_channel_to_group;
        ac.set_valuei.resource_index = channel_index;
        ac.set_valuei.value = group_index;

        _cmd_buffer.put(ac);
    }

    void audio_release_resource(u32 index)
    {
        if (!pen::slot_resources_free(&_audio_slot_resources, index))
            return;

        audio_cmd ac;

        ac.command_index = e_cmd::release_resource;
        ac.resource_index = index;

        _cmd_buffer.put(ac);
    }

    u32 audio_add_dsp_to_group(const u32 group_index, dsp_type type)
    {
        u32 res = pen::slot_resources_get_next(&_audio_slot_resources);

        audio_cmd ac;

        ac.command_index = e_cmd::add_dsp_to_group;
        ac.set_valuei.resource_index = group_index;
        ac.set_valuei.value = type;
        ac.resource_slot = res;

        _cmd_buffer.put(ac);

        return res;
    }

    void audio_dsp_set_three_band_eq(const u32 eq_index, const f32 low, const f32 med, const f32 high)
    {
        audio_cmd ac;

        ac.command_index = e_cmd::dsp_set_three_band_eq;
        ac.set_value3f.resource_index = eq_index;
        ac.set_value3f.value[0] = low;
        ac.set_value3f.value[1] = med;
        ac.set_value3f.value[2] = high;

        _cmd_buffer.put(ac);
    }

    void audio_dsp_set_gain(const u32 dsp_index, const f32 gain)
    {
        audio_cmd ac;

        ac.command_index = e_cmd::dsp_set_gain;
        ac.set_valuef.resource_index = dsp_index;
        ac.set_valuef.value = gain;

        _cmd_buffer.put(ac);
    }

    void audio_channel_stop(const u32 channel_index)
    {
        audio_cmd ac;

        ac.command_index = e_cmd::channel_stop;
        ac.resource_index = channel_index;

        _cmd_buffer.put(ac);
    }
} // namespace put
