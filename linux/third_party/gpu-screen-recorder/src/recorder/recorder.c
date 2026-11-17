#include "../../include/recorder/recorder.h"
#include "../../include/recorder/error.h"
#include "../../include/recorder/audio_codec.h"
#include "../../include/recorder/video_codec.h"
#include "../../include/recorder/codec_select.h"
#include "../../include/recorder/muxer.h"
#include "../../include/recorder/replay_save.h"
#include "../../include/recorder/audio_capture.h"
#include "../../include/recorder/recording_clock.h"
#include "../../include/recorder/screenshot.h"
#include "../../include/encoder/encoder.h"
#include "../../include/encoder/video/video.h"
#include "../../include/window/window.h"
#include "../../include/color_conversion.h"
#include "../../include/damage.h"
#include "../../include/cursor.h"
#include "../../include/plugins.h"
#include "../../include/utils.h"
#include "../../include/ffmpeg_utils.h"
#include "../../include/log.h"
#include "../../include/json.h"
#include "../../include/recorder/dynamic_audio.h"
#include "../../include/recorder/audio_spool.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <limits.h>
#include <unistd.h>
#include <stdatomic.h>

#include <libavutil/time.h>
#include <libavutil/mastering_display_metadata.h>
#include <libavformat/avformat.h>

#include <X11/Xlib.h>

#define GSR_VIDEO_STREAM_INDEX 0

#define GSR_SET_PAUSED_REQUEST_NONE -1
#define GSR_SET_PAUSED_REQUEST_UNPAUSE 0
#define GSR_SET_PAUSED_REQUEST_PAUSE 1

#define GSR_REPLAY_RECORDING_REQUEST_NONE 0
#define GSR_REPLAY_RECORDING_REQUEST_TOGGLE 1
#define GSR_REPLAY_RECORDING_REQUEST_START 2
#define GSR_REPLAY_RECORDING_REQUEST_STOP 3

typedef struct {
    pthread_t thread;
    atomic_int finished;
    bool success;
    char filepath[PATH_MAX], directory[64];
    char paths[GSR_MAX_DYNAMIC_AUDIO_TRACKS][PATH_MAX];
    char names[GSR_MAX_DYNAMIC_AUDIO_TRACKS][GSR_AUDIO_TRACK_NAME_MAX_SIZE];
    size_t count;
    int64_t origin_us;
} gsr_audio_finalizer;

struct gsr_recorder {
    gsr_recorder_settings settings;
    gsr_recorder_callbacks callbacks;
    gsr_windowing *windowing;
    gsr_egl *egl;
    gsr_window *window;
    gsr_capture_deps *capture_deps;
    gsr_capture_sources *capture_sources;
    gsr_audio_input_tracks *audio_input_tracks;

    char file_extension[32];
    bool force_no_audio_offset;
    double target_fps;
    bool uses_amix;
    bool audio_only;
    bool dynamic_audio;
    bool audio_started;
    double next_audio_discovery;
    struct recorder_audio_discovery *audio_discovery;
    char spool_directory[64];
    gsr_audio_finalizer *audio_finalizer;
    struct { char path[PATH_MAX]; AVFormatContext *format; size_t destination; gsr_audio_track *track; } spools[GSR_MAX_DYNAMIC_AUDIO_TRACKS];
    size_t num_spools;
    bool spooling;
    bool spool_failed;
    int64_t recording_origin_us;
    bool hdr;
    bool low_power;
    vec2i video_size;

    AVFormatContext *av_format_context;
    AVStream *video_stream;
    AVCodecContext *video_codec_context;
    AVFrame *video_frame;
    gsr_video_sources video_sources_data;
    gsr_video_sources *video_sources;
    gsr_encoder encoder;
    bool encoder_initialized;
    gsr_video_encoder *video_encoder;
    gsr_color_conversion color_conversion;
    bool color_conversion_initialized;
    gsr_color_conversion *output_color_conversion;
    gsr_plugins plugins;
    gsr_recording_clock *recording_clock;
    gsr_audio_capture audio_capture;
    bool audio_capture_initialized;
    gsr_replay_save replay_save;

    gsr_recording_output replay_recording_output;
    size_t replay_recording_items[GSR_MAX_RECORDING_DESTINATIONS];
    size_t num_replay_recording_items;
    char replay_recording_filepath[PATH_MAX];
    bool replay_recording;

    double fps_start_time;
    int fps_counter;
    int damage_fps_counter;
    bool paused;
    double record_start_time;
    int64_t video_pts_counter;
    int64_t video_prev_pts;
    bool hdr_metadata_set;

    atomic_int running;
    atomic_int toggle_pause;
    atomic_int set_paused_request;
    atomic_int replay_recording_request;
    atomic_int replay_recording_state;
    atomic_int save_replay_seconds;
    atomic_int save_replay_restart_replay;
    bool should_stop_error;
    bool force_iframe_frame;
    int audio_max_frame_size;
    bool use_damage_tracking;
    gsr_damage damage;
    const char **plugin_filepaths;
    int num_plugin_filepaths;
#ifdef GSR_APP_AUDIO
    gsr_pipewire_audio *pipewire_audio;
#endif
};

static void gsr_recorder_stop_recording(gsr_recorder *self);

static int recorder_setup_container(gsr_recorder *self) {
    // The output format is automatically guessed by the file extension
    avformat_alloc_output_context2(&self->av_format_context, NULL, self->settings.container_format, self->settings.filename);
    if (!self->av_format_context) {
        if(self->settings.container_format) {
            if(strcmp(self->settings.container_format, "whip") == 0)
                gsr_log(GSR_LOG_LEVEL_ERROR, "Container format 'whip' is not supported by your FFmpeg version. Whip streaming requires FFmpeg version 8.0 or later");
            else
                gsr_log(GSR_LOG_LEVEL_ERROR, "Container format '%s' (argument -c) is not valid", self->settings.container_format);
        } else {
            gsr_log(GSR_LOG_LEVEL_ERROR, "Failed to deduce container format from file extension. Use the '-c' option to specify container format");
            args_parser_print_usage();
            return GSR_ERROR_GENERIC;
        }
        return GSR_ERROR_GENERIC;
    }

    set_format_context_options(self->av_format_context);

    const AVOutputFormat *output_format = self->av_format_context->oformat;

    const char *file_extensions = output_format->extensions ? output_format->extensions : "";
    const char *file_extension_end = strchr(file_extensions, ',');
    if(file_extension_end)
        snprintf(self->file_extension, sizeof(self->file_extension), "%.*s", (int)(file_extension_end - file_extensions), file_extensions);
    else
        snprintf(self->file_extension, sizeof(self->file_extension), "%s", file_extensions);

    if(self->file_extension[0] == '\0')
        snprintf(self->file_extension, sizeof(self->file_extension), "%s", self->settings.container_format ? self->settings.container_format : "");

    if(!self->audio_only && strcmp(self->file_extension, "mp3") == 0) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "the mp3 container doesn't support video. Record without the -w option to record audio only");
        return GSR_ERROR_GENERIC;
    }

    self->force_no_audio_offset = self->settings.is_livestream || self->settings.is_output_piped || (strcmp(self->file_extension, "mp4") != 0 && strcmp(self->file_extension, "mkv") != 0 && strcmp(self->file_extension, "webm") != 0);
    self->target_fps = 1.0 / (double)self->settings.fps;

    self->uses_amix = gsr_audio_input_tracks_should_use_amix(self->audio_input_tracks);
    self->settings.audio_codec = select_audio_codec_with_fallback(self->settings.audio_codec, self->file_extension, self->uses_amix, self->settings.audio_codec_specified);
    if(!self->audio_only)
        self->settings.video_codec = video_codec_fallback_for_container(self->settings.video_codec, self->file_extension);

    // The whip muxer is network only so local recordings created while streaming with whip (-ro and replay) are saved as mkv instead,
    // see gsr_recording_output_start
    if(strcmp(self->file_extension, "whip") == 0)
        snprintf(self->file_extension, sizeof(self->file_extension), "mkv");

    return GSR_ERROR_OK;
}

static int recorder_setup_video_sources(gsr_recorder *self) {
    self->video_size = (vec2i){0, 0};
    if(self->audio_only)
        return GSR_ERROR_OK;

    const int video_sources_result = gsr_video_sources_create(&self->video_sources_data, &self->settings, self->egl, self->capture_deps, false, self->capture_sources, &self->video_size);
    if(video_sources_result != GSR_ERROR_OK) {
        return video_sources_result;
    }

    self->video_sources = &self->video_sources_data;

    // (Some?) livestreaming services require at least one audio track to work.
    // If not audio is provided then create one silent audio track.
    if(self->settings.is_livestream && self->audio_input_tracks->num_items == 0) {
        gsr_log(GSR_LOG_LEVEL_INFO, "live streaming but no audio track was added. Adding a silent audio track");
        gsr_merged_audio_inputs silent_audio_track;
        memset(&silent_audio_track, 0, sizeof(silent_audio_track));
        gsr_audio_input silent_audio_input;
        memset(&silent_audio_input, 0, sizeof(silent_audio_input));
        if(!gsr_merged_audio_inputs_add(&silent_audio_track, &silent_audio_input) || !gsr_audio_input_tracks_add(self->audio_input_tracks, &silent_audio_track)) {
            return GSR_ERROR_GENERIC;
        }
    }

    self->video_stream = NULL;

    return GSR_ERROR_OK;
}

