#include "../../include/recorder/muxer.h"
#include "../../include/recorder/audio_codec.h"
#include "../../include/recorder/dynamic_audio.h"
#include "../../include/recorder/audio_spool.h"
#include "../../include/ffmpeg_utils.h"
#include "../../include/utils.h"
#include "../../include/log.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/mastering_display_metadata.h>

static void gsr_recording_output_deinit(gsr_recording_output *self) {
    if(self->audio_streams) {
        free(self->audio_streams);
        self->audio_streams = NULL;
    }
    self->num_audio_streams = 0;
}

AVStream* create_stream(AVFormatContext *av_format_context, AVCodecContext *codec_context) {
    AVStream *stream = avformat_new_stream(av_format_context, NULL);
    if (!stream) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "Could not allocate stream");
        return NULL;
    }
    stream->id = av_format_context->nb_streams - 1;
    stream->time_base = codec_context->time_base;
    stream->avg_frame_rate = codec_context->framerate;
    //stream->r_frame_rate = codec_context->framerate;
    return stream;
}

bool add_hdr_metadata_to_video_stream(gsr_capture *cap, AVStream *video_stream) {
    size_t light_metadata_size = 0;
    size_t mastering_display_metadata_size = 0;
    AVContentLightMetadata *light_metadata = av_content_light_metadata_alloc(&light_metadata_size);
    #if LIBAVUTIL_VERSION_INT < AV_VERSION_INT(59, 37, 100)
    AVMasteringDisplayMetadata *mastering_display_metadata = av_mastering_display_metadata_alloc();
    mastering_display_metadata_size = sizeof(*mastering_display_metadata);
    #else
    AVMasteringDisplayMetadata *mastering_display_metadata = av_mastering_display_metadata_alloc_size(&mastering_display_metadata_size);
    #endif

    if(!light_metadata || !mastering_display_metadata) {
        if(light_metadata)
            av_freep(&light_metadata);

        if(mastering_display_metadata)
            av_freep(&mastering_display_metadata);

        return false;
    }

    if(!gsr_capture_get_frame_hdr_metadata(cap, mastering_display_metadata, light_metadata)) {
        av_freep(&light_metadata);
        av_freep(&mastering_display_metadata);
        return false;
    }

    // TODO: More error checking

    #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(60, 31, 102)
    const bool content_light_level_added = av_stream_add_side_data(video_stream, AV_PKT_DATA_CONTENT_LIGHT_LEVEL, (uint8_t*)light_metadata, light_metadata_size) == 0;
    #else
    const bool content_light_level_added = av_packet_side_data_add(&video_stream->codecpar->coded_side_data, &video_stream->codecpar->nb_coded_side_data, AV_PKT_DATA_CONTENT_LIGHT_LEVEL, light_metadata, light_metadata_size, 0) != NULL;
    #endif

    #if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(60, 31, 102)
    const bool mastering_display_metadata_added = av_stream_add_side_data(video_stream, AV_PKT_DATA_MASTERING_DISPLAY_METADATA, (uint8_t*)mastering_display_metadata, mastering_display_metadata_size) == 0;
    #else
    const bool mastering_display_metadata_added = av_packet_side_data_add(&video_stream->codecpar->coded_side_data, &video_stream->codecpar->nb_coded_side_data, AV_PKT_DATA_MASTERING_DISPLAY_METADATA, mastering_display_metadata, mastering_display_metadata_size, 0) != NULL;
    #endif

    if(!content_light_level_added)
        av_freep(&light_metadata);

    if(!mastering_display_metadata_added)
        av_freep(&mastering_display_metadata);

    // Return true even on failure because we dont want to retry adding hdr metadata on failure
    return true;
}

void set_format_context_options(AVFormatContext *av_format_context) {
    if(LIBAVFORMAT_VERSION_INT >= AV_VERSION_INT(62, 6, 101)) {
        av_opt_set(av_format_context->priv_data, "use_editlist", "1", 0);
        const AVOption *opt = av_opt_find(av_format_context->priv_data, "movflags", NULL, 0, 0);
        if (opt && opt->unit) {
            const AVOption *flag = av_opt_find(av_format_context->priv_data, "hybrid_fragmented", opt->unit, 0, 0);
            if (flag)
                av_opt_set(av_format_context->priv_data, "movflags", "+hybrid_fragmented", 0);
        }
    } else {
        const AVOutputFormat *output_format = av_format_context->oformat;
        const char *file_extension = output_format->extensions ? output_format->extensions : "";
        if(strcmp(file_extension, "mp4") != 0 && strcmp(file_extension, "mov") != 0)
            return;

        gsr_log(GSR_LOG_LEVEL_WARNING, "your FFmpeg version is known to be buggy (it doesn't have working hybrid_fragmented movflags). If you experience stutter in the mp4 file then update your FFmpeg to at least version 8 or record to a mkv file");
    }
}

