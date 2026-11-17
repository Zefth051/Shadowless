#ifndef GSR_RECORDER_AUDIO_GAIN_H
#define GSR_RECORDER_AUDIO_GAIN_H

#include <libavutil/frame.h>
#include <libavutil/samplefmt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

typedef struct {
    double system, microphone, game, music;
} gsr_audio_gains;

/* The first two values retain compatibility with older app/engine pairs. */
static inline bool gsr_audio_gain_read(const char *path, gsr_audio_gains *gains) {
    if(!path || !path[0]) return false;
    FILE *file = fopen(path, "r");
    if(!file) return false;
    gsr_audio_gains next = {1.0, 1.0, 1.0, 1.0};
    const int count = fscanf(file, "%lf %lf %lf %lf", &next.system, &next.microphone, &next.game, &next.music);
    const bool legacy = count == 2 && fgetc(file) == EOF;
    fclose(file);
    if((!legacy && count != 4) || !isfinite(next.system) || !isfinite(next.microphone)
            || !isfinite(next.game) || !isfinite(next.music)
            || next.system < 0.0 || next.system > 1.0 || next.microphone < 0.0 || next.microphone > 10.0
            || next.game < 0.0 || next.game > 1.0 || next.music < 0.0 || next.music > 1.0)
        return false;
    *gains = next;
    return true;
}

static inline double gsr_audio_role_gain(const char *track_name, bool four_tracks, bool microphone, const gsr_audio_gains *gains) {
    if(microphone) return gains->microphone;
    if(four_tracks && strcmp(track_name, "Game") == 0) return gains->game;
    if(four_tracks && strcmp(track_name, "Music") == 0) return gains->music;
    return gains->system;
}

/* Gain is applied to owned PCM before mixing, never to a system device or playback stream. */
static inline void gsr_audio_gain_apply(AVFrame *frame, double gain) {
    if(gain == 1.0) return;
#if LIBAVUTIL_VERSION_MAJOR < 58
    const int channels = frame->channels;
#else
    const int channels = frame->ch_layout.nb_channels;
#endif
    const bool planar = av_sample_fmt_is_planar(frame->format);
    const int planes = planar ? channels : 1;
    const int count = frame->nb_samples * (planar ? 1 : channels);
    for(int plane = 0; plane < planes; ++plane) {
        void *data = frame->extended_data[plane];
        switch(av_get_packed_sample_fmt(frame->format)) {
            case AV_SAMPLE_FMT_FLT:
                for(int i = 0; i < count; ++i) ((float*)data)[i] *= gain;
                break;
            case AV_SAMPLE_FMT_S16:
                for(int i = 0; i < count; ++i)
                    ((int16_t*)data)[i] = (int16_t)fmax(-32768.0, fmin(32767.0, ((int16_t*)data)[i] * gain));
                break;
            case AV_SAMPLE_FMT_S32:
                for(int i = 0; i < count; ++i)
                    ((int32_t*)data)[i] = (int32_t)fmax(-2147483648.0, fmin(2147483647.0, ((int32_t*)data)[i] * gain));
                break;
            default: break;
        }
    }
}
#endif