static int recorder_setup_encoder(gsr_recorder *self) {
    const size_t estimated_replay_buffer_packets = calculate_estimated_replay_buffer_packets(self->settings.replay_buffer_size_secs, self->settings.fps, self->settings.audio_codec, self->audio_input_tracks);
    self->recording_clock = gsr_recording_clock_create();
    if(!self->recording_clock) {
        return GSR_ERROR_GENERIC;
    }

    self->encoder_initialized = true;
    if(!gsr_encoder_init(&self->encoder, self->settings.replay_storage, estimated_replay_buffer_packets, self->settings.replay_buffer_size_secs, self->settings.filename)) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "failed to create encoder");
        return GSR_ERROR_GENERIC;
    }

    return GSR_ERROR_OK;
}

static int recorder_setup_video_encoder(gsr_recorder *self) {
    if(self->audio_only)
        return GSR_ERROR_OK;

    if(self->settings.video_encoder == GSR_VIDEO_ENCODER_HW_CPU && self->settings.video_codec != (gsr_video_codec)GSR_VIDEO_CODEC_AUTO && self->settings.video_codec != GSR_VIDEO_CODEC_H264) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "-encoder cpu was specified but a codec other than h264 was specified. -encoder cpu supports only h264 at the moment");
        return GSR_ERROR_GENERIC;
    }

    self->low_power = false;
    const AVCodec *video_codec_f = NULL;
    const int select_video_codec_result = select_video_codec_with_fallback(self->video_size, &self->settings, self->file_extension, self->egl, &self->low_power, &video_codec_f);
    if(select_video_codec_result != GSR_ERROR_OK) {
        return select_video_codec_result;
    }

    const enum AVPixelFormat video_pix_fmt = get_pixel_format(self->settings.video_codec, self->egl->gpu_info.vendor, self->settings.pixel_format, self->settings.video_encoder == GSR_VIDEO_ENCODER_HW_CPU);
    self->video_codec_context = create_video_codec_context(video_pix_fmt, video_codec_f, self->egl, &self->settings, self->video_size.x, self->video_size.y);
    if(!self->settings.is_replaying)
        self->video_stream = create_stream(self->av_format_context, self->video_codec_context);

    self->video_frame = av_frame_alloc();
    if(!self->video_frame) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "Failed to allocate video frame");
        return GSR_ERROR_GENERIC;
    }
    self->video_frame->format = self->video_codec_context->pix_fmt;
    self->video_frame->width = self->video_size.x;
    self->video_frame->height = self->video_size.y;
    self->video_frame->color_range = self->video_codec_context->color_range;
    self->video_frame->color_primaries = self->video_codec_context->color_primaries;
    self->video_frame->color_trc = self->video_codec_context->color_trc;
    self->video_frame->colorspace = self->video_codec_context->colorspace;
    self->video_frame->chroma_location = self->video_codec_context->chroma_sample_location;

    self->video_encoder = create_video_encoder(self->egl, &self->settings);
    if(!self->video_encoder) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "failed to create video encoder");
        return GSR_ERROR_GENERIC;
    }

    if(!gsr_video_encoder_start(self->video_encoder, self->video_codec_context, self->video_frame)) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "failed to start video encoder");
        return GSR_ERROR_GENERIC;
    }

    self->video_size.x = self->video_codec_context->width;
    self->video_size.y = self->video_codec_context->height;
    gsr_video_sources_update_with_real_video_size(self->video_sources, self->video_size);

    memset(&self->plugins, 0, sizeof(self->plugins));

    if(gsr_load_plugins(&self->plugins, self->plugin_filepaths, self->num_plugin_filepaths, &self->settings, self->egl, self->video_size) != GSR_ERROR_OK) {
        return GSR_ERROR_GENERIC;

    }

    gsr_color_conversion_params color_conversion_params;
    memset(&color_conversion_params, 0, sizeof(color_conversion_params));
    color_conversion_params.color_range = self->settings.color_range;
    color_conversion_params.yuv_matrix = self->video_codec_context->colorspace == AVCOL_SPC_BT2020_NCL ? GSR_YUV_MATRIX_BT2020 : GSR_YUV_MATRIX_BT709;
    color_conversion_params.egl = self->egl;
    color_conversion_params.load_external_image_shader = gsr_video_sources_uses_external_image(self->video_sources);
    gsr_video_encoder_get_textures(self->video_encoder, color_conversion_params.destination_textures, color_conversion_params.destination_textures_size, &color_conversion_params.num_destination_textures, &color_conversion_params.destination_color);

    self->color_conversion_initialized = true;
    if(gsr_color_conversion_init(&self->color_conversion, &color_conversion_params) != 0) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "main: failed to create color conversion");
        return GSR_ERROR_GENERIC;
    }

    gsr_color_conversion_clear(&self->color_conversion);

    self->output_color_conversion = self->plugins.num_plugins > 0 ? &self->plugins.color_conversion : &self->color_conversion;

    if(self->settings.video_encoder == GSR_VIDEO_ENCODER_HW_CPU) {
        if(!open_video_software(self->video_codec_context, &self->settings)) {
            return GSR_ERROR_GENERIC;
        }
    } else {
        if(!open_video_hardware(self->video_codec_context, self->low_power, self->egl, &self->settings)) {
            return GSR_ERROR_GENERIC;
        }
    }

    if(self->video_stream) {
        avcodec_parameters_from_context(self->video_stream->codecpar, self->video_codec_context);
        const size_t video_destination_id = gsr_encoder_add_recording_destination(&self->encoder, self->video_codec_context, self->av_format_context, self->video_stream, GSR_RECORDING_FIRST_PTS_CAPTURE_START);
        if(self->settings.write_first_frame_ts && video_destination_id != (size_t)-1) {
            char ts_filepath[PATH_MAX + 4];
            snprintf(ts_filepath, sizeof(ts_filepath), "%s.ts", self->settings.filename);
            gsr_encoder_set_recording_destination_first_frame_ts_filepath(&self->encoder, video_destination_id, ts_filepath);
        }
    }

    return GSR_ERROR_OK;
}

static int recorder_setup_audio_track(gsr_recorder *self, const gsr_merged_audio_inputs *merged_audio_inputs, int audio_stream_index, gsr_audio_track *audio_track) {
    const bool use_amix = gsr_audio_inputs_should_use_amix(merged_audio_inputs);

    memset(audio_track, 0, sizeof(*audio_track));
    audio_track->stream_index = audio_stream_index;
    audio_track->codec_context = create_audio_codec_context(self->settings.fps, self->settings.audio_codec, use_amix, self->settings.audio_bitrate);
    if(!audio_track->codec_context)
        return GSR_ERROR_GENERIC;

    AVStream *audio_stream = NULL;
    if(!self->settings.is_replaying && !(self->dynamic_audio && gsr_audio_inputs_has_app_audio(merged_audio_inputs))) {
        audio_stream = create_stream(self->av_format_context, audio_track->codec_context);
        if(!audio_stream)
            return GSR_ERROR_GENERIC;

        if(gsr_encoder_add_recording_destination(&self->encoder, audio_track->codec_context, self->av_format_context, audio_stream, GSR_RECORDING_FIRST_PTS_CAPTURE_START) == (size_t)-1)
            gsr_log(GSR_LOG_LEVEL_ERROR, "added too many audio sources");
    }

    snprintf(audio_track->name, sizeof(audio_track->name), "%s", merged_audio_inputs->track_name);
    if(audio_stream && audio_track->name[0] != '\0' && !self->settings.exclude_metadata) {
        av_dict_set(&audio_stream->metadata, "title", audio_track->name, 0);
        av_dict_set(&audio_stream->metadata, "handler_name", audio_track->name, 0);
    }

    if(!open_audio(audio_track->codec_context, self->settings.ffmpeg_audio_opts))
        return GSR_ERROR_GENERIC;

    if(audio_stream)
        avcodec_parameters_from_context(audio_stream->codecpar, audio_track->codec_context);

    #if LIBAVCODEC_VERSION_MAJOR < 60
    const int num_channels = audio_track->codec_context->channels;
    #else
    const int num_channels = audio_track->codec_context->ch_layout.nb_channels;
    #endif

    const size_t num_capture_sources = gsr_audio_inputs_num_capture_sources(merged_audio_inputs);
    AVFilterContext *src_filter_ctx[GSR_MAX_AUDIO_SOURCES_PER_TRACK];
    if(use_amix) {
        if(num_capture_sources > GSR_MAX_AUDIO_SOURCES_PER_TRACK) {
            gsr_log(GSR_LOG_LEVEL_ERROR, "too many audio sources for one audio track, the maximum is %d", GSR_MAX_AUDIO_SOURCES_PER_TRACK);
            return GSR_ERROR_GENERIC;
        }

        if(gsr_audio_init_filter_graph(audio_track->codec_context, &audio_track->graph, &audio_track->sink, src_filter_ctx, num_capture_sources) < 0) {
            gsr_log(GSR_LOG_LEVEL_ERROR, "failed to create audio filter");
            return GSR_ERROR_GENERIC;
        }
    }

    const double audio_fps = (double)audio_track->codec_context->sample_rate / (double)audio_track->codec_context->frame_size;
    const double timeout_sec = 1000.0 / audio_fps / 1000.0;

    const double audio_startup_time_seconds = self->force_no_audio_offset ? 0 : audio_codec_get_desired_delay(self->settings.audio_codec, self->settings.fps);
    const double num_audio_frames_shift = audio_startup_time_seconds / timeout_sec;
    audio_track->pts = -audio_track->codec_context->frame_size * num_audio_frames_shift;

    if(gsr_audio_inputs_has_app_audio(merged_audio_inputs)) {
#ifdef GSR_APP_AUDIO
        return gsr_audio_track_init_application_input(audio_track, merged_audio_inputs, audio_track->codec_context, num_channels, num_audio_frames_shift, self->pipewire_audio, src_filter_ctx, use_amix);
#else
        return GSR_ERROR_UNSUPPORTED;
#endif
    }

    return gsr_audio_track_init_device_inputs(audio_track, merged_audio_inputs, audio_track->codec_context, num_channels, num_audio_frames_shift, src_filter_ctx, use_amix);
}