bool av_write_header(AVFormatContext *av_format_context, const gsr_recorder_settings *settings) {
    AVDictionary *options = NULL;
    av_dict_set(&options, "strict", "experimental", 0);

    if(strcmp(av_format_context->oformat->name, "whip") == 0) {
        // The default udp send buffer size of the ffmpeg whip muxer (32kb) is too small to handle keyframe bursts,
        // which makes the muxer permanently fail with "UDP send blocked" errors (the muxer treats one blocked send as fatal).
        // A keyframe is sent as a burst of hundreds of rtp packets and each packet uses ~2kb of kernel buffer accounting,
        // so this needs to be large. The kernel silently caps this to net.core.wmem_max, which may also need to be raised
        av_dict_set(&options, "ts_buffer_size", "4194304", 0);
    }

    if(settings->auth && settings->auth[0] != '\0')
        av_dict_set(&options, "authorization", settings->auth, 0);

    // This is done last because -ffmpeg-opts is documented to overwrite options set by gpu screen recorder with the same name
    if(settings->ffmpeg_opts)
        av_dict_parse_string(&options, settings->ffmpeg_opts, "=", ";", 0);

    const int ret = avformat_write_header(av_format_context, &options);
    if(ret < 0)
        gsr_log(GSR_LOG_LEVEL_ERROR, "error occurred when writing header to output file: %s", gsr_av_error_to_string(ret));

    av_dict_free(&options);
    return ret >= 0;
}

bool gsr_recording_output_start(gsr_recording_output *self, const char *filename, const gsr_recorder_settings *settings, AVCodecContext *video_codec_context, const gsr_audio_capture *audio_capture, bool hdr, gsr_video_sources *video_sources) {
    memset(self, 0, sizeof(*self));

    const char *container_format = settings->container_format;
    // The whip muxer is network only. Local recordings created while streaming with whip are saved as mkv,
    // which supports the h264+opus codecs that whip forces
    if(container_format && strcmp(container_format, "whip") == 0)
        container_format = "matroska";

    AVFormatContext *av_format_context = NULL;
    avformat_alloc_output_context2(&av_format_context, NULL, container_format, filename);
    if(!av_format_context) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "gsr_recording_output_start: failed to create output context for '%s'", filename);
        return false;
    }
    set_format_context_options(av_format_context);

    AVStream *video_stream = create_stream(av_format_context, video_codec_context);
    if(!video_stream) {
        avformat_free_context(av_format_context);
        return false;
    }
    avcodec_parameters_from_context(video_stream->codecpar, video_codec_context);

    if(audio_capture->num_tracks > 0) {
        self->audio_streams = calloc(audio_capture->num_tracks, sizeof(gsr_recording_audio_stream));
        if(!self->audio_streams) {
            gsr_log(GSR_LOG_LEVEL_ERROR, "gsr_recording_output_start: failed to allocate audio streams");
            avformat_free_context(av_format_context);
            return false;
        }
    }

    for(size_t i = 0; i < audio_capture->num_tracks; ++i) {
        const gsr_audio_track *audio_track = audio_capture->tracks[i];
        AVStream *audio_stream = create_stream(av_format_context, audio_track->codec_context);
        if(!audio_stream) {
            gsr_recording_output_deinit(self);
            avformat_free_context(av_format_context);
            return false;
        }

        if(audio_track->name[0] != '\0' && !settings->exclude_metadata) {
            av_dict_set(&audio_stream->metadata, "title", audio_track->name, 0);
            av_dict_set(&audio_stream->metadata, "handler_name", audio_track->name, 0);
        }
        avcodec_parameters_from_context(audio_stream->codecpar, audio_track->codec_context);

        self->audio_streams[i].audio_track = audio_track;
        self->audio_streams[i].stream = audio_stream;
        ++self->num_audio_streams;
    }

    const int open_ret = avio_open(&av_format_context->pb, filename, AVIO_FLAG_WRITE);
    if(open_ret < 0) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "gsr_recording_output_start: could not open '%s': %s", filename, gsr_av_error_to_string(open_ret));
        gsr_recording_output_deinit(self);
        avformat_free_context(av_format_context);
        return false;
    }

    AVDictionary *options = NULL;
    av_dict_set(&options, "strict", "experimental", 0);

    if(settings->ffmpeg_opts)
        av_dict_parse_string(&options, settings->ffmpeg_opts, "=", ";", 0);

    const int header_write_ret = avformat_write_header(av_format_context, &options);
    av_dict_free(&options);
    if(header_write_ret < 0) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "gsr_recording_output_start: error occurred when writing header to output file: %s", gsr_av_error_to_string(header_write_ret));
        avio_close(av_format_context->pb);
        gsr_recording_output_deinit(self);
        avformat_free_context(av_format_context);
        return false;
    }

    for(size_t i = 0; i < video_sources->num_items; ++i) {
        if(hdr && add_hdr_metadata_to_video_stream(video_sources->items[i].capture, video_stream))
            break;
    }

    self->av_format_context = av_format_context;
    self->video_stream = video_stream;
    return true;
}

