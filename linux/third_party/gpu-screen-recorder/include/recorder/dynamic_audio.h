#ifndef GSR_DYNAMIC_AUDIO_H
#define GSR_DYNAMIC_AUDIO_H
#include <stdint.h>
#include <libavutil/mathematics.h>
/* Reserve PC remainder and microphone in addition to application identities. */
#define GSR_MAX_DYNAMIC_AUDIO_APPS 64
#define GSR_MAX_DYNAMIC_AUDIO_TRACKS (GSR_MAX_DYNAMIC_AUDIO_APPS + 2)
/* All stems share the video origin, including applications first heard later. */
static inline int64_t gsr_dynamic_audio_origin(int64_t video_origin, AVRational video_time_base, AVRational audio_time_base) {
    return av_rescale_q(video_origin, video_time_base, audio_time_base);
}
#endif