static bool recorder_start_audio_spool(gsr_recorder *self, gsr_audio_track *track) {
    if(!track->dynamic_source || !self->spooling) return true;
    if(self->num_spools >= GSR_MAX_DYNAMIC_AUDIO_TRACKS) return false;
    const size_t n = self->num_spools;
    snprintf(self->spools[n].path, sizeof(self->spools[n].path), "%s/%zu.mkv", self->spool_directory, n);
    AVFormatContext *format = NULL;
    avformat_alloc_output_context2(&format, NULL, "matroska", self->spools[n].path);
    if(!format) return false;
    format->avoid_negative_ts = AVFMT_AVOID_NEG_TS_DISABLED;
    AVStream *stream = create_stream(format, track->codec_context);
    if(!stream || avcodec_parameters_from_context(stream->codecpar, track->codec_context) < 0
            || avio_open(&format->pb, self->spools[n].path, AVIO_FLAG_WRITE) < 0
            || avformat_write_header(format, NULL) < 0) {
        if(format->pb) avio_closep(&format->pb);
        avformat_free_context(format);
        return false;
    }
    size_t destination = gsr_encoder_add_recording_destination(&self->encoder, track->codec_context,
        format, stream, GSR_RECORDING_FIRST_PTS_CAPTURE_START);
    if(destination == (size_t)-1) { avio_closep(&format->pb); avformat_free_context(format); return false; }
    self->spools[n].format = format;
    self->spools[n].destination = destination;
    self->spools[n].track = track;
    ++self->num_spools;
    return true;
}

static bool recorder_begin_audio_spools(gsr_recorder *self) {
    if(!self->dynamic_audio) return true;
    snprintf(self->spool_directory, sizeof(self->spool_directory), "/tmp/shadowless-audio-XXXXXX");
    if(!mkdtemp(self->spool_directory)) return false;
    self->spooling = true;
    self->spool_failed = false;
    self->num_spools = 0;
    for(size_t i = 0; i < self->audio_capture.num_tracks; ++i)
        if(!recorder_start_audio_spool(self, self->audio_capture.tracks[i])) { self->spool_failed = true; return false; }
    return true;
}

static void* recorder_finalize_audio_thread(void *userdata) {
    gsr_audio_finalizer *job = userdata;
    const char *paths[GSR_MAX_DYNAMIC_AUDIO_TRACKS], *names[GSR_MAX_DYNAMIC_AUDIO_TRACKS];
    for(size_t i = 0; i < job->count; ++i) { paths[i] = job->paths[i]; names[i] = job->names[i]; }
    job->success = gsr_audio_spools_finalize(job->filepath, paths, names, job->count, job->origin_us);
    if(job->success) {
        for(size_t i = 0; i < job->count; ++i) unlink(paths[i]);
        rmdir(job->directory);
    } else {
        gsr_log(GSR_LOG_LEVEL_ERROR, "dynamic audio finalization failed; retained recording '%s' and stems '%s'", job->filepath, job->directory);
    }
    atomic_store(&job->finished, 1);
    return NULL;
}

static bool recorder_poll_audio_finalizer(gsr_recorder *self, bool wait, bool notify) {
    gsr_audio_finalizer *job = self->audio_finalizer;
    if(!job || (!wait && !atomic_load(&job->finished))) return true;
    pthread_join(job->thread, NULL);
    const bool success = job->success;
    if(notify && self->callbacks.recording_stopped) {
        if(success) gsr_log(GSR_LOG_LEVEL_INFO, "Stopped recording");
        self->callbacks.recording_stopped(success ? job->filepath : NULL, self->callbacks.userdata);
    }
    free(job);
    self->audio_finalizer = NULL;
    return success;
}

static bool recorder_finish_audio_spools(gsr_recorder *self, const char *filepath) {
    if(!self->dynamic_audio || !self->spooling) return true;
    bool success = !self->spool_failed;
    gsr_audio_finalizer *job = calloc(1, sizeof(*job));
    if(!job) success = false;
    for(size_t i = 0; i < self->num_spools; ++i) {
        if(!gsr_encoder_remove_recording_destination(&self->encoder, self->spools[i].destination)) {
            self->spool_failed = true;
            success = false;
        }
        if(av_write_trailer(self->spools[i].format) < 0) success = false;
        if(avio_closep(&self->spools[i].format->pb) < 0) success = false;
        avformat_free_context(self->spools[i].format);
        self->spools[i].format = NULL;
        if(job) {
            snprintf(job->paths[i], sizeof(job->paths[i]), "%s", self->spools[i].path);
            snprintf(job->names[i], sizeof(job->names[i]), "%s", self->spools[i].track->name);
        }
    }
    self->spooling = false;
    if(job) {
        snprintf(job->filepath, sizeof(job->filepath), "%s", filepath);
        snprintf(job->directory, sizeof(job->directory), "%s", self->spool_directory);
        job->count = self->num_spools;
        job->origin_us = self->recording_origin_us;
        atomic_init(&job->finished, 0);
        if(success && pthread_create(&job->thread, NULL, recorder_finalize_audio_thread, job) != 0) success = false;
    }
    if(success) self->audio_finalizer = job;
    else {
        free(job);
        gsr_log(GSR_LOG_LEVEL_ERROR, "dynamic audio finalization failed; retained recording '%s' and stems '%s'", filepath, self->spool_directory);
    }
    self->num_spools = 0;
    return success;
}

#ifdef GSR_APP_AUDIO
static void recorder_update_app_choice(gsr_audio_track *track) {
    bool game = false, included = true;
    const char *initial_game = getenv("SHADOWLESS_GAME_AUDIO_APP");
    if(initial_game && strcasecmp(initial_game, track->application_name) == 0) game = true;
    const char *path = getenv("SHADOWLESS_AUDIO_APPS_FILE");
    FILE *file = path ? fopen(path, "rb") : NULL;
    if(file) {
        char data[65537];
        const size_t len = fread(data, 1, sizeof(data)-1, file);
        const bool complete = !ferror(file) && feof(file);
        fclose(file);
        if(complete) {
            data[len] = '\0';
            sj_Reader reader = sj_reader(data, len);
            sj_Value root = sj_read(&reader), key, value;
            char escaped[GSR_AUDIO_INPUT_NAME_MAX_SIZE * 6 + 1];
            gsr_json_escape_string(escaped, sizeof(escaped), track->application_name);
            while(root.type == SJ_OBJECT && sj_iter_object(&reader, root, &key, &value)) {
                if(!gsr_json_string_equals(&key, escaped) || value.type != SJ_OBJECT) continue;
                sj_Value property, setting;
                while(sj_iter_object(&reader, value, &property, &setting)) {
                    if(setting.type != SJ_BOOL) continue;
                    const bool enabled = setting.end-setting.start == 4 && memcmp(setting.start, "true", 4) == 0;
                    if(gsr_json_string_equals(&property, "game")) game = enabled;
                    if(gsr_json_string_equals(&property, "included")) included = enabled;
                }
                break;
            }
        }
    }
    atomic_store(&track->game, game);
    track->included = included;
    if(track->application_name[0])
        snprintf(track->name, sizeof(track->name), "%s%s: %s", game ? "Game" : "App", included ? "" : " (off)", track->application_name);
}

