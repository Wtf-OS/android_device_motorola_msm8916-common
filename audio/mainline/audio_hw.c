/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "audio_hw_default"
//#define LOG_NDEBUG 0

#include <errno.h>
#include <malloc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <log/log.h>

#include <hardware/audio.h>
#include <hardware/hardware.h>
#include <stdatomic.h>
#include <system/audio.h>
#include <tinyalsa/asoundlib.h>

#define STUB_DEFAULT_SAMPLE_RATE   48000
#define STUB_DEFAULT_AUDIO_FORMAT  AUDIO_FORMAT_PCM_16_BIT

#define STUB_INPUT_BUFFER_MILLISECONDS  128 /* LPASS: 24576 bytes / stereo S16 */
#define STUB_INPUT_DEFAULT_CHANNEL_MASK AUDIO_CHANNEL_IN_STEREO

#define STUB_OUTPUT_BUFFER_MILLISECONDS  10
#define STUB_OUTPUT_DEFAULT_CHANNEL_MASK AUDIO_CHANNEL_OUT_STEREO

struct stub_audio_device {
    struct audio_hw_device device;
    atomic_bool mic_muted;
};

struct stub_stream_out {
    struct audio_stream_out stream;
    struct pcm *pcm;
    pthread_mutex_t lock;
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    size_t frame_count;
};

struct stub_stream_in {
    struct audio_stream_in stream;
    struct pcm *pcm;
    pthread_mutex_t lock;
    struct stub_audio_device *device;
    bool back_mic;
    int64_t frames_read;
    uint32_t sample_rate;
    audio_channel_mask_t channel_mask;
    audio_format_t format;
    size_t frame_count;
};

