// audio.cpp
// Copyright 2014 - 2019 Alex Dixon.
// License: https://github.com/polymonster/pmtech/blob/master/license.md

#include "audio.h"

#include "console.h"
#include "data_struct.h"
#include "memory.h"
#include "slot_resource.h"

#include "fmod.hpp"

using namespace put;

namespace
{
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
        u32   num_dsp = 0;

        audio_resource_type type;

        std::atomic<u8> assigned_flag;
    };

    struct resource_state
    {
        union {
            audio_channel_state channel_state;
            audio_group_state   group_state;
            audio_fft_spectrum* fft_spectrum;
            audio_eq_state      eq_state;
            f32                 gain_value;
        };
    };

    FMOD::System*                              _sound_system;
    pen::res_pool<audio_resource_allocation>   _audio_resources;
    pen::multi_array_buffer<resource_state, 2> _resource_states;
    pen::res_pool<std::atomic<bool>>           _sound_file_info_ready;
    pen::res_pool<audio_sound_file_info>       _sound_file_info;
} // namespace

namespace put
{
    void direct::audio_system_initialise()
    {
        // init fmod
        FMOD_RESULT result;

        result = FMOD::System_Create(&_sound_system);

        static const u32 max_channels = 32;
        result = _sound_system->init(max_channels, FMOD_INIT_NORMAL, NULL);

        static u32 reserved = 128;

        _audio_resources.init(reserved);
        _sound_file_info_ready.init(reserved);
        _sound_file_info.init(reserved);
        _resource_states.init(reserved);

        PEN_ASSERT(result == FMOD_OK);
    }

    void direct::audio_system_shutdown()
    {
        for (s32 i = 0; i < _audio_resources._capacity; ++i)
            if (_audio_resources[i].assigned_flag)
                direct::audio_release_resource(i);

        _sound_system->release();
    }

    void update_channel_state(u32 resource_index)
    {
        _resource_states.grow(resource_index);

        audio_resource_allocation& res = _audio_resources[resource_index];

        resource_state& rs = _resource_states.backbuffer()[resource_index];

        audio_channel_state* state = &rs.channel_state;

        FMOD::Channel* channel = (FMOD::Channel*)res.resource;

        channel->getPosition(&state->position_ms, FMOD_TIMEUNIT_MS);
        channel->getPitch(&state->pitch);
        channel->getFrequency(&state->frequency);

        bool paused = false;
        channel->getPaused(&paused);

        bool playing = false;
        channel->isPlaying(&playing);

        if (!playing)
        {
            state->play_state = NOT_PLAYING;
        }
        else
        {
            state->play_state = PLAYING;

            if (paused)
            {
                state->play_state = PAUSED;
            }
        }
    }

    void update_group_state(u32 resource_index)
    {
        _resource_states.grow(resource_index);

        audio_resource_allocation& res = _audio_resources[resource_index];

        resource_state& rs = _resource_states.backbuffer()[resource_index];

        audio_group_state* state = &rs.group_state;

        FMOD::ChannelGroup* channel = (FMOD::ChannelGroup*)res.resource;

        channel->getPitch(&state->pitch);

        channel->getVolume(&state->volume);

        bool paused = false;
        channel->getPaused(&paused);

        bool playing = false;
        channel->isPlaying(&playing);

        if (!playing)
        {
            state->play_state = NOT_PLAYING;
        }
        else
        {
            state->play_state = PLAYING;

            if (paused)
            {
                state->play_state = PAUSED;
            }
        }
    }

    void update_fft(u32 resource_index)
    {
        _resource_states.grow(resource_index);

        audio_resource_allocation& res = _audio_resources[resource_index];

        resource_state& rs = _resource_states.backbuffer()[resource_index];

        audio_fft_spectrum** fft = &rs.fft_spectrum;

        FMOD::DSP* fft_dsp = (FMOD::DSP*)res.resource;

        FMOD_RESULT result = fft_dsp->getParameterData(FMOD_DSP_FFT_SPECTRUMDATA, (void**)fft, 0, 0, 0);

        PEN_ASSERT(result == FMOD_OK);
    }

    void update_three_band_eq(u32 resource_index)
    {
        _resource_states.grow(resource_index);

        FMOD::DSP* eq_dsp = (FMOD::DSP*)_audio_resources[resource_index].resource;

        resource_state& rs = _resource_states.backbuffer()[resource_index];

        eq_dsp->getParameterFloat(FMOD_DSP_THREE_EQ_LOWGAIN, &rs.eq_state.low, nullptr, 0);
        eq_dsp->getParameterFloat(FMOD_DSP_THREE_EQ_MIDGAIN, &rs.eq_state.med, nullptr, 0);
        eq_dsp->getParameterFloat(FMOD_DSP_THREE_EQ_HIGHGAIN, &rs.eq_state.high, nullptr, 0);
    }