typedef struct { char names[GSR_MAX_DYNAMIC_AUDIO_APPS][GSR_AUDIO_INPUT_NAME_MAX_SIZE]; size_t count; bool overflow; } recorder_app_snapshot;
static bool recorder_copy_app(const char *name, void *userdata) {
    recorder_app_snapshot *snapshot = userdata;
    if(snapshot->count >= GSR_MAX_DYNAMIC_AUDIO_APPS || strlen(name) >= GSR_AUDIO_INPUT_NAME_MAX_SIZE) { snapshot->overflow = true; return false; }
    snprintf(snapshot->names[snapshot->count++], GSR_AUDIO_INPUT_NAME_MAX_SIZE, "%s", name);
    return true;
}

/* One unpublished track at a time. The worker never touches live tracks, the
   encoder, spools or filter_mutex, including while waiting on Pulse/PipeWire. */
typedef struct recorder_audio_discovery {
    pthread_t thread;
    atomic_bool finished;
    gsr_recorder *recorder;
    recorder_app_snapshot known;
    size_t num_tracks;
    gsr_audio_track track;
    bool prepared;
    int result;
} recorder_audio_discovery;

static int recorder_prepare_app(gsr_recorder *self, const char *name, int stream_index, gsr_audio_track *track) {
    gsr_audio_input input = {.type = GSR_AUDIO_INPUT_TYPE_APPLICATION};
    snprintf(input.name, sizeof(input.name), "%s", name);
    gsr_merged_audio_inputs merged = {.items = &input, .num_items = 1};
    snprintf(merged.track_name, sizeof(merged.track_name), "App: %s", input.name);
    const int result = recorder_setup_audio_track(self, &merged, stream_index, track);
    if(result != GSR_ERROR_OK) return result;
    track->dynamic_source = true;
    snprintf(track->application_name, sizeof(track->application_name), "%s", input.name);
    atomic_init(&track->game, false);
    recorder_update_app_choice(track);
    return GSR_ERROR_OK;
}

static int recorder_register_app(gsr_recorder *self, gsr_audio_track *track) {
    if(self->audio_started) {
        const double elapsed = gsr_recording_clock_get_time(self->recording_clock) - self->record_start_time;
        track->initial_frames = (int64_t)(fmax(0.0, elapsed) * track->codec_context->sample_rate / track->codec_context->frame_size);
        const int64_t shift = track->initial_frames * track->codec_context->frame_size;
        track->pts += shift;
        for(size_t j = 0; j < track->num_audio_devices; ++j) track->audio_devices[j].frame->pts += shift;
    }
    /* No PipeWire calls under this lock. Replay snapshots run on this same
       capture thread and therefore only observe fully registered tracks. */
    gsr_audio_capture_lock_filter(&self->audio_capture);
    const bool added = gsr_audio_capture_add_track(&self->audio_capture, track);
    gsr_audio_track *stored = added ? self->audio_capture.tracks[self->audio_capture.num_tracks-1] : NULL;
    gsr_audio_capture_unlock_filter(&self->audio_capture);
    if(!stored) return GSR_ERROR_GENERIC;
    memset(track, 0, sizeof(*track)); /* ownership transferred even if startup fails */
    if(!recorder_start_audio_spool(self, stored)) { self->spool_failed = true; return GSR_ERROR_GENERIC; }
    if(self->audio_started && gsr_audio_capture_start_track(&self->audio_capture, stored) != GSR_ERROR_OK) return GSR_ERROR_GENERIC;
    char escaped[GSR_AUDIO_INPUT_NAME_MAX_SIZE * 6 + 1];
    gsr_json_escape_string(escaped, sizeof(escaped), stored->application_name);
    gsr_log(GSR_LOG_LEVEL_INFO, "shadowless-audio-app: {\"name\":\"%s\",\"game\":%s}", escaped, atomic_load(&stored->game) ? "true" : "false");
    return GSR_ERROR_OK;
}

static int recorder_find_app(gsr_recorder *self, const recorder_app_snapshot *known, size_t num_tracks, gsr_audio_track *track, bool *prepared) {
    recorder_app_snapshot snapshot = {0};
    gsr_pipewire_audio_for_each_app(self->pipewire_audio, recorder_copy_app, &snapshot);
    if(snapshot.overflow) { gsr_log(GSR_LOG_LEVEL_ERROR, "dynamic application audio capacity exceeded"); return GSR_ERROR_GENERIC; }
    for(size_t n = 0; n < snapshot.count; ++n) {
        bool exists = false;
        for(size_t i = 0; i < known->count; ++i)
            if(strcasecmp(known->names[i], snapshot.names[n]) == 0) { exists = true; break; }
        if(exists) continue;
        if(known->count >= GSR_MAX_DYNAMIC_AUDIO_APPS || num_tracks >= GSR_MAX_DYNAMIC_AUDIO_TRACKS) {
            gsr_log(GSR_LOG_LEVEL_ERROR, "dynamic application audio reached the session limit of %d applications; capture stopped to avoid losing sources", GSR_MAX_DYNAMIC_AUDIO_APPS);
            return GSR_ERROR_GENERIC;
        }
        const int result = recorder_prepare_app(self, snapshot.names[n], num_tracks + 1, track);
        *prepared = result == GSR_ERROR_OK;
        return result;
    }
    return GSR_ERROR_OK;
}

static void* recorder_discover_audio_thread(void *userdata) {
    recorder_audio_discovery *job = userdata;
    job->result = recorder_find_app(job->recorder, &job->known, job->num_tracks, &job->track, &job->prepared);
    atomic_store(&job->finished, true);
    return NULL;
}

static void recorder_join_audio_discovery(gsr_recorder *self) {
    recorder_audio_discovery *job = self->audio_discovery;
    if(!job) return;
    pthread_join(job->thread, NULL);
    gsr_audio_track_deinit(&job->track);
    free(job);
    self->audio_discovery = NULL;
}

static int recorder_discover_audio(gsr_recorder *self) {
    if(!self->dynamic_audio) return GSR_ERROR_OK;
    if(self->audio_discovery) {
        recorder_audio_discovery *job = self->audio_discovery;
        if(!atomic_load(&job->finished)) return GSR_ERROR_OK;
        pthread_join(job->thread, NULL);
        int result = job->result;
        if(result == GSR_ERROR_OK && job->prepared) result = recorder_register_app(self, &job->track);
        gsr_audio_track_deinit(&job->track);
        free(job);
        self->audio_discovery = NULL;
        if(result != GSR_ERROR_OK) return result;
    }
    recorder_app_snapshot known = {0};
    for(size_t i = 0; i < self->audio_capture.num_tracks; ++i) {
        gsr_audio_track *track = self->audio_capture.tracks[i];
        if(track->application_name[0]) {
            recorder_copy_app(track->application_name, &known);
            recorder_update_app_choice(track);
        }
    }
    if(self->audio_started) {
        recorder_audio_discovery *job = calloc(1, sizeof(*job));
        if(!job) return GSR_ERROR_GENERIC;
        job->recorder = self;
        job->known = known;
        job->num_tracks = self->audio_capture.num_tracks;
        atomic_init(&job->finished, false);
        if(pthread_create(&job->thread, NULL, recorder_discover_audio_thread, job) != 0) { free(job); return GSR_ERROR_GENERIC; }
        self->audio_discovery = job;
        return GSR_ERROR_OK;
    }
    /* Initial devices are opened before capture begins. */
    for(;;) {
        gsr_audio_track track = {0};
        bool prepared = false;
        int result = recorder_find_app(self, &known, self->audio_capture.num_tracks, &track, &prepared);
        if(result == GSR_ERROR_OK && prepared) result = recorder_register_app(self, &track);
        gsr_audio_track_deinit(&track);
        if(result != GSR_ERROR_OK || !prepared) return result;
        recorder_copy_app(self->audio_capture.tracks[self->audio_capture.num_tracks-1]->application_name, &known);
    }
}
#endif