static int setup_speaker(void)
{
    struct mixer *mixer = mixer_open(0);
    struct mixer_ctl *volume, *route, *enable;
    int ret = -ENODEV;
    if (!mixer) return ret;
    volume = mixer_get_ctl_by_name(mixer, "RX3 Digital Volume");
    route = mixer_get_ctl_by_name(mixer, "RX3 MIX1 INP1");
    enable = mixer_get_ctl_by_name(mixer, "SPK DAC Switch");
    if (volume && route && enable &&
        !mixer_ctl_set_value(volume, 0, 76) &&
        !mixer_ctl_set_enum_by_string(route, "RX1") &&
        !mixer_ctl_set_value(enable, 0, 1)) ret = 0;
    mixer_close(mixer);
    return ret;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;

    ALOGV("out_get_sample_rate: %u", out->sample_rate);
    return out->sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;

    ALOGV("out_set_sample_rate: %d", rate);
    if (rate != 48000) return -EINVAL;
    out->sample_rate = rate;
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;
    size_t buffer_size = out->frame_count *
                         audio_stream_out_frame_size(&out->stream);

    ALOGV("out_get_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static audio_channel_mask_t out_get_channels(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;

    ALOGV("out_get_channels: %x", out->channel_mask);
    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    const struct stub_stream_out *out = (const struct stub_stream_out *)stream;

    ALOGV("out_get_format: %d", out->format);
    return out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;

    ALOGV("out_set_format: %d", format);
    if (format != AUDIO_FORMAT_PCM_16_BIT) return -EINVAL;
    out->format = format;
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;
    pthread_mutex_lock(&out->lock);
    if (out->pcm) { pcm_close(out->pcm); out->pcm = NULL; }
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    ALOGV("out_dump");
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    ALOGV("out_set_parameters");
    return 0;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    ALOGV("out_get_parameters");
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    ALOGV("out_get_latency");
    return 43;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    ALOGV("out_set_volume: Left:%f Right:%f", left, right);
    return 0;
}

static ssize_t out_write(struct audio_stream_out *stream, const void *buffer, size_t bytes)
{
    struct stub_stream_out *out = (struct stub_stream_out *)stream;
    int ret = 0;
    pthread_mutex_lock(&out->lock);
    if (!out->pcm) {
        struct pcm_config config = {
            .channels = 2, .rate = 48000, .period_size = 1024, .period_count = 2,
            .format = PCM_FORMAT_S16_LE,
        };
        out->pcm = pcm_open(0, 0, PCM_OUT, &config);
        if (!out->pcm || !pcm_is_ready(out->pcm)) {
            ALOGE("LPASS PCM unavailable: %s", out->pcm ? pcm_get_error(out->pcm) : "allocation failed");
            if (out->pcm) pcm_close(out->pcm);
            out->pcm = NULL;
            ret = -ENODEV;
        }
    }
    if (!ret && pcm_write(out->pcm, buffer, bytes)) {
        ALOGE("LPASS write failed: %s", pcm_get_error(out->pcm));
        pcm_close(out->pcm); out->pcm = NULL;
        ret = -EIO;
    }
    pthread_mutex_unlock(&out->lock);
    if (ret) usleep(bytes * 1000000LL / 4 / 48000);
    return ret ? ret : (ssize_t)bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    *dsp_frames = 0;
    ALOGV("out_get_render_position: dsp_frames: %p", dsp_frames);
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    ALOGV("out_add_audio_effect: %p", effect);
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    ALOGV("out_remove_audio_effect: %p", effect);
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    *timestamp = 0;
    ALOGV("out_get_next_write_timestamp: %ld", (long int)(*timestamp));
    return -EINVAL;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;

    ALOGV("in_get_sample_rate: %u", in->sample_rate);
    return in->sample_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;

    ALOGV("in_set_sample_rate: %u", rate);
    if (rate != 48000) return -EINVAL;
    in->sample_rate = rate;
    return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;
    size_t buffer_size = in->frame_count *
                         audio_stream_in_frame_size(&in->stream);

    ALOGV("in_get_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static audio_channel_mask_t in_get_channels(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;

    ALOGV("in_get_channels: %x", in->channel_mask);
    return in->channel_mask;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    const struct stub_stream_in *in = (const struct stub_stream_in *)stream;

    ALOGV("in_get_format: %d", in->format);
    return in->format;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;

    ALOGV("in_set_format: %d", format);
    if (format != AUDIO_FORMAT_PCM_16_BIT) return -EINVAL;
    in->format = format;
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;
    pthread_mutex_lock(&in->lock);
    if (in->pcm) { pcm_close(in->pcm); in->pcm = NULL; }
    pthread_mutex_unlock(&in->lock);
    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    return 0;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer, size_t bytes)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;
    int ret = 0;
    pthread_mutex_lock(&in->lock);
    if (!in->pcm) {
        struct mixer *mixer = mixer_open(0);
        struct mixer_ctl *route = mixer ? mixer_get_ctl_by_name(mixer, "DEC1 MUX") : NULL;
        if (!route || mixer_ctl_set_enum_by_string(route, in->back_mic ? "ADC3" : "ADC1"))
            ret = -ENODEV;
        struct mixer_ctl *second = mixer ? mixer_get_ctl_by_name(mixer, "DEC2 MUX") : NULL;
        if (!second || mixer_ctl_set_enum_by_string(second, in->back_mic ? "ADC1" : "ADC3"))
            ret = -ENODEV;
        const char *gains[] = { "ADC1 Volume", "ADC3 Volume" };
        for (unsigned i = 0; i < 2; ++i) {
            struct mixer_ctl *gain = mixer ? mixer_get_ctl_by_name(mixer, gains[i]) : NULL;
            if (!gain || mixer_ctl_set_value(gain, 0, 7)) ret = -ENODEV;
        }
        struct mixer_ctl *adc2 = mixer ? mixer_get_ctl_by_name(mixer, "ADC2 MUX") : NULL;
        if (!adc2 || mixer_ctl_set_enum_by_string(adc2, "INP3")) ret = -ENODEV;
        if (mixer) mixer_close(mixer);
        if (!ret) {
            struct pcm_config config = { .channels = 2, .rate = 48000,
                .period_size = 6144, .period_count = 2, .format = PCM_FORMAT_S16_LE };
            in->frames_read = 0;
            in->pcm = pcm_open(0, 1, PCM_IN | PCM_MONOTONIC, &config);
            if (!in->pcm || !pcm_is_ready(in->pcm)) {
                ALOGE("Microphone PCM unavailable: %s", in->pcm ? pcm_get_error(in->pcm) : "allocation failed");
                if (in->pcm) pcm_close(in->pcm);
                in->pcm = NULL;
                ret = -ENODEV;
            }
        }
    }
    if (!ret && pcm_read(in->pcm, buffer, bytes)) {
        ALOGE("Microphone read failed: %s", pcm_get_error(in->pcm));
        pcm_close(in->pcm); in->pcm = NULL;
        ret = -EIO;
    }
    if (!ret) in->frames_read += bytes / audio_stream_in_frame_size(stream);
    if (!ret && atomic_load(&in->device->mic_muted)) memset(buffer, 0, bytes);
    pthread_mutex_unlock(&in->lock);
    if (ret) usleep(bytes * 1000000LL / 4 / 48000);
    return ret ? ret : (ssize_t)bytes;
}

static int in_get_capture_position(const struct audio_stream_in *stream,
                                   int64_t *frames, int64_t *time)
{
    struct stub_stream_in *in = (struct stub_stream_in *)stream;
    unsigned avail;
    struct timespec stamp;
    pthread_mutex_lock(&in->lock);
    int ret = in->pcm ? pcm_get_htimestamp(in->pcm, &avail, &stamp) : -ENODEV;
    if (!ret) {
        *frames = in->frames_read + avail;
        *time = (int64_t)stamp.tv_sec * 1000000000LL + stamp.tv_nsec;
    }
    pthread_mutex_unlock(&in->lock);
    return ret;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static size_t samples_per_milliseconds(size_t milliseconds,
                                       uint32_t sample_rate,
                                       size_t channel_count)
{
    return milliseconds * sample_rate * channel_count / 1000;
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address __unused)
{
    ALOGV("adev_open_output_stream...");

    *stream_out = NULL;
    if ((config->sample_rate && config->sample_rate != 48000) ||
        (config->channel_mask && config->channel_mask != AUDIO_CHANNEL_OUT_STEREO) ||
        (config->format != AUDIO_FORMAT_DEFAULT && config->format != AUDIO_FORMAT_PCM_16_BIT)) {
        config->sample_rate = 48000;
        config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        config->format = AUDIO_FORMAT_PCM_16_BIT;
        return -EINVAL;
    }
    struct stub_stream_out *out =
            (struct stub_stream_out *)calloc(1, sizeof(struct stub_stream_out));
    if (!out)
        return -ENOMEM;

    int route_ret = setup_speaker();
    if (route_ret) { free(out); return route_ret; }
    pthread_mutex_init(&out->lock, NULL);
    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->sample_rate = config->sample_rate;
    if (out->sample_rate == 0)
        out->sample_rate = STUB_DEFAULT_SAMPLE_RATE;
    out->channel_mask = config->channel_mask;
    if (out->channel_mask == AUDIO_CHANNEL_NONE)
        out->channel_mask = STUB_OUTPUT_DEFAULT_CHANNEL_MASK;
    out->format = config->format;
    if (out->format == AUDIO_FORMAT_DEFAULT)
        out->format = STUB_DEFAULT_AUDIO_FORMAT;
    out->frame_count = samples_per_milliseconds(
                           STUB_OUTPUT_BUFFER_MILLISECONDS,
                           out->sample_rate, 1);

    ALOGV("adev_open_output_stream: sample_rate: %u, channels: %x, format: %d,"
          " frames: %zu", out->sample_rate, out->channel_mask, out->format,
          out->frame_count);
    *stream_out = &out->stream;
    return 0;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    ALOGV("adev_close_output_stream...");
    out_standby(&stream->common);
    pthread_mutex_destroy(&((struct stub_stream_out *)stream)->lock);
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    ALOGV("adev_set_parameters");
    return -ENOSYS;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    ALOGV("adev_get_parameters");
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    ALOGV("adev_init_check");
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    ALOGV("adev_set_voice_volume: %f", volume);
    return -ENOSYS;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    ALOGV("adev_set_master_volume: %f", volume);
    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev, float *volume)
{
    ALOGV("adev_get_master_volume: %f", *volume);
    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    ALOGV("adev_set_master_mute: %d", muted);
    return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    ALOGV("adev_get_master_mute: %d", *muted);
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    ALOGV("adev_set_mode: %d", mode);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    atomic_store(&((struct stub_audio_device *)dev)->mic_muted, state);
    return 0;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    *state = atomic_load(&((const struct stub_audio_device *)dev)->mic_muted);
    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    size_t buffer_size = samples_per_milliseconds(
                             STUB_INPUT_BUFFER_MILLISECONDS,
                             config->sample_rate,
                             audio_channel_count_from_in_mask(
                                 config->channel_mask));

    if (!audio_has_proportional_frames(config->format)) {
        // Since the audio data is not proportional choose an arbitrary size for
        // the buffer.
        buffer_size *= 4;
    } else {
        buffer_size *= audio_bytes_per_sample(config->format);
    }
    ALOGV("adev_get_input_buffer_size: %zu", buffer_size);
    return buffer_size;
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags __unused,
                                  const char *address __unused,
                                  audio_source_t source __unused)
{
    ALOGV("adev_open_input_stream...");

    *stream_in = NULL;
    if (devices != AUDIO_DEVICE_IN_BUILTIN_MIC && devices != AUDIO_DEVICE_IN_BACK_MIC)
        return -EINVAL;
    if ((config->sample_rate && config->sample_rate != 48000) ||
        (config->channel_mask && config->channel_mask != AUDIO_CHANNEL_IN_STEREO) ||
        (config->format != AUDIO_FORMAT_DEFAULT && config->format != AUDIO_FORMAT_PCM_16_BIT)) {
        config->sample_rate = 48000;
        config->channel_mask = AUDIO_CHANNEL_IN_STEREO;
        config->format = AUDIO_FORMAT_PCM_16_BIT;
        return -EINVAL;
    }
    struct stub_stream_in *in = (struct stub_stream_in *)calloc(1, sizeof(struct stub_stream_in));
    if (!in)
        return -ENOMEM;

    pthread_mutex_init(&in->lock, NULL);
    in->device = (struct stub_audio_device *)dev;
    in->back_mic = devices == AUDIO_DEVICE_IN_BACK_MIC;
    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;
    in->stream.get_capture_position = in_get_capture_position;
    in->sample_rate = config->sample_rate;
    if (in->sample_rate == 0)
        in->sample_rate = STUB_DEFAULT_SAMPLE_RATE;
    in->channel_mask = config->channel_mask;
    if (in->channel_mask == AUDIO_CHANNEL_NONE)
        in->channel_mask = STUB_INPUT_DEFAULT_CHANNEL_MASK;
    in->format = config->format;
    if (in->format == AUDIO_FORMAT_DEFAULT)
        in->format = STUB_DEFAULT_AUDIO_FORMAT;
    in->frame_count = samples_per_milliseconds(
                          STUB_INPUT_BUFFER_MILLISECONDS, in->sample_rate, 1);

    ALOGV("adev_open_input_stream: sample_rate: %u, channels: %x, format: %d,"
          "frames: %zu", in->sample_rate, in->channel_mask, in->format,
          in->frame_count);
    *stream_in = &in->stream;
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *in)
{
    in_standby(&in->common);
    pthread_mutex_destroy(&((struct stub_stream_in *)in)->lock);
    free(in);
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    ALOGV("adev_dump");
    return 0;
}

static int adev_close(hw_device_t *device)
{
    ALOGV("adev_close");
    free(device);
    return 0;
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    ALOGV("adev_open: %s", name);

    struct stub_audio_device *adev;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct stub_audio_device));
    if (!adev)
        return -ENOMEM;

    atomic_init(&adev->mic_muted, false);
    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.get_master_volume = adev_get_master_volume;
    adev->device.set_master_mute = adev_set_master_mute;
    adev->device.get_master_mute = adev_get_master_mute;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    *device = &adev->device.common;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Default audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