bool gsr_recording_output_stop(gsr_recording_output *self) {
    bool trailer_written = true;
    if(gsr_av_format_context_write_trailer(self->av_format_context) != 0) {
        trailer_written = false;
    }

    const bool closed = avio_close(self->av_format_context->pb) == 0;
    avformat_free_context(self->av_format_context);
    self->av_format_context = NULL;
    self->video_stream = NULL;
    gsr_recording_output_deinit(self);
    return trailer_written && closed;
}

bool gsr_create_new_recording_filepath_from_timestamp(char *filepath, size_t filepath_size, const char *directory, const char *filename_prefix, const char *file_extension, bool date_folders) {
    char date_str[128];
    char output_folder[PATH_MAX];
    int written = 0;

    if(date_folders) {
        gsr_get_date_only_str(date_str, sizeof(date_str));
        written = snprintf(output_folder, sizeof(output_folder), "%s/%s", directory, date_str);
        if(written < 0 || written >= (int)sizeof(output_folder)) {
            gsr_log(GSR_LOG_LEVEL_ERROR, "the directory path is too long: %s", directory);
            return false;
        }

        if(create_directory_recursive(output_folder) != 0)
            gsr_log(GSR_LOG_LEVEL_ERROR, "failed to create directory: %s", output_folder);

        gsr_get_time_only_str(date_str, sizeof(date_str));
    } else {
        written = snprintf(output_folder, sizeof(output_folder), "%s", directory);
        if(written < 0 || written >= (int)sizeof(output_folder)) {
            gsr_log(GSR_LOG_LEVEL_ERROR, "the directory path is too long: %s", directory);
            return false;
        }

        if(create_directory_recursive(output_folder) != 0)
            gsr_log(GSR_LOG_LEVEL_ERROR, "failed to create directory: %s", output_folder);

        gsr_get_date_str(date_str, sizeof(date_str));
    }

    written = snprintf(filepath, filepath_size, "%s/%s_%s.%s", output_folder, filename_prefix, date_str, file_extension);
    if(written < 0 || written >= (int)filepath_size) {
        gsr_log(GSR_LOG_LEVEL_ERROR, "the output filepath is too long");
        return false;
    }

    return true;
}

gsr_recording_audio_stream* gsr_recording_output_get_audio_stream_by_index(gsr_recording_output *self, int stream_index) {
    for(size_t i = 0; i < self->num_audio_streams; ++i) {
        if(self->audio_streams[i].audio_track->stream_index == stream_index)
            return &self->audio_streams[i];
    }
    return NULL;
}