static int recorder_setup_audio(gsr_recorder *self) {
    if(gsr_audio_capture_init(&self->audio_capture, &self->encoder, self->recording_clock, &self->running) != GSR_ERROR_OK)
        return GSR_ERROR_GENERIC;

    int audio_stream_index = self->audio_only ? 0 : GSR_VIDEO_STREAM_INDEX + 1;
    for(size_t i = 0; i < self->audio_input_tracks->num_items; ++i) {
        gsr_audio_track audio_track;
        gsr_merged_audio_inputs merged = self->audio_input_tracks->items[i];
        gsr_audio_input remainder = {.type = GSR_AUDIO_INPUT_TYPE_APPLICATION};
        if(self->dynamic_audio && strcmp(merged.track_name, "Microphone") != 0) {
            snprintf(remainder.name, sizeof(remainder.name), "__shadowless_unassigned__");
            merged.items = &remainder; merged.num_items = 1;
            snprintf(merged.track_name, sizeof(merged.track_name), "PC (other)");
        }
        const int audio_track_result = recorder_setup_audio_track(self, &merged, audio_stream_index, &audio_track);
        audio_track.dynamic_source = self->dynamic_audio && strcmp(merged.track_name, "PC (other)") == 0;
        atomic_init(&audio_track.game, false);
        if(audio_track_result != GSR_ERROR_OK) {
            gsr_audio_track_deinit(&audio_track);
            return audio_track_result;
        }

        if(!gsr_audio_capture_add_track(&self->audio_capture, &audio_track)) {
            gsr_audio_track_deinit(&audio_track);
            return GSR_ERROR_GENERIC;
        }

        ++audio_stream_index;

        if(audio_track.codec_context->frame_size > self->audio_max_frame_size)
            self->audio_max_frame_size = audio_track.codec_context->frame_size;
    }

#ifdef GSR_APP_AUDIO
    if(self->dynamic_audio && recorder_discover_audio(self) != GSR_ERROR_OK) return GSR_ERROR_GENERIC;
#endif
    if(self->dynamic_audio && !self->settings.is_replaying && !recorder_begin_audio_spools(self)) return GSR_ERROR_GENERIC;

    return GSR_ERROR_OK;
}

static int recorder_open_output(gsr_recorder *self) {
    if(!self->settings.is_replaying && !(self->av_format_context->oformat->flags & AVFMT_NOFILE)) {
        const int ret = avio_open(&self->av_format_context->pb, self->settings.filename, AVIO_FLAG_WRITE);
        if(ret < 0) {
            gsr_log(GSR_LOG_LEVEL_ERROR, "Could not open '%s': %s", self->settings.filename, gsr_av_error_to_string(ret));
            return GSR_ERROR_GENERIC;
        }
    }

    if(!self->settings.is_replaying) {
        if(!av_write_header(self->av_format_context, &self->settings))
            return GSR_ERROR_GENERIC;
    }

    return GSR_ERROR_OK;
}

gsr_recorder* gsr_recorder_create(const gsr_recorder_params *params, const gsr_recorder_callbacks *callbacks, int *error) {
    *error = GSR_ERROR_GENERIC;

    gsr_recorder *self = calloc(1, sizeof(gsr_recorder));
    if(!self) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "gsr_recorder_create: failed to allocate recorder");
        return NULL;
    }

    self->settings = *params->settings;
    if(callbacks)
        self->callbacks = *callbacks;
    self->windowing = params->windowing;
    self->egl = &params->windowing->egl;
    self->window = params->windowing->window;
    self->capture_deps = params->capture_deps;
    self->capture_sources = params->capture_sources;
    self->audio_input_tracks = params->audio_input_tracks;
    self->audio_only = params->capture_sources->num_items == 0;
    self->dynamic_audio = getenv("SHADOWLESS_DYNAMIC_AUDIO") && strcmp(getenv("SHADOWLESS_DYNAMIC_AUDIO"), "1") == 0;
    atomic_init(&self->running, 1);
    atomic_init(&self->toggle_pause, 0);
    atomic_init(&self->set_paused_request, GSR_SET_PAUSED_REQUEST_NONE);
    atomic_init(&self->replay_recording_request, GSR_REPLAY_RECORDING_REQUEST_NONE);
    atomic_init(&self->replay_recording_state, 0);
    atomic_init(&self->save_replay_seconds, 0);
    atomic_init(&self->save_replay_restart_replay, GSR_RESTART_REPLAY_USE_OPTION);
    self->audio_max_frame_size = 1024;
    int error_code = GSR_ERROR_GENERIC;
    self->hdr = video_codec_is_hdr(params->settings->video_codec);
    self->plugin_filepaths = params->plugin_filepaths;
    self->num_plugin_filepaths = params->num_plugin_filepaths;
#ifdef GSR_APP_AUDIO
    self->pipewire_audio = params->pipewire_audio;
#endif

    const struct {
        int (*setup)(gsr_recorder *self);
    } setup_phases[] = {
        { recorder_setup_container },
        { recorder_setup_video_sources },
        { recorder_setup_encoder },
        { recorder_setup_video_encoder },
        { recorder_setup_audio },
        { recorder_open_output },
    };

    for(size_t i = 0; i < sizeof(setup_phases)/sizeof(setup_phases[0]); ++i) {
        error_code = setup_phases[i].setup(self);
        if(error_code != GSR_ERROR_OK)
            goto fail;
    }

    *error = GSR_ERROR_OK;
    return self;

    fail:
    *error = error_code;
    gsr_recorder_destroy(self, false);
    return NULL;
}

static void recorder_process_events(gsr_recorder *self) {
    while(gsr_window_process_event(self->window)) {
        if(self->capture_deps->x11_cursor_display && self->settings.record_cursor)
            gsr_cursor_on_event(&self->capture_deps->x11_cursor, gsr_window_get_event_data(self->window));

        gsr_damage_on_event(&self->damage, gsr_window_get_event_data(self->window));
        for(size_t video_source_index = 0; video_source_index < self->video_sources->num_items; ++video_source_index) {
            gsr_video_source *video_source = &self->video_sources->items[video_source_index];
            gsr_capture_on_event(video_source->capture, self->egl);
        }
    }

    if(self->capture_deps->x11_cursor_display && self->settings.record_cursor)
        gsr_cursor_tick(&self->capture_deps->x11_cursor, DefaultRootWindow(self->capture_deps->x11_cursor_display));
}

static bool recorder_tick_video_sources(gsr_recorder *self) {
    gsr_damage_tick(&self->damage);

    self->should_stop_error = false;
    bool damaged = false;

    if(self->use_damage_tracking)
        damaged = gsr_damage_is_damaged(&self->damage);

    for(size_t video_source_index = 0; video_source_index < self->video_sources->num_items; ++video_source_index) {
        gsr_video_source *video_source = &self->video_sources->items[video_source_index];
        gsr_capture_tick(video_source->capture);

        if(gsr_capture_should_stop(video_source->capture, &self->should_stop_error)) {
            atomic_store(&self->running, 0);
            break;
        }

        if(video_source->capture_source->type == GSR_CAPTURE_SOURCE_TYPE_FOCUSED_WINDOW) {
            assert(video_source->capture->get_window_id);
            const Window damage_target_window = video_source->capture->get_window_id(video_source->capture);

            if((int64_t)damage_target_window != video_source->capture_source->window_id) {
                gsr_damage_stop_tracking_window(&self->damage, video_source->capture_source->window_id);
                if(damage_target_window != 0)
                    gsr_damage_start_tracking_window(&self->damage, damage_target_window);
            }

            video_source->capture_source->window_id = damage_target_window;
        }

        if(video_source->capture->is_damaged)
            damaged |= video_source->capture->is_damaged(video_source->capture);
        else if(!self->use_damage_tracking)
            damaged = true;
    }

    damaged |= gsr_plugins_is_damaged(&self->plugins);

    // TODO: Readd wayland sync warning when removing this
    if(self->settings.framerate_mode != GSR_FRAMERATE_MODE_CONTENT)
        damaged = true;

    if(damaged)
        ++self->damage_fps_counter;

    return damaged;
}

static void recorder_update_fps_counters(gsr_recorder *self) {
    ++self->fps_counter;
    const double time_now = clock_get_monotonic_seconds();
    //const double frame_timer_elapsed = time_now - frame_timer_start;
    const double elapsed = time_now - self->fps_start_time;
    if (elapsed >= 1.0) {
        if(self->settings.verbose) {
            gsr_log(GSR_LOG_LEVEL_INFO, "update fps: %d, damage fps: %d", self->fps_counter, self->damage_fps_counter);
        }
        self->fps_start_time = time_now;
        self->fps_counter = 0;
        self->damage_fps_counter = 0;
    }
}

static bool frame_side_data_equals(const AVFrame *frame, enum AVFrameSideDataType type, const void *data, size_t size) {
    const AVFrameSideData *side_data = av_frame_get_side_data(frame, type);
    return side_data && side_data->size >= size && memcmp(side_data->data, data, size) == 0;
}