    void update_gain(u32 resource_index)
    {
        _resource_states.grow(resource_index);

        resource_state& rs = _resource_states.backbuffer()[resource_index];

        FMOD::DSP* gain_dsp = (FMOD::DSP*)_audio_resources[resource_index].resource;

        gain_dsp->getParameterFloat(FMOD_DSP_CHANNELMIX_GAIN_CH0, &rs.gain_value, nullptr, 0);
    }

    void direct::audio_system_update()
    {
        _sound_system->update();

        for (s32 i = 0; i < _audio_resources._capacity; ++i)
        {
            if (_audio_resources[i].assigned_flag)
            {
                switch (_audio_resources[i].type)
                {
                    case AUDIO_RESOURCE_CHANNEL:
                    {
                        update_channel_state(i);
                    }
                    break;

                    case AUDIO_RESOURCE_GROUP:
                    {
                        update_group_state(i);
                    }
                    break;

                    case AUDIO_RESOURCE_DSP_FFT:
                    {
                        update_fft(i);
                    }
                    break;

                    case AUDIO_RESOURCE_DSP_EQ:
                    {
                        update_three_band_eq(i);
                    }
                    break;

                    case AUDIO_RESOURCE_DSP_GAIN:
                    {
                        update_gain(i);
                    }
                    break;

                    default:
                        break;
                }
            }
        }

        _resource_states.swap_buffers();
    }

    u32 direct::audio_create_sound(const c8* filename, u32 resource_slot)
    {
        _audio_resources.grow(resource_slot);
        _sound_file_info.grow(resource_slot);
        _sound_file_info_ready.grow(resource_slot);

        _audio_resources[resource_slot].assigned_flag |= 0xff;
        _audio_resources[resource_slot].type = AUDIO_RESOURCE_SOUND;

        FMOD_RESULT result = _sound_system->createSound(filename, FMOD_DEFAULT, NULL,
                                                        (FMOD::Sound**)&_audio_resources[resource_slot].resource);

        PEN_ASSERT(result == FMOD_OK);

        // populate sound info
        FMOD::Sound* new_sound = (FMOD::Sound*)_audio_resources[resource_slot].resource;

        new_sound->getLength(&_sound_file_info[resource_slot].length_ms, FMOD_TIMEUNIT_MS);

        _sound_file_info_ready[resource_slot] = true;

        return resource_slot;
    }

    u32 direct::audio_create_stream(const c8* filename, u32 resource_slot)
    {
        _audio_resources.grow(resource_slot);
        _sound_file_info.grow(resource_slot);
        _sound_file_info_ready.grow(resource_slot);

        _audio_resources[resource_slot].assigned_flag |= 0xff;
        _audio_resources[resource_slot].type = AUDIO_RESOURCE_SOUND;

        FMOD_RESULT result = _sound_system->createStream(filename, FMOD_LOOP_NORMAL | FMOD_2D, 0,
                                                         (FMOD::Sound**)&_audio_resources[resource_slot].resource);

        PEN_ASSERT(result == FMOD_OK);

        return resource_slot;
    }

    u32 direct::audio_create_channel_group(u32 resource_slot)
    {
        _audio_resources.grow(resource_slot);
        _sound_file_info.grow(resource_slot);
        _sound_file_info_ready.grow(resource_slot);

        _audio_resources[resource_slot].assigned_flag |= 0xff;
        _audio_resources[resource_slot].type = AUDIO_RESOURCE_GROUP;

        FMOD_RESULT result;

        result = _sound_system->createChannelGroup(NULL, (FMOD::ChannelGroup**)&_audio_resources[resource_slot].resource);

        PEN_ASSERT(result == FMOD_OK);

        return resource_slot;
    }

    u32 direct::audio_create_channel_for_sound(u32 sound_index, u32 resource_slot)
    {
        _audio_resources.grow(resource_slot);
        _sound_file_info.grow(resource_slot);
        _sound_file_info_ready.grow(resource_slot);

        _audio_resources[resource_slot].assigned_flag |= 0xff;
        _audio_resources[resource_slot].type = AUDIO_RESOURCE_CHANNEL;

        FMOD_RESULT result;

        result = _sound_system->playSound((FMOD::Sound*)_audio_resources[sound_index].resource, 0, false,
                                          (FMOD::Channel**)&_audio_resources[resource_slot].resource);

        PEN_ASSERT(result == FMOD_OK);

        return resource_slot;
    }