size_t calculate_estimated_replay_buffer_packets(int64_t replay_buffer_size_secs, int fps, gsr_audio_codec audio_codec, const gsr_audio_input_tracks *audio_inputs) {
    if(replay_buffer_size_secs == -1)
        return 0;

    const char *dynamic = getenv("SHADOWLESS_DYNAMIC_AUDIO");
    const size_t num_tracks = dynamic && strcmp(dynamic, "1") == 0
        ? GSR_MAX_DYNAMIC_AUDIO_TRACKS : audio_inputs->num_items;
    int audio_fps = 0;
    if(num_tracks > 0)
        audio_fps = GSR_AUDIO_SAMPLE_RATE / audio_codec_get_frame_size(audio_codec);

    return replay_buffer_size_secs * (fps + audio_fps * num_tracks);
}


typedef struct {
    AVFormatContext *context;
    AVPacket *packet;
    int *stream_map;
    bool finished;
    int64_t origin_us;
    int64_t end_us;
} gsr_audio_spool_input;

static bool audio_spool_read_packet(gsr_audio_spool_input *input, bool stem, int64_t *next_progress_us) {
    av_packet_unref(input->packet);
    for(;;) {
        const int result = av_read_frame(input->context, input->packet);
        if(result == AVERROR_EOF) {
            input->finished = true;
            return true;
        }
        if(result < 0)
            return false;
        const int64_t now_us = av_gettime_relative();
        if(now_us >= *next_progress_us) {
            gsr_log(GSR_LOG_LEVEL_INFO, "shadowless-audio-finalizing: progress");
            *next_progress_us = now_us + 2000000;
        }
        const int index = input->packet->stream_index;
        if(index < 0 || (unsigned)index >= input->context->nb_streams)
            return false;
        if(input->stream_map[index] < 0) {
            av_packet_unref(input->packet);
            continue;
        }
        if(stem) {
            AVStream *stream = input->context->streams[index];
            const int64_t pts = input->packet->pts != AV_NOPTS_VALUE ? input->packet->pts : input->packet->dts;
            if(pts == AV_NOPTS_VALUE)
                return false;
            const int64_t time_us = av_rescale_q(pts, stream->time_base, AV_TIME_BASE_Q);
            if(time_us < input->origin_us || time_us >= input->end_us) {
                av_packet_unref(input->packet);
                continue;
            }
        }
        return true;
    }
}