static void recorder_update_frame_hdr_metadata(gsr_recorder *self) {
    AVMasteringDisplayMetadata mastering_display_metadata;
    AVContentLightMetadata light_metadata;
    memset(&mastering_display_metadata, 0, sizeof(mastering_display_metadata));
    memset(&light_metadata, 0, sizeof(light_metadata));

    bool found = false;
    for(size_t video_source_index = 0; video_source_index < self->video_sources->num_items && !found; ++video_source_index) {
        found = gsr_capture_get_frame_hdr_metadata(self->video_sources->items[video_source_index].capture, &mastering_display_metadata, &light_metadata);
    }

    if(!found)
        return;

    if(frame_side_data_equals(self->video_frame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA, &mastering_display_metadata, sizeof(mastering_display_metadata)) &&
        frame_side_data_equals(self->video_frame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL, &light_metadata, sizeof(light_metadata)))
    {
        return;
    }

    av_frame_remove_side_data(self->video_frame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
    av_frame_remove_side_data(self->video_frame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);

    AVMasteringDisplayMetadata *frame_mastering_display_metadata = av_mastering_display_metadata_create_side_data(self->video_frame);
    if(frame_mastering_display_metadata)
        *frame_mastering_display_metadata = mastering_display_metadata;

    AVContentLightMetadata *frame_light_metadata = av_content_light_metadata_create_side_data(self->video_frame);
    if(frame_light_metadata)
        *frame_light_metadata = light_metadata;

    /* Some encoders (such as vaapi) only write the hdr metadata of the frame on keyframes */
    self->force_iframe_frame = true;
}

static void recorder_capture_and_encode_frame(gsr_recorder *self, bool damaged) {
    const double this_video_frame_time = gsr_recording_clock_get_time(self->recording_clock);
    const int64_t expected_frames = floor((this_video_frame_time - self->record_start_time) / self->target_fps);
    const int64_t num_missed_frames = expected_frames - self->video_pts_counter;

    if(damaged && num_missed_frames >= 1 && !self->paused) {
        gsr_video_encoder_prepare_frame(self->video_encoder, self->video_frame, &self->color_conversion);

        gsr_damage_clear(&self->damage);
        gsr_plugins_clear_damage(&self->plugins);
        gsr_capture_deps_cleanup_kms_fds(self->capture_deps);

        gsr_capture_deps_update_kms(self->capture_deps);

        bool capture_has_synchronous_task = false;
        for(size_t video_source_index = 0; video_source_index < self->video_sources->num_items; ++video_source_index) {
            gsr_video_source *video_source = &self->video_sources->items[video_source_index];
            if(video_source->capture->clear_damage)
                video_source->capture->clear_damage(video_source->capture);

            if(video_source->capture->capture_has_synchronous_task) {
                capture_has_synchronous_task = video_source->capture->capture_has_synchronous_task(video_source->capture);
                if(capture_has_synchronous_task) {
                    self->paused = true;
                    gsr_recording_clock_set_paused(self->recording_clock, true);
                }
            }
        }

        for(size_t video_source_index = 0; video_source_index < self->video_sources->num_items; ++video_source_index) {
            gsr_video_source *video_source = &self->video_sources->items[video_source_index];
            if(video_source->capture->pre_capture)
                video_source->capture->pre_capture(video_source->capture, &video_source->metadata, self->output_color_conversion);
        }

        if(self->output_color_conversion->schedule_clear) {
            self->output_color_conversion->schedule_clear = false;
            gsr_color_conversion_clear(self->output_color_conversion);
        }

        for(size_t video_source_index = 0; video_source_index < self->video_sources->num_items; ++video_source_index) {
            gsr_video_source *video_source = &self->video_sources->items[video_source_index];
            gsr_capture_capture(video_source->capture, &video_source->metadata, self->output_color_conversion);
        }

        gsr_capture_deps_cleanup_kms_fds(self->capture_deps);

        if(self->plugins.num_plugins > 0) {
            gsr_plugins_draw(&self->plugins);
            gsr_color_conversion_draw(&self->color_conversion, self->plugins.texture,
            (vec2i){0, 0}, self->video_size,
            (vec2i){0, 0}, self->video_size,
            self->video_size, GSR_ROT_0, GSR_FLIP_NONE, GSR_SOURCE_COLOR_RGB, false);
        }

        if(capture_has_synchronous_task) {
            self->paused = false;
            gsr_recording_clock_set_paused(self->recording_clock, false);
        }

        gsr_egl_swap_buffers(self->egl);
        gsr_video_encoder_copy_textures_to_frame(self->video_encoder, self->video_frame, self->output_color_conversion);

        for(size_t video_source_index = 0; video_source_index < self->video_sources->num_items; ++video_source_index) {
            gsr_video_source *video_source = &self->video_sources->items[video_source_index];
            if(self->hdr && !self->hdr_metadata_set && !self->settings.is_replaying && add_hdr_metadata_to_video_stream(video_source->capture, self->video_stream))
                self->hdr_metadata_set = true;
        }

        if(self->hdr)
            recorder_update_frame_hdr_metadata(self);

        // TODO: Check if duplicate frame can be saved just by writing it with a different pts instead of sending it again
        const int num_frames_to_encode = self->settings.framerate_mode == GSR_FRAMERATE_MODE_CONSTANT ? num_missed_frames : 1;
        for(int i = 0; i < num_frames_to_encode; ++i) {
            if(self->settings.framerate_mode == GSR_FRAMERATE_MODE_CONSTANT) {
                self->video_frame->pts = self->video_pts_counter + i;
            } else {
                self->video_frame->pts = (this_video_frame_time - self->record_start_time) * (double)AV_TIME_BASE;
                const bool same_pts = self->video_frame->pts == self->video_prev_pts;
                self->video_prev_pts = self->video_frame->pts;
                if(same_pts)
                    continue;
            }

            if(self->force_iframe_frame) {
                self->video_frame->pict_type = AV_PICTURE_TYPE_I;
            }

            int ret = avcodec_send_frame(self->video_codec_context, self->video_frame);
            if(ret == 0) {
                // TODO: Move to separate thread because this could write to network (for example when livestreaming)
                gsr_encoder_receive_packets(&self->encoder, self->video_codec_context, self->video_frame->pts, GSR_VIDEO_STREAM_INDEX);
            } else {
                gsr_log(GSR_LOG_LEVEL_ERROR, "avcodec_send_frame failed, error: %s", gsr_av_error_to_string(ret));
            }

            if(self->force_iframe_frame) {
                self->force_iframe_frame = false;
                self->video_frame->pict_type = AV_PICTURE_TYPE_NONE;
            }
        }

        self->video_pts_counter += num_missed_frames;
    }
}

static void recorder_apply_pause_toggle(gsr_recorder *self) {
    const bool toggle_pause = atomic_exchange(&self->toggle_pause, 0) == 1;
    const int set_paused_request = atomic_exchange(&self->set_paused_request, GSR_SET_PAUSED_REQUEST_NONE);
    if(self->settings.is_replaying)
        return;

    bool new_paused = self->paused;
    if(toggle_pause)
        new_paused = !new_paused;
    if(set_paused_request != GSR_SET_PAUSED_REQUEST_NONE)
        new_paused = set_paused_request == GSR_SET_PAUSED_REQUEST_PAUSE;

    if(new_paused != self->paused) {
        self->paused = new_paused;
        gsr_recording_clock_set_paused(self->recording_clock, self->paused);
        gsr_log(GSR_LOG_LEVEL_INFO, self->paused ? "Paused" : "Unpaused");
    }
}

static void recorder_snapshot_recording_origin(gsr_recorder *self) {
    pthread_mutex_lock(&self->encoder.file_write_mutex);
    for(size_t i = 0; i < self->encoder.num_recording_destinations; ++i) {
        const gsr_encoder_recording_destination *destination = &self->encoder.recording_destinations[i];
        if(destination->format_context == self->replay_recording_output.av_format_context
                && destination->codec_context == self->video_codec_context && destination->first_pts >= 0)
            self->recording_origin_us = av_rescale_q(destination->first_pts, self->video_codec_context->time_base, AV_TIME_BASE_Q);
    }
    pthread_mutex_unlock(&self->encoder.file_write_mutex);
}

static void recorder_apply_replay_recording_toggle(gsr_recorder *self) {
    const int request = atomic_exchange(&self->replay_recording_request, GSR_REPLAY_RECORDING_REQUEST_NONE);
    if(request == GSR_REPLAY_RECORDING_REQUEST_NONE)
        return;

    if(!self->settings.replay_recording_directory) {
        if(request != GSR_REPLAY_RECORDING_REQUEST_STOP && self->callbacks.recording_started)
            self->callbacks.recording_started(NULL, self->callbacks.userdata);
        return;
    }

    bool new_replay_recording_state = !self->replay_recording;
    if(request == GSR_REPLAY_RECORDING_REQUEST_START)
        new_replay_recording_state = true;
    else if(request == GSR_REPLAY_RECORDING_REQUEST_STOP)
        new_replay_recording_state = false;

    if(new_replay_recording_state == self->replay_recording)
        return;

    if(new_replay_recording_state && self->audio_finalizer) {
        if(self->callbacks.recording_started) self->callbacks.recording_started(NULL, self->callbacks.userdata);
        return;
    }
    if(new_replay_recording_state) {
        gsr_audio_capture_lock_filter(&self->audio_capture);
        self->num_replay_recording_items = 0;
        const bool filepath_created = gsr_create_new_recording_filepath_from_timestamp(self->replay_recording_filepath, sizeof(self->replay_recording_filepath), self->settings.replay_recording_directory, "Video", self->file_extension, self->settings.date_folders);
        gsr_audio_capture recording_audio = {0};
        gsr_audio_track *fixed_tracks[GSR_MAX_DYNAMIC_AUDIO_TRACKS];
        for(size_t i = 0; self->dynamic_audio && i < self->audio_capture.num_tracks; ++i) {
            if(!self->audio_capture.tracks[i]->dynamic_source)
                fixed_tracks[recording_audio.num_tracks++] = self->audio_capture.tracks[i];
        }
        recording_audio.tracks = self->dynamic_audio ? fixed_tracks : self->audio_capture.tracks;
        if(!self->dynamic_audio) recording_audio.num_tracks = self->audio_capture.num_tracks;
        self->recording_origin_us = 0;
        if(filepath_created && recorder_begin_audio_spools(self) && gsr_recording_output_start(&self->replay_recording_output, self->replay_recording_filepath, &self->settings, self->video_codec_context, &recording_audio, self->hdr, self->video_sources)) {
            const size_t video_recording_destination_id = gsr_encoder_add_recording_destination(&self->encoder, self->video_codec_context, self->replay_recording_output.av_format_context, self->replay_recording_output.video_stream, GSR_RECORDING_FIRST_PTS_NEXT_KEYFRAME);
            if(self->settings.write_first_frame_ts && video_recording_destination_id != (size_t)-1) {
                char ts_filepath[PATH_MAX + 4];
                snprintf(ts_filepath, sizeof(ts_filepath), "%s.ts", self->replay_recording_filepath);
                gsr_encoder_set_recording_destination_first_frame_ts_filepath(&self->encoder, video_recording_destination_id, ts_filepath);
            }

            if(video_recording_destination_id != (size_t)-1 && self->num_replay_recording_items < GSR_MAX_RECORDING_DESTINATIONS) {
                self->replay_recording_items[self->num_replay_recording_items] = video_recording_destination_id;
                ++self->num_replay_recording_items;
            }

            for(size_t i = 0; i < self->replay_recording_output.num_audio_streams; ++i) {
                const gsr_recording_audio_stream *audio_stream = &self->replay_recording_output.audio_streams[i];
                const size_t audio_recording_destination_id = gsr_encoder_add_recording_destination(&self->encoder, audio_stream->audio_track->codec_context, self->replay_recording_output.av_format_context, audio_stream->stream, GSR_RECORDING_FIRST_PTS_NEXT_KEYFRAME);
                if(audio_recording_destination_id != (size_t)-1 && self->num_replay_recording_items < GSR_MAX_RECORDING_DESTINATIONS) {
                    self->replay_recording_items[self->num_replay_recording_items] = audio_recording_destination_id;
                    ++self->num_replay_recording_items;
                }
            }

            self->replay_recording = true;
            atomic_store(&self->replay_recording_state, 1);
            self->force_iframe_frame = true;
            gsr_log(GSR_LOG_LEVEL_INFO, "Started recording");
            if(self->callbacks.recording_started)
                self->callbacks.recording_started(self->replay_recording_filepath, self->callbacks.userdata);
        } else {
            if(self->spooling) {
                self->spool_failed = true;
                recorder_finish_audio_spools(self, self->replay_recording_filepath);
            }
            if(self->callbacks.recording_started)
                self->callbacks.recording_started(NULL, self->callbacks.userdata);
        }
        gsr_audio_capture_unlock_filter(&self->audio_capture);
    } else if(self->replay_recording_output.av_format_context) {
        recorder_snapshot_recording_origin(self);
        for(size_t i = 0; i < self->num_replay_recording_items; ++i) {
            if(!gsr_encoder_remove_recording_destination(&self->encoder, self->replay_recording_items[i])) self->spool_failed = true;
        }
        self->num_replay_recording_items = 0;

        const bool base_saved = gsr_recording_output_stop(&self->replay_recording_output);
        if(!base_saved) self->spool_failed = true;
        const bool stems_saved = recorder_finish_audio_spools(self, self->replay_recording_filepath);
        if(base_saved && stems_saved && !self->audio_finalizer) {
            gsr_log(GSR_LOG_LEVEL_INFO, "Stopped recording");
            if(self->callbacks.recording_stopped)
                self->callbacks.recording_stopped(self->replay_recording_filepath, self->callbacks.userdata);
        } else if(!base_saved || !stems_saved) {
            if(self->callbacks.recording_stopped)
                self->callbacks.recording_stopped(NULL, self->callbacks.userdata);
        }

        self->replay_recording = false;
        atomic_store(&self->replay_recording_state, 0);
        self->replay_recording_filepath[0] = '\0';
    }
}

static void recorder_poll_replay_save(gsr_recorder *self) {
    bool replay_save_result = false;
    const char *replay_save_output_filepath = NULL;
    if(gsr_replay_save_poll(&self->replay_save, &replay_save_result, &replay_save_output_filepath)) {
        if(self->callbacks.replay_saved)
            self->callbacks.replay_saved(replay_save_output_filepath[0] == '\0' || !replay_save_result ? NULL : replay_save_output_filepath, self->callbacks.userdata);
    }

    if(atomic_load(&self->save_replay_seconds) != 0 && !gsr_replay_save_is_running(&self->replay_save) && self->settings.is_replaying) {
        int current_save_replay_seconds = atomic_load(&self->save_replay_seconds);
        if(current_save_replay_seconds > 0)
            current_save_replay_seconds += self->settings.keyint;

        atomic_store(&self->save_replay_seconds, 0);
        const int restart_replay_request = atomic_exchange(&self->save_replay_restart_replay, GSR_RESTART_REPLAY_USE_OPTION);
        const bool restart_replay = restart_replay_request == GSR_RESTART_REPLAY_USE_OPTION ? self->settings.restart_replay_on_save : restart_replay_request == GSR_RESTART_REPLAY_ENABLE;
        const bool replay_start_result = gsr_replay_save_start(&self->replay_save, self->video_codec_context, GSR_VIDEO_STREAM_INDEX, &self->audio_capture, &self->encoder, &self->settings, self->file_extension, self->hdr, self->video_sources, current_save_replay_seconds);
        if(!replay_start_result && self->callbacks.replay_saved)
            self->callbacks.replay_saved(NULL, self->callbacks.userdata);

        if(restart_replay && current_save_replay_seconds == GSR_SAVE_REPLAY_SECONDS_FULL) {
            pthread_mutex_lock(&self->encoder.replay_mutex);
            gsr_replay_buffer_clear(self->encoder.replay_buffer);
            pthread_mutex_unlock(&self->encoder.replay_mutex);
        }
    }
}

static void recorder_tick_video(gsr_recorder *self) {
    if(self->audio_only)
        return;

    recorder_process_events(self);
    const bool damaged = recorder_tick_video_sources(self);
    recorder_update_fps_counters(self);
    recorder_capture_and_encode_frame(self, damaged);
}

static void recorder_sleep_until_next_frame(gsr_recorder *self) {
    if(self->audio_only) {
        av_usleep(20.0 * 1000.0); // 20 milliseconds
        return;
    }

    const double time_at_frame_end = gsr_recording_clock_get_time(self->recording_clock);
    const double time_elapsed_total = time_at_frame_end - self->record_start_time;
    const int64_t frames_elapsed = floor(time_elapsed_total / self->target_fps);
    const double time_at_next_frame = (frames_elapsed + 1) * self->target_fps;
    double time_to_next_frame = time_at_next_frame - time_elapsed_total;
    if(time_to_next_frame > self->target_fps)
        time_to_next_frame = self->target_fps;
    const int64_t end_num_missed_frames = frames_elapsed - self->video_pts_counter;

    if(time_to_next_frame > 0.0 && end_num_missed_frames <= 0)
        av_usleep(time_to_next_frame * 1000.0 * 1000.0);
    else {
        if(self->paused)
            av_usleep(20.0 * 1000.0); // 20 milliseconds
        else if(self->settings.framerate_mode == GSR_FRAMERATE_MODE_CONTENT)
            av_usleep(2.8 * 1000.0); // 2.8 milliseconds
    }
}

int gsr_recorder_run(gsr_recorder *self) {
    self->fps_start_time = clock_get_monotonic_seconds();
    //double frame_timer_start = self->fps_start_time;
    self->fps_counter = 0;
    self->damage_fps_counter = 0;

    self->paused = false;
    self->replay_recording = false;

    memset(&self->replay_recording_output, 0, sizeof(self->replay_recording_output));

    gsr_replay_save_init(&self->replay_save);

    self->force_iframe_frame = false;

    gsr_recording_clock_start(self->recording_clock);
    self->record_start_time = gsr_recording_clock_get_start_time(self->recording_clock);

    if(gsr_audio_capture_start(&self->audio_capture, self->audio_max_frame_size, self->uses_amix) != GSR_ERROR_OK) {
        /* The audio threads that did start have to stop before they can be joined */
        atomic_store(&self->running, 0);
        return GSR_ERROR_GENERIC;
    }

    self->audio_started = true;

    // Set update_fps to 24 to test if duplicate/delayed frames cause video/audio desync or too fast/slow video.
    //const double update_fps = fps + 190;
    self->should_stop_error = false;

    self->video_pts_counter = 0;
    self->video_prev_pts = 0;

    self->hdr_metadata_set = false;
    self->hdr = video_codec_is_hdr(self->settings.video_codec);

    memset(&self->damage, 0, sizeof(self->damage));
    if(self->settings.framerate_mode == GSR_FRAMERATE_MODE_CONTENT && gsr_capture_sources_has_damage_tracked_target(self->capture_sources)) {
        if(gsr_window_get_display_server(self->window) == GSR_DISPLAY_SERVER_X11) {
            gsr_damage_init(&self->damage, self->egl, &self->capture_deps->x11_cursor, self->settings.record_cursor);
            self->use_damage_tracking = true;

            for(size_t i = 0; i < self->capture_sources->num_items; ++i) {
                const gsr_capture_source *capture_source = &self->capture_sources->items[i];
                switch(capture_source->type) {
                    case GSR_CAPTURE_SOURCE_TYPE_WINDOW:
                        gsr_damage_start_tracking_window(&self->damage, capture_source->window_id);
                        break;
                    case GSR_CAPTURE_SOURCE_TYPE_MONITOR:
                    case GSR_CAPTURE_SOURCE_TYPE_REGION:
                        // TODO: When capturing a region only track damage in that region
                        gsr_damage_start_tracking_monitor(&self->damage, capture_source->name);
                        break;
                    default:
                        break;
                }
            }
        } else if(gsr_capture_sources_has_monitor_or_region(self->capture_sources)) {
            gsr_log(GSR_LOG_LEVEL_WARNING, "\"-fm content\" has no effect on Wayland when recording a monitor. Either record a monitor on X11 or capture with desktop portal instead (-w portal)");
        }
    }

    while(atomic_load(&self->running)) {
#ifdef GSR_APP_AUDIO
        if(self->dynamic_audio && clock_get_monotonic_seconds() >= self->next_audio_discovery) {
            self->next_audio_discovery = clock_get_monotonic_seconds() + 0.10;
            if(recorder_discover_audio(self) != GSR_ERROR_OK) { self->should_stop_error = true; break; }
        }
#endif
        recorder_tick_video(self);
        recorder_apply_pause_toggle(self);
        recorder_poll_audio_finalizer(self, false, true);
        recorder_apply_replay_recording_toggle(self);
        recorder_poll_replay_save(self);
        recorder_sleep_until_next_frame(self);
    }

    gsr_recorder_stop_recording(self);
    return self->should_stop_error ? GSR_ERROR_CAPTURE_FAILED : GSR_ERROR_OK;
}

static void gsr_recorder_flush_video(gsr_recorder *self) {
    if(!self->video_codec_context || !self->video_frame)
        return;

    if(avcodec_send_frame(self->video_codec_context, NULL) >= 0)
        gsr_encoder_receive_packets(&self->encoder, self->video_codec_context, self->video_frame->pts, GSR_VIDEO_STREAM_INDEX);
}

static void gsr_recorder_stop_recording(gsr_recorder *self) {
    atomic_store(&self->running, 0);
#ifdef GSR_APP_AUDIO
    recorder_join_audio_discovery(self);
#endif

    bool final_replay_save_result = false;
    const char *final_replay_save_output_filepath = NULL;
    if(gsr_replay_save_join(&self->replay_save, &final_replay_save_result, &final_replay_save_output_filepath)) {
        if(self->callbacks.replay_saved)
            self->callbacks.replay_saved(final_replay_save_result && final_replay_save_output_filepath[0] ? final_replay_save_output_filepath : NULL, self->callbacks.userdata);
    }

    gsr_plugins_deinit(&self->plugins);

    gsr_recorder_flush_video(self);

    gsr_audio_capture_join_threads(&self->audio_capture);
    gsr_audio_capture_flush(&self->audio_capture);

    if(self->replay_recording_output.av_format_context) {
        recorder_snapshot_recording_origin(self);
        for(size_t i = 0; i < self->num_replay_recording_items; ++i) {
            if(!gsr_encoder_remove_recording_destination(&self->encoder, self->replay_recording_items[i])) self->spool_failed = true;
        }
        self->num_replay_recording_items = 0;

        const bool base_saved = gsr_recording_output_stop(&self->replay_recording_output);
        if(!base_saved) self->spool_failed = true;
        const bool stems_saved = recorder_finish_audio_spools(self, self->replay_recording_filepath);
        if(base_saved && stems_saved && !self->audio_finalizer) {
            gsr_log(GSR_LOG_LEVEL_INFO, "Stopped recording");
            if(self->callbacks.recording_stopped)
                self->callbacks.recording_stopped(self->replay_recording_filepath, self->callbacks.userdata);
        } else if(!base_saved || !stems_saved) {
            if(self->callbacks.recording_stopped)
                self->callbacks.recording_stopped(NULL, self->callbacks.userdata);
        }
    }

    recorder_poll_audio_finalizer(self, true, true);

    if(!self->settings.is_replaying) {
        if(self->dynamic_audio) {
            pthread_mutex_lock(&self->encoder.file_write_mutex);
            for(size_t i = 0; i < self->encoder.num_recording_destinations; ++i) {
                const gsr_encoder_recording_destination *destination = &self->encoder.recording_destinations[i];
                if(destination->format_context == self->av_format_context && destination->write_failed)
                    self->spool_failed = true;
            }
            pthread_mutex_unlock(&self->encoder.file_write_mutex);
        }
        bool saved = gsr_av_format_context_write_trailer(self->av_format_context) == 0;
        if(self->dynamic_audio) {
            if(avio_closep(&self->av_format_context->pb) < 0) saved = false;
            if(!saved) self->spool_failed = true;
            saved = recorder_finish_audio_spools(self, self->settings.filename) && saved;
            saved = recorder_poll_audio_finalizer(self, true, false) && saved;
        }
        if(self->callbacks.recording_stopped)
            self->callbacks.recording_stopped(saved ? self->settings.filename : NULL, self->callbacks.userdata);
    }
}

void gsr_recorder_destroy(gsr_recorder *self, bool exiting) {
    if(!self)
        return;

#ifdef GSR_APP_AUDIO
    recorder_join_audio_discovery(self);
#endif
    if(self->spooling) {
        self->spool_failed = true;
        recorder_finish_audio_spools(self, self->settings.filename);
    }
    recorder_poll_audio_finalizer(self, true, false);
    gsr_audio_capture_deinit(&self->audio_capture);
    gsr_plugins_deinit(&self->plugins);

    if(self->use_damage_tracking)
        gsr_damage_deinit(&self->damage);

    if(self->color_conversion_initialized)
        gsr_color_conversion_deinit(&self->color_conversion);

    if(self->video_frame)
        av_frame_free(&self->video_frame);

    if(self->video_codec_context)
        avcodec_free_context(&self->video_codec_context);

    if(self->video_encoder)
        gsr_video_encoder_destroy(self->video_encoder, NULL);

    if(self->encoder_initialized)
        gsr_encoder_deinit(&self->encoder, exiting);

    gsr_video_sources_deinit(&self->video_sources_data);

    if(self->av_format_context) {
        if(self->av_format_context->pb && !(self->av_format_context->oformat->flags & AVFMT_NOFILE))
            avio_close(self->av_format_context->pb);
        avformat_free_context(self->av_format_context);
    }

    gsr_recording_clock_destroy(self->recording_clock);
    free(self);
}

void gsr_recorder_stop(gsr_recorder *self) {
    atomic_store(&self->running, 0);
}

void gsr_recorder_toggle_pause(gsr_recorder *self) {
    atomic_store(&self->toggle_pause, 1);
}

void gsr_recorder_set_paused(gsr_recorder *self, bool paused) {
    atomic_store(&self->set_paused_request, paused ? GSR_SET_PAUSED_REQUEST_PAUSE : GSR_SET_PAUSED_REQUEST_UNPAUSE);
}

void gsr_recorder_toggle_replay_recording(gsr_recorder *self) {
    atomic_store(&self->replay_recording_request, GSR_REPLAY_RECORDING_REQUEST_TOGGLE);
}

void gsr_recorder_start_replay_recording(gsr_recorder *self) {
    atomic_store(&self->replay_recording_request, GSR_REPLAY_RECORDING_REQUEST_START);
}

void gsr_recorder_stop_replay_recording(gsr_recorder *self) {
    atomic_store(&self->replay_recording_request, GSR_REPLAY_RECORDING_REQUEST_STOP);
}

bool gsr_recorder_is_replay_recording(const gsr_recorder *self) {
    return atomic_load(&self->replay_recording_state) == 1;
}

void gsr_recorder_save_replay(gsr_recorder *self, int seconds, int restart_replay) {
    atomic_store(&self->save_replay_restart_replay, restart_replay);
    atomic_store(&self->save_replay_seconds, seconds);
}