    void direct::audio_channel_set_position(const u32 channel_index, const u32 position_ms)
    {
        FMOD::Channel* p_chan = (FMOD::Channel*)_audio_resources[channel_index].resource;

        p_chan->setPosition(position_ms, FMOD_TIMEUNIT_MS);
    }

    void direct::audio_channel_set_frequency(const u32 channel_index, const f32 frequency)
    {
        FMOD::Channel* p_chan = (FMOD::Channel*)_audio_resources[channel_index].resource;

        p_chan->setFrequency(frequency);
    }

    void direct::audio_channel_stop(const u32 channel_index)
    {
        FMOD::Channel* p_chan = (FMOD::Channel*)_audio_resources[channel_index].resource;

        p_chan->stop();
    }

    void direct::audio_group_set_pause(const u32 group_index, const bool val)
    {
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)_audio_resources[group_index].resource;

        p_group->setPaused(val);
    }

    void direct::audio_group_set_mute(const u32 group_index, const bool val)
    {
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)_audio_resources[group_index].resource;

        p_group->setPaused(val);
    }

    void direct::audio_group_set_pitch(const u32 group_index, const f32 pitch)
    {
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)_audio_resources[group_index].resource;

        p_group->setPitch(pitch);
    }

    void direct::audio_group_set_volume(const u32 group_index, const f32 volume)
    {
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)_audio_resources[group_index].resource;

        p_group->setVolume(volume);
    }

    u32 direct::audio_release_resource(u32 index)
    {
        if (index == 0)
        {
            return 0;
        }

        if (_audio_resources[index].assigned_flag)
        {
            void* p_res = _audio_resources[index].resource;

            switch (_audio_resources[index].type)
            {
                case AUDIO_RESOURCE_CHANNEL:
                {
                    // channels are not releaseable
                }
                break;

                case AUDIO_RESOURCE_GROUP:
                {
                    ((FMOD::ChannelGroup*)p_res)->release();
                }
                break;

                case AUDIO_RESOURCE_DSP_FFT:
                case AUDIO_RESOURCE_DSP_EQ:
                case AUDIO_RESOURCE_DSP_GAIN:
                {
                    ((FMOD::DSP*)p_res)->release();
                }
                break;

                case AUDIO_RESOURCE_SOUND:
                {
                    ((FMOD::Sound*)p_res)->release();
                }
                break;

                default:
                    break;
            }
        }

        return 0;
    }

    void direct::audio_add_channel_to_group(const u32 channel_index, const u32 group_index)
    {
        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)_audio_resources[group_index].resource;
        FMOD::Channel*      p_chan = (FMOD::Channel*)_audio_resources[channel_index].resource;

        FMOD_RESULT result;

        result = p_chan->setChannelGroup(p_group);

        PEN_ASSERT(result == FMOD_OK);
    }

    FMOD_DSP_TYPE pen_dsp_to_fmod_type(dsp_type type, audio_resource_type& resource_type)
    {
        resource_type = AUDIO_RESOURCE_DSP;

        switch (type)
        {
            case DSP_FFT:
                resource_type = AUDIO_RESOURCE_DSP_FFT;
                return FMOD_DSP_TYPE_FFT;
            case DSP_THREE_BAND_EQ:
                resource_type = AUDIO_RESOURCE_DSP_EQ;
                return FMOD_DSP_TYPE_THREE_EQ;
            case DSP_GAIN:
                resource_type = AUDIO_RESOURCE_DSP_GAIN;
                return FMOD_DSP_TYPE_CHANNELMIX;
            default:
                PEN_ERROR;
        }

        return FMOD_DSP_TYPE_UNKNOWN;
    }

    u32 direct::audio_add_dsp_to_group(const u32 group_index, dsp_type type, u32 resource_slot)
    {
        _audio_resources.grow(resource_slot);

        _audio_resources[resource_slot].assigned_flag |= 0xff;

        audio_resource_type res_type;
        FMOD_DSP_TYPE       fmod_dsp = pen_dsp_to_fmod_type(type, res_type);

        FMOD_RESULT result;

        _audio_resources[resource_slot].type = res_type;

        FMOD::DSP** new_dsp = (FMOD::DSP**)&_audio_resources[resource_slot].resource;

        result = _sound_system->createDSPByType(fmod_dsp, new_dsp);

        PEN_ASSERT(result == FMOD_OK);

        FMOD::ChannelGroup* p_group = (FMOD::ChannelGroup*)_audio_resources[group_index].resource;

        p_group->addDSP(_audio_resources[group_index].num_dsp++, *new_dsp);

        return resource_slot;
    }

    void direct::audio_dsp_set_three_band_eq(const u32 eq_index, const f32 low, const f32 med, const f32 high)
    {
        FMOD::DSP* eq_dsp = (FMOD::DSP*)_audio_resources[eq_index].resource;

        eq_dsp->setParameterFloat(0, low);
        eq_dsp->setParameterFloat(1, med);
        eq_dsp->setParameterFloat(2, high);
    }

    void direct::audio_dsp_set_gain(const u32 dsp_index, const f32 gain)
    {
        FMOD::DSP* gain_dsp = (FMOD::DSP*)_audio_resources[dsp_index].resource;

        gain_dsp->setParameterFloat(FMOD_DSP_CHANNELMIX_GAIN_CH0, gain);
        gain_dsp->setParameterFloat(FMOD_DSP_CHANNELMIX_GAIN_CH1, gain);
    }

    pen_error audio_channel_get_state(const u32 channel_index, audio_channel_state* state)
    {
        if (_audio_resources[channel_index].assigned_flag)
        {
            if (_audio_resources[channel_index].type == AUDIO_RESOURCE_CHANNEL)
            {
                const resource_state& rs = _resource_states.frontbuffer()[channel_index];

                *state = rs.channel_state;

                return PEN_ERR_OK;
            }

            return PEN_ERR_FAILED;
        }

        return PEN_ERR_NOT_READY;
    }

    pen_error audio_channel_get_sound_file_info(const u32 sound_index, audio_sound_file_info* info)
    {
        if (_audio_resources[sound_index].assigned_flag && _sound_file_info_ready[sound_index])
        {
            if (_audio_resources[sound_index].type == AUDIO_RESOURCE_SOUND)
            {
                *info = _sound_file_info[sound_index];

                return PEN_ERR_OK;
            }

            return PEN_ERR_FAILED;
        }

        return PEN_ERR_NOT_READY;
    }

    pen_error audio_group_get_state(const u32 group_index, audio_group_state* state)
    {
        if (_audio_resources[group_index].assigned_flag)
        {
            if (_audio_resources[group_index].type == AUDIO_RESOURCE_GROUP)
            {
                const resource_state& rs = _resource_states.frontbuffer()[group_index];

                *state = rs.group_state;

                return PEN_ERR_OK;
            }

            return PEN_ERR_FAILED;
        }

        return PEN_ERR_NOT_READY;
    }

    pen_error audio_dsp_get_spectrum(const u32 spectrum_dsp, audio_fft_spectrum* spectrum)
    {
        if (_audio_resources[spectrum_dsp].assigned_flag)
        {
            if (_audio_resources[spectrum_dsp].type == AUDIO_RESOURCE_DSP_FFT)
            {
                const resource_state& rs = _resource_states.frontbuffer()[spectrum_dsp];

                if (rs.fft_spectrum != nullptr)
                {
                    *spectrum = *rs.fft_spectrum;
                }

                return PEN_ERR_OK;
            }

            return PEN_ERR_FAILED;
        }

        return PEN_ERR_NOT_READY;
    }

    pen_error audio_dsp_get_three_band_eq(const u32 eq_dsp, audio_eq_state* eq_state)
    {
        if (_audio_resources[eq_dsp].assigned_flag)
        {
            if (_audio_resources[eq_dsp].type == AUDIO_RESOURCE_DSP_EQ)
            {
                const resource_state& rs = _resource_states.frontbuffer()[eq_dsp];

                *eq_state = rs.eq_state;

                return PEN_ERR_OK;
            }

            return PEN_ERR_FAILED;
        }

        return PEN_ERR_NOT_READY;
    }

    pen_error audio_dsp_get_gain(const u32 dsp_index, f32* gain)
    {
        if (_audio_resources[dsp_index].assigned_flag)
        {
            if (_audio_resources[dsp_index].type == AUDIO_RESOURCE_DSP_GAIN)
            {
                const resource_state& rs = _resource_states.frontbuffer()[dsp_index];

                *gain = rs.gain_value;

                return PEN_ERR_OK;
            }

            return PEN_ERR_FAILED;
        }

        return PEN_ERR_NOT_READY;
    }
} // namespace put