bool gsr_audio_spools_finalize(const char *base_filepath, const char **spool_paths,
        const char **names, size_t count, int64_t origin_us) {
    if(!base_filepath || !base_filepath[0] || (count && (!spool_paths || !names)) || count > GSR_MAX_DYNAMIC_AUDIO_TRACKS)
        return false;
    if(count == 0)
        return true;

    gsr_log(GSR_LOG_LEVEL_INFO, "shadowless-audio-finalizing: progress");
    int64_t next_progress_us = av_gettime_relative() + 2000000;
    bool success = false;
    bool temporary_created = false;
    AVFormatContext *output = NULL, *validation = NULL;
    gsr_audio_spool_input *inputs = calloc(count + 1, sizeof(*inputs));
    char temporary[PATH_MAX] = {0};
    if(!inputs)
        return false;
    const int written = snprintf(temporary, sizeof(temporary), "%s.dynamic-XXXXXX", base_filepath);
    if(written < 0 || (size_t)written >= sizeof(temporary))
        goto cleanup;
    const AVOutputFormat *format = av_guess_format(NULL, base_filepath, NULL);
    if(!format || avformat_alloc_output_context2(&output, format, NULL, temporary) < 0)
        goto cleanup;

    for(size_t i = 0; i <= count; ++i) {
        gsr_audio_spool_input *input = &inputs[i];
        const char *path = i == 0 ? base_filepath : spool_paths[i - 1];
        if(!path || avformat_open_input(&input->context, path, NULL, NULL) < 0
                || avformat_find_stream_info(input->context, NULL) < 0)
            goto cleanup;
        if(i == 0 && (input->context->duration == AV_NOPTS_VALUE || input->context->duration <= 0))
            goto cleanup;
        if(origin_us > INT64_MAX - inputs[0].context->duration)
            goto cleanup;
        input->origin_us = i == 0 ? 0 : origin_us;
        input->end_us = origin_us + inputs[0].context->duration;
        input->stream_map = malloc(input->context->nb_streams * sizeof(*input->stream_map));
        input->packet = av_packet_alloc();
        if(!input->stream_map || !input->packet)
            goto cleanup;
        unsigned included = 0;
        for(unsigned j = 0; j < input->context->nb_streams; ++j) {
            AVStream *from = input->context->streams[j];
            input->stream_map[j] = -1;
            if(i > 0 && from->codecpar->codec_type != AVMEDIA_TYPE_AUDIO)
                continue;
            AVStream *to = avformat_new_stream(output, NULL);
            if(!to || avcodec_parameters_copy(to->codecpar, from->codecpar) < 0)
                goto cleanup;
            to->codecpar->codec_tag = 0;
            to->time_base = from->time_base;
            to->avg_frame_rate = from->avg_frame_rate;
            to->sample_aspect_ratio = from->sample_aspect_ratio;
            to->disposition = from->disposition;
            if(av_dict_copy(&to->metadata, from->metadata, 0) < 0)
                goto cleanup;
            if(i > 0 && names[i - 1]) {
                if(av_dict_set(&to->metadata, "title", names[i - 1], 0) < 0
                        || av_dict_set(&to->metadata, "handler_name", names[i - 1], 0) < 0)
                    goto cleanup;
            }
            input->stream_map[j] = to->index;
            ++included;
        }
        if(included == 0)
            goto cleanup;
        if(i == 0 && av_dict_copy(&output->metadata, input->context->metadata, 0) < 0)
            goto cleanup;
        if(!audio_spool_read_packet(input, i > 0, &next_progress_us))
            goto cleanup;
    }

    const int fd = mkstemp(temporary);
    if(fd < 0)
        goto cleanup;
    close(fd);
    temporary_created = true;
    if(avio_open(&output->pb, temporary, AVIO_FLAG_WRITE) < 0 || avformat_write_header(output, NULL) < 0)
        goto cleanup;

    for(;;) {
        size_t next = count + 1;
        int64_t next_dts = INT64_MAX;
        for(size_t i = 0; i <= count; ++i) {
            gsr_audio_spool_input *input = &inputs[i];
            if(input->finished)
                continue;
            AVPacket *packet = input->packet;
            const int64_t dts = packet->dts != AV_NOPTS_VALUE ? packet->dts : packet->pts;
            if(dts == AV_NOPTS_VALUE)
                goto cleanup;
            const int64_t normalized = av_rescale_q(dts,
                input->context->streams[packet->stream_index]->time_base, AV_TIME_BASE_Q) - input->origin_us;
            if(next == count + 1 || normalized < next_dts) {
                next = i;
                next_dts = normalized;
            }
        }
        if(next == count + 1)
            break;
        gsr_audio_spool_input *input = &inputs[next];
        AVPacket *packet = input->packet;
        AVStream *from = input->context->streams[packet->stream_index];
        AVStream *to = output->streams[input->stream_map[packet->stream_index]];
        const int64_t offset = av_rescale_q(input->origin_us, AV_TIME_BASE_Q, from->time_base);
        if(packet->pts != AV_NOPTS_VALUE) packet->pts -= offset;
        if(packet->dts != AV_NOPTS_VALUE) packet->dts -= offset;
        av_packet_rescale_ts(packet, from->time_base, to->time_base);
        packet->stream_index = to->index;
        packet->pos = -1;
        if(av_interleaved_write_frame(output, packet) < 0 || !audio_spool_read_packet(input, next > 0, &next_progress_us))
            goto cleanup;
    }
    if(av_write_trailer(output) < 0 || avio_closep(&output->pb) < 0)
        goto cleanup;
    if(avformat_open_input(&validation, temporary, NULL, NULL) < 0
            || avformat_find_stream_info(validation, NULL) < 0
            || validation->nb_streams != output->nb_streams
            || validation->duration <= 0 || validation->duration == AV_NOPTS_VALUE)
        goto cleanup;
    avformat_close_input(&validation);
    if(rename(temporary, base_filepath) != 0)
        goto cleanup;
    success = true;

cleanup:
    if(!success)
        gsr_log(GSR_LOG_LEVEL_ERROR, "dynamic audio finalization failed; recording and audio spools preserved: %s", base_filepath);
    avformat_close_input(&validation);
    if(output) {
        if(output->pb) avio_closep(&output->pb);
        avformat_free_context(output);
    }
    for(size_t i = 0; i <= count; ++i) {
        av_packet_free(&inputs[i].packet);
        avformat_close_input(&inputs[i].context);
        free(inputs[i].stream_map);
    }
    free(inputs);
    if(!success && temporary_created) unlink(temporary);
    return success;
}
