"""Native dynamic-audio storage and real saved-media clock regressions (no audio server)."""
from pathlib import Path
import json
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
ENGINE = ROOT / 'linux/third_party/gpu-screen-recorder'


def compile_fixture(directory, source, libraries, sources=()):
    path = Path(directory)
    (path / 'fixture.c').write_text(source)
    flags = shlex.split(subprocess.check_output(
        ['pkg-config', '--cflags', '--libs', *libraries], text=True))
    subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-Wno-unused-parameter', '-ffunction-sections',
                    '-fdata-sections', '-Wl,--gc-sections', '-I', str(ENGINE / 'include'),
                    str(path / 'fixture.c'), *(str(ENGINE / item) for item in sources),
                    '-o', str(path / 'fixture'), *flags, '-pthread', '-lm'], check=True)
    return path / 'fixture'


class DynamicAudioTests(unittest.TestCase):
    def test_tracks_keep_identity_when_applications_arrive_after_start(self):
        source = r'''
#include "recorder/audio_capture.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    gsr_audio_capture capture = {0};
    gsr_audio_track first = {0};
    strcpy(first.name, "App: browser"); first.stream_index = 7;
    assert(gsr_audio_capture_add_track(&capture, &first));
    gsr_audio_track *identity = capture.tracks[0];
    for(int i = 1; i < 40; ++i) {
        gsr_audio_track late = {0}; late.stream_index = i + 7;
        assert(gsr_audio_capture_add_track(&capture, &late));
        assert(capture.tracks[0] == identity);
        assert(identity->stream_index == 7);
        assert(strcmp(identity->name, "App: browser") == 0);
    }
    for(size_t i = 0; i < capture.num_tracks; ++i) free(capture.tracks[i]);
    free(capture.tracks);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            fixture = compile_fixture(directory, source,
                ['libavfilter', 'libavcodec', 'libavutil', 'libswresample', 'libdrm', 'x11', 'xrandr'],
                ['src/recorder/audio_capture.c', 'src/utils.c', 'src/log.c'])
            subprocess.run([str(fixture)], check=True)

    def test_late_app_preparation_is_unpublished_and_shutdown_joins_worker(self):
        # Replace only the server/codec opening boundary; exercise the real
        # discovery worker and ownership cleanup without a running audio server.
        recorder = (ENGINE / 'src/recorder/recorder.c').read_text()
        begin = recorder.index('static int recorder_setup_audio_track(')
        end = recorder.index('\nstatic bool recorder_start_audio_spool', begin)
        recorder = recorder[:begin] + r'''
static atomic_bool opening, release_open;
static pthread_t capture_thread;
static int closes;
static int recorder_setup_audio_track(gsr_recorder *self, const gsr_merged_audio_inputs *inputs, int index, gsr_audio_track *track) {
    assert(!pthread_equal(pthread_self(), capture_thread));
    assert(self->audio_capture.num_tracks == 0);
    atomic_store(&opening, true);
    while(!atomic_load(&release_open)) av_usleep(1000);
    track->stream_index = index;
    return GSR_ERROR_OK;
}
''' + recorder[end:]
        recorder = recorder.replace('#include "../../include/', '#include "' + str(ENGINE / 'include') + '/')
        source = '#define GSR_APP_AUDIO\n' + recorder + r'''
void gsr_pipewire_audio_for_each_app(gsr_pipewire_audio *self, gsr_pipewire_audio_app_query_callback callback, void *userdata) {
    (void)self;
    callback("Late browser", userdata);
}
void gsr_audio_track_deinit(gsr_audio_track *track) {
    assert(pthread_equal(pthread_self(), capture_thread));
    assert(strcmp(track->application_name, "Late browser") == 0);
    ++closes;
}
void gsr_audio_capture_lock_filter(gsr_audio_capture *capture) { (void)capture; assert(0); }
void gsr_audio_capture_unlock_filter(gsr_audio_capture *capture) { (void)capture; assert(0); }
bool gsr_audio_capture_add_track(gsr_audio_capture *capture, const gsr_audio_track *track) {
    (void)capture; (void)track; assert(0); return false;
}
int gsr_audio_capture_start_track(gsr_audio_capture *capture, gsr_audio_track *track) {
    (void)capture; (void)track; assert(0); return GSR_ERROR_GENERIC;
}
double gsr_recording_clock_get_time(const gsr_recording_clock *clock) { (void)clock; return 0; }
AVStream *create_stream(AVFormatContext *format, AVCodecContext *codec) {
    (void)format; (void)codec; assert(0); return NULL;
}
size_t gsr_encoder_add_recording_destination(gsr_encoder *encoder, AVCodecContext *codec,
        AVFormatContext *format, AVStream *stream, int64_t pts) {
    (void)encoder; (void)codec; (void)format; (void)stream; (void)pts; assert(0); return (size_t)-1;
}
int main(void) {
    capture_thread = pthread_self();
    atomic_init(&opening, false); atomic_init(&release_open, false);
    gsr_recorder *recorder = calloc(1, sizeof(*recorder)); assert(recorder);
    recorder->dynamic_audio = true; recorder->audio_started = true;
    assert(recorder_discover_audio(recorder) == GSR_ERROR_OK);
    recorder_audio_discovery *job = recorder->audio_discovery; assert(job);
    while(!atomic_load(&opening)) av_usleep(1000);
    /* Opening is blocked, but the capture caller is free and nothing is
       published into audio capture or replay-visible storage. */
    assert(!atomic_load(&job->finished));
    assert(recorder_discover_audio(recorder) == GSR_ERROR_OK);
    assert(!atomic_load(&job->finished));
    assert(recorder->audio_capture.num_tracks == 0);
    atomic_store(&release_open, true);
    while(!atomic_load(&job->finished)) av_usleep(1000);
    assert(job->prepared && job->result == GSR_ERROR_OK);
    assert(job->track.stream_index == 1 && !strcmp(job->track.application_name, "Late browser"));
    assert(recorder->audio_capture.num_tracks == 0);
    recorder_join_audio_discovery(recorder);
    assert(!recorder->audio_discovery && closes == 1);
    recorder_join_audio_discovery(recorder); assert(closes == 1);
    /* 64 application identities leave separate slots for remainder and mic. */
    assert(GSR_MAX_DYNAMIC_AUDIO_TRACKS >= GSR_MAX_DYNAMIC_AUDIO_APPS + 2);
    recorder_app_snapshot snapshot = {0};
    for(int i = 0; i < GSR_MAX_DYNAMIC_AUDIO_APPS; ++i) assert(recorder_copy_app("app", &snapshot));
    assert(!snapshot.overflow && snapshot.count == GSR_MAX_DYNAMIC_AUDIO_APPS);
    assert(!recorder_copy_app("overflow", &snapshot) && snapshot.overflow);
    free(recorder);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            fixture = compile_fixture(directory, source,
                ['libavformat', 'libavcodec', 'libavutil', 'libavfilter', 'libdrm', 'libpipewire-0.3'],
                ['src/json.c', 'src/log.c'])
            subprocess.run([str(fixture)], check=True, timeout=10)

    @unittest.skipUnless(shutil.which('ffmpeg'), 'FFmpeg required')
    def test_finalizer_keeps_capture_thread_free_and_notifies_once_after_join(self):
        source = r'''
#define GSR_APP_AUDIO
#include "RECORDER_SOURCE"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
static pthread_t capture_thread;
static int calls, successes, removals;
static void stopped(const char *path, void *userdata) {
    (void)userdata;
    assert(pthread_equal(pthread_self(), capture_thread));
    ++calls;
    if(path) ++successes;
}
bool gsr_encoder_remove_recording_destination(gsr_encoder *self, size_t id) {
    (void)self; (void)id; ++removals; return true;
}
static AVFormatContext *open_spool(const char *source, const char *path) {
    AVFormatContext *input = NULL, *output = NULL;
    assert(avformat_open_input(&input, source, NULL, NULL) >= 0);
    assert(avformat_find_stream_info(input, NULL) >= 0);
    assert(avformat_alloc_output_context2(&output, NULL, "matroska", path) >= 0);
    AVStream *stream = avformat_new_stream(output, NULL); assert(stream);
    assert(avcodec_parameters_copy(stream->codecpar, input->streams[0]->codecpar) >= 0);
    stream->time_base = input->streams[0]->time_base;
    assert(avio_open(&output->pb, path, AVIO_FLAG_WRITE) >= 0);
    assert(avformat_write_header(output, NULL) >= 0);
    AVPacket *packet = av_packet_alloc(); assert(packet);
    while(av_read_frame(input, packet) >= 0) {
        av_packet_rescale_ts(packet, input->streams[0]->time_base, stream->time_base);
        assert(av_interleaved_write_frame(output, packet) >= 0);
        av_packet_unref(packet);
    }
    av_packet_free(&packet); avformat_close_input(&input);
    return output; /* recorder_finish_audio_spools owns trailer/close. */
}
typedef struct { const char *source, *fifo; } writer_args;
static void *write_base(void *userdata) {
    const writer_args *args = userdata;
    FILE *input = fopen(args->source, "rb"), *output = fopen(args->fifo, "wb");
    assert(input && output);
    char buffer[4096]; size_t size;
    while((size = fread(buffer, 1, sizeof(buffer), input)) > 0) assert(fwrite(buffer, 1, size, output) == size);
    assert(!ferror(input)); fclose(input); assert(fclose(output) == 0);
    return NULL;
}
static void set_spool(gsr_recorder *recorder, gsr_audio_track *track, const char *source, const char *output) {
    recorder->spooling = true; recorder->num_spools = 1;
    recorder->spools[0].format = open_spool(source, output);
    recorder->spools[0].track = track;
    snprintf(recorder->spools[0].path, sizeof(recorder->spools[0].path), "%s", output);
}
int main(int argc, char **argv) {
    assert(argc == 6);
    capture_thread = pthread_self();
    gsr_recorder *recorder = calloc(1, sizeof(*recorder)); assert(recorder);
    recorder->dynamic_audio = true;
    recorder->callbacks.recording_stopped = stopped;
    gsr_audio_track track = {0}; strcpy(track.name, "App: browser");
    assert(mkfifo(argv[3], 0600) == 0);
    set_spool(recorder, &track, argv[2], argv[4]);
    assert(recorder_finish_audio_spools(recorder, argv[3]));
    assert(recorder->audio_finalizer && !atomic_load(&recorder->audio_finalizer->finished));
    assert(calls == 0 && removals == 1);
    unsigned heartbeat = 0;
    for(unsigned i = 0; i < 1000; ++i) {
        ++heartbeat;
        assert(recorder_poll_audio_finalizer(recorder, false, true));
        assert(recorder->audio_finalizer && calls == 0);
    }
    assert(heartbeat == 1000); /* blocked real media I/O cannot block capture-loop polling. */
    writer_args args = {argv[1], argv[3]}; pthread_t writer;
    assert(pthread_create(&writer, NULL, write_base, &args) == 0);
    assert(recorder_poll_audio_finalizer(recorder, true, true));
    pthread_join(writer, NULL);
    assert(!recorder->audio_finalizer && calls == 1 && successes == 1);
    assert(access(argv[4], F_OK) != 0);
    assert(recorder_poll_audio_finalizer(recorder, true, true)); assert(calls == 1);
    set_spool(recorder, &track, argv[2], argv[4]);
    assert(recorder_finish_audio_spools(recorder, argv[5]));
    assert(!recorder_poll_audio_finalizer(recorder, true, true));
    assert(calls == 2 && successes == 1 && !recorder->audio_finalizer);
    assert(access(argv[4], F_OK) == 0 && access(argv[5], F_OK) == 0);
    assert(recorder_poll_audio_finalizer(recorder, true, true)); assert(calls == 2);
    free(recorder);
    return 0;
}
'''.replace('RECORDER_SOURCE', str(ENGINE / 'src/recorder/recorder.c'))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            fixture = compile_fixture(path, source,
                ['libavformat', 'libavcodec', 'libavutil', 'libavfilter', 'libdrm', 'libpipewire-0.3'],
                ['src/recorder/muxer.c', 'src/log.c'])
            base, stem = path / 'video.mkv', path / 'audio.mkv'
            subprocess.run(['ffmpeg', '-v', 'error', '-nostdin', '-f', 'lavfi', '-i',
                'color=size=32x32:rate=25:duration=6', '-c:v', 'mpeg4', '-bf', '0', '-threads', '1', str(base)], check=True)
            subprocess.run(['ffmpeg', '-v', 'error', '-nostdin', '-f', 'lavfi', '-i',
                'sine=frequency=880:sample_rate=48000:duration=3', '-c:a', 'aac', '-threads', '1', str(stem)], check=True)
            invalid = path / 'invalid.mkv'; invalid.write_bytes(b'not media')
            subprocess.run([str(fixture), str(base), str(stem), str(path / 'blocked.mkv'),
                            str(path / 'spool.mkv'), str(invalid)], check=True, timeout=20)

    def test_recovered_packet_write_failure_preserves_recording_and_spool(self):
        source = r'''
#define GSR_APP_AUDIO
#include "RECORDER_SOURCE"
static int writes;
static int fail_once(AVFormatContext *format, AVPacket *packet) {
    if(++writes == 1) return AVERROR(ENOSPC);
    return av_write_frame(format, packet);
}
#define av_write_frame fail_once
#include "ENCODER_SOURCE"
#undef av_write_frame
bool gsr_replay_buffer_append(gsr_replay_buffer *buffer, const AVPacket *packet, double time) {
    (void)buffer; (void)packet; (void)time; assert(0); return false;
}
void gsr_av_format_context_mark_packet_written(AVFormatContext *format) { (void)format; }
int main(int argc, char **argv) {
    assert(argc == 2);
    gsr_recorder *recorder = calloc(1, sizeof(*recorder)); assert(recorder);
    assert(pthread_mutex_init(&recorder->encoder.file_write_mutex, NULL) == 0);
    const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE); assert(encoder);
    AVCodecContext *codec = avcodec_alloc_context3(encoder); assert(codec);
    codec->sample_rate = 48000; codec->sample_fmt = AV_SAMPLE_FMT_S16;
    codec->time_base = (AVRational){1,48000};
    av_channel_layout_default(&codec->ch_layout, 2);
    assert(avcodec_open2(codec, encoder, NULL) == 0);
    gsr_audio_track track = {.codec_context = codec, .dynamic_source = true};
    strcpy(track.name, "App: browser");
    recorder->dynamic_audio = true; recorder->spooling = true;
    snprintf(recorder->spool_directory, sizeof(recorder->spool_directory), "%s", argv[1]);
    assert(recorder_start_audio_spool(recorder, &track));
    char stem[PATH_MAX]; snprintf(stem, sizeof(stem), "%s", recorder->spools[0].path);
    char base[PATH_MAX]; snprintf(base, sizeof(base), "%s/base.mkv", argv[1]);
    FILE *file = fopen(base, "wb"); assert(file); fputs("original recording", file); fclose(file);
    AVFrame *frame = av_frame_alloc(); assert(frame);
    frame->format = codec->sample_fmt; frame->sample_rate = codec->sample_rate;
    frame->nb_samples = 480; av_channel_layout_copy(&frame->ch_layout, &codec->ch_layout);
    assert(av_frame_get_buffer(frame, 0) == 0);
    memset(frame->data[0], 0, 480 * 4);
    for(int i = 0; i < 2; ++i) {
        frame->pts = i * 480;
        assert(avcodec_send_frame(codec, frame) == 0);
        gsr_encoder_receive_packets(&recorder->encoder, codec, frame->pts, 1);
    }
    assert(writes == 2 && recorder->encoder.recording_destinations[0].write_failed);
    /* Trailer and close can succeed after a failed packet. That must never
       turn this recording into a success or delete the recoverable inputs. */
    assert(!recorder_finish_audio_spools(recorder, base));
    assert(recorder->spool_failed && !recorder->audio_finalizer);
    assert(!recorder->spooling && !recorder->encoder.num_recording_destinations);
    assert(access(base, F_OK) == 0 && access(stem, F_OK) == 0);
    file = fopen(base, "rb"); assert(file); char data[32] = {0};
    assert(fread(data, 1, sizeof(data), file) == 18 && !strcmp(data, "original recording")); fclose(file);
    av_frame_free(&frame); avcodec_free_context(&codec);
    pthread_mutex_destroy(&recorder->encoder.file_write_mutex); free(recorder);
    return 0;
}
'''.replace('RECORDER_SOURCE', str(ENGINE / 'src/recorder/recorder.c')).replace(
            'ENCODER_SOURCE', str(ENGINE / 'src/encoder/encoder.c'))
        with tempfile.TemporaryDirectory() as directory:
            fixture = compile_fixture(directory, source,
                ['libavformat', 'libavcodec', 'libavutil', 'libavfilter', 'libdrm', 'libpipewire-0.3', 'x11', 'xrandr'],
                ['src/recorder/muxer.c', 'src/log.c', 'src/utils.c'])
            # Keep the spool name within GSR's fixed buffer even under a long TMPDIR.
            subprocess.run([str(fixture), '.'], cwd=directory, check=True, timeout=10)

    def test_live_application_choices_handle_escaped_identity_and_preserve_capture_clock(self):
        source = r'''
#define GSR_APP_AUDIO
#include "RECORDER_SOURCE"
int main(int argc, char **argv) {
    assert(argc == 2);
    setenv("SHADOWLESS_AUDIO_APPS_FILE", argv[1], 1);
    unsetenv("SHADOWLESS_GAME_AUDIO_APP");
    gsr_audio_track track = {0};
    strcpy(track.application_name, "Browser \"work\"\nprofile");
    track.pts = 144000;
    AVCodecContext codec = {0}; track.codec_context = &codec;
    recorder_update_app_choice(&track);
    assert(atomic_load(&track.game) && !track.included);
    assert(strncmp(track.name, "Game (off): ", 12) == 0);
    assert(track.pts == 144000 && track.codec_context == &codec);
    FILE *file = fopen(argv[1], "wb"); assert(file);
    fputs("{\"Browser \\\"work\\\"\\nprofile\":{\"included\":true,\"game\":false}}", file);
    assert(fclose(file) == 0);
    recorder_update_app_choice(&track);
    assert(!atomic_load(&track.game) && track.included);
    assert(strncmp(track.name, "App: ", 5) == 0);
    assert(track.pts == 144000 && track.codec_context == &codec);
    return 0;
}
'''.replace('RECORDER_SOURCE', str(ENGINE / 'src/recorder/recorder.c'))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            fixture = compile_fixture(path, source,
                ['libavformat', 'libavcodec', 'libavutil', 'libavfilter', 'libdrm', 'libpipewire-0.3'], ['src/json.c'])
            choices = path / 'choices.json'
            choices.write_text(json.dumps({'Browser "work"\nprofile': {'included': False, 'game': True}}))
            subprocess.run([str(fixture), str(choices)], check=True)

    def test_sparse_capture_ids_map_to_contiguous_saved_streams(self):
        source = r'''
#include "recorder/muxer.h"
#include <assert.h>
int main(void) {
    gsr_audio_track browser = {.stream_index = 7}, game = {.stream_index = 42};
    AVStream browser_stream = {.index = 1}, game_stream = {.index = 2};
    gsr_recording_audio_stream streams[] = {{.audio_track=&browser, .stream=&browser_stream},
                                          {.audio_track=&game, .stream=&game_stream}};
    gsr_recording_output output = {.audio_streams=streams, .num_audio_streams=2};
    assert(gsr_recording_output_get_audio_stream_by_index(&output, 7)->stream->index == 1);
    assert(gsr_recording_output_get_audio_stream_by_index(&output, 42)->stream->index == 2);
    assert(gsr_recording_output_get_audio_stream_by_index(&output, 1) == NULL);
    assert(gsr_recording_output_get_audio_stream_by_index(&output, 100) == NULL);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            fixture = compile_fixture(directory, source,
                ['libavformat', 'libavcodec', 'libavutil', 'libavfilter', 'libdrm'], ['src/recorder/muxer.c'])
            subprocess.run([str(fixture)], check=True)

    def test_large_packet_budget_still_expires_old_replay_data_and_keeps_save_snapshot(self):
        source = r'''
#include "replay_buffer/replay_buffer.h"
#include "replay_buffer/replay_buffer_disk.h"
#include <assert.h>
gsr_replay_buffer *gsr_replay_buffer_disk_create(const char *directory, double seconds) {
    (void)directory; (void)seconds; return NULL;
}
static void append(gsr_replay_buffer *buffer, int first, int last) {
    uint8_t data = 1;
    AVPacket packet = {0}; packet.data = &data; packet.size = 1;
    packet.flags = AV_PKT_FLAG_KEY;
    for(int i = first; i <= last; ++i) {
        packet.pts = packet.dts = i;
        assert(gsr_replay_buffer_append(buffer, &packet, i));
    }
}
static int64_t first_pts(gsr_replay_buffer *buffer) {
    return gsr_replay_buffer_iterator_get_packet(buffer, (gsr_replay_buffer_iterator){0,0})->pts;
}
int main(void) {
    gsr_replay_buffer *buffer = gsr_replay_buffer_create(GSR_REPLAY_STORAGE_RAM, NULL, 30, 1000);
    assert(buffer); append(buffer, 0, 100);
    assert(first_pts(buffer) == 70);
    gsr_replay_buffer *snapshot = gsr_replay_buffer_clone(buffer); assert(snapshot);
    assert(first_pts(snapshot) == 70);
    append(buffer, 101, 120);
    assert(first_pts(buffer) == 90 && first_pts(snapshot) == 70);
    gsr_replay_buffer_iterator iterator = {0,0};
    int64_t expected = 70;
    do {
        assert(gsr_replay_buffer_iterator_get_packet(snapshot, iterator)->pts == expected++);
    } while(gsr_replay_buffer_iterator_next(snapshot, &iterator));
    assert(expected == 101);
    gsr_replay_buffer_clear(buffer); append(buffer, 200, 200);
    assert(first_pts(buffer) == 200 && first_pts(snapshot) == 70);
    gsr_replay_buffer_destroy(snapshot); gsr_replay_buffer_destroy(buffer);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            fixture = compile_fixture(directory, source,
                ['libavcodec', 'libavutil', 'libdrm', 'x11', 'xrandr'],
                ['src/replay_buffer/replay_buffer.c', 'src/replay_buffer/replay_buffer_ram.c', 'src/utils.c', 'src/log.c'])
            subprocess.run([str(fixture)], check=True)

    def test_replay_packet_budget_reserves_late_sources(self):
        source = r'''
#include "recorder/muxer.h"
#include "recorder/dynamic_audio.h"
#include <assert.h>
#include <stdlib.h>
int main(void) {
    gsr_audio_input_tracks inputs = {0}; inputs.num_items = 1;
    unsetenv("SHADOWLESS_DYNAMIC_AUDIO");
    const size_t legacy = calculate_estimated_replay_buffer_packets(30, 60, GSR_AUDIO_CODEC_AAC, &inputs);
    setenv("SHADOWLESS_DYNAMIC_AUDIO", "1", 1);
    const size_t dynamic = calculate_estimated_replay_buffer_packets(30, 60, GSR_AUDIO_CODEC_AAC, &inputs);
    assert(dynamic > legacy * 10);
    inputs.num_items = 40;
    assert(calculate_estimated_replay_buffer_packets(30, 60, GSR_AUDIO_CODEC_AAC, &inputs) == dynamic);
    assert(calculate_estimated_replay_buffer_packets(-1, 60, GSR_AUDIO_CODEC_AAC, &inputs) == 0);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            fixture = compile_fixture(directory, source,
                ['libavformat', 'libavcodec', 'libavutil', 'libavfilter', 'libdrm'],
                ['src/recorder/muxer.c', 'src/recorder/audio_codec.c', 'src/log.c'])
            subprocess.run([str(fixture)], check=True)

    def test_replay_origin_uses_video_clock_for_late_and_reopened_sources(self):
        source = r'''
#include "recorder/dynamic_audio.h"
#include <assert.h>
int main(void) {
    /* Replay starts at video time 2s; an app first sounds at capture time 5s. */
    const int64_t origin = gsr_dynamic_audio_origin(50, (AVRational){1,25}, (AVRational){1,48000});
    assert(origin == 96000);
    assert(240000 - origin == 144000);
    /* The same source resumes at 7s and keeps its original shared clock. */
    assert(336000 - origin == 240000);
    assert(gsr_dynamic_audio_origin(2000000, (AVRational){1,1000000}, (AVRational){1,48000}) == origin);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            fixture = compile_fixture(directory, source, ['libavutil'])
            subprocess.run([str(fixture)], check=True)

    @unittest.skipUnless(shutil.which('ffmpeg') and shutil.which('ffprobe'), 'FFmpeg tools required')
    def test_continuous_finalization_keeps_late_stems_and_preserves_failed_inputs(self):
        source = r'''
#include "recorder/audio_spool.h"
#include <assert.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    assert(argc == 4 || argc == 5);
    const char *paths[] = {argv[2], argc == 5 ? argv[4] : NULL};
    const char *names[] = {"App: late-browser", "Game: running-game"};
    assert(gsr_audio_spools_finalize(argv[1], paths, names, argc == 5 ? 2 : 1, 2000000) == (atoi(argv[3]) != 0));
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            fixture = compile_fixture(path, source, ['libavformat', 'libavcodec', 'libavutil', 'libavfilter', 'libdrm'],
                                      ['src/recorder/muxer.c', 'src/log.c'])
            stem = path / 'late.mkv'
            subprocess.run(['ffmpeg', '-v', 'error', '-nostdin', '-f', 'lavfi', '-i',
                'sine=frequency=880:sample_rate=48000:duration=3', '-af', 'asetpts=PTS+5/TB',
                '-c:a', 'aac', '-threads', '1', '-copyts', str(stem)], check=True)
            game = path / 'game.mkv'
            subprocess.run(['ffmpeg', '-v', 'error', '-nostdin', '-f', 'lavfi', '-i',
                'sine=frequency=440:sample_rate=48000:duration=9', '-af', 'asetpts=PTS+2/TB',
                '-c:a', 'aac', '-threads', '1', '-copyts', str(game)], check=True)
            import hashlib
            for extension in ('mp4', 'mkv'):
                with self.subTest(container=extension):
                    base = path / f'continuous.{extension}'
                    subprocess.run(['ffmpeg', '-v', 'error', '-nostdin', '-f', 'lavfi', '-i',
                        'color=size=32x32:rate=25:duration=6', '-c:v', 'mpeg4', '-bf', '0',
                        '-threads', '1', str(base)], check=True)
                    initial = hashlib.sha256(base.read_bytes()).digest()
                    bad = path / 'broken.mkv'
                    bad.write_bytes(b'not a media file')
                    subprocess.run([str(fixture), str(base), str(stem), '0', str(bad)], check=True)
                    self.assertEqual(hashlib.sha256(base.read_bytes()).digest(), initial)
                    self.assertTrue(bad.exists())
                    subprocess.run([str(fixture), str(base), str(stem), '1', str(game)], check=True)
                    self.assertTrue(stem.exists(), 'caller retains spool until successful finalization is acknowledged')
                    info = json.loads(subprocess.check_output(['ffprobe', '-v', 'error', '-show_streams',
                        '-show_packets', '-of', 'json', str(base)], text=True))
                    self.assertEqual(len(info['streams']), 3)
                    audio = info['streams'][1]
                    self.assertEqual(audio['codec_type'], 'audio')
                    self.assertIn('App: late-browser', audio.get('tags', {}).values())
                    self.assertIn('Game: running-game', info['streams'][2].get('tags', {}).values())
                    game_packets = [float(packet['pts_time']) for packet in info['packets'] if packet['stream_index'] == 2]
                    self.assertAlmostEqual(game_packets[0], 0.0, delta=0.05)
                    self.assertLess(game_packets[-1], 6.01, 'stem data after recording end is trimmed')
                    first_audio = next(float(packet['pts_time']) for packet in info['packets'] if packet['stream_index'] == 1)
                    self.assertAlmostEqual(first_audio, 3.0, delta=0.05)
                    subprocess.run(['ffmpeg', '-v', 'error', '-nostdin', '-i', str(base), '-f', 'null', '-'], check=True)

    @unittest.skipUnless(shutil.which('ffmpeg') and shutil.which('ffprobe'), 'FFmpeg tools required')
    def test_saved_mp4_and_mkv_keep_late_stem_on_capture_clock(self):
        # The fixture invokes the same origin helper as replay saving, then writes real
        # packets through libavformat; the generated application starts at capture 5s.
        source = r'''
#include "recorder/dynamic_audio.h"
#include <libavformat/avformat.h>
#include <assert.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 3);
    AVFormatContext *input = NULL, *output = NULL;
    assert(avformat_open_input(&input, argv[1], NULL, NULL) == 0);
    assert(avformat_find_stream_info(input, NULL) >= 0);
    assert(avformat_alloc_output_context2(&output, NULL, NULL, argv[2]) >= 0);
    for(unsigned i = 0; i < input->nb_streams; ++i) {
        AVStream *stream = avformat_new_stream(output, NULL); assert(stream);
        assert(avcodec_parameters_copy(stream->codecpar, input->streams[i]->codecpar) >= 0);
        stream->codecpar->codec_tag = 0;
        stream->time_base = input->streams[i]->time_base;
        av_dict_set(&stream->metadata, "title", i == 0 ? "Video" : "App: late-browser", 0);
        av_dict_set(&stream->metadata, "handler_name", i == 0 ? "Video" : "App: late-browser", 0);
    }
    assert(avio_open(&output->pb, argv[2], AVIO_FLAG_WRITE) >= 0);
    assert(avformat_write_header(output, NULL) >= 0);
    AVPacket *packet = av_packet_alloc(); assert(packet);
    while(av_read_frame(input, packet) >= 0) {
        AVStream *from = input->streams[packet->stream_index];
        AVStream *to = output->streams[packet->stream_index];
        /* Source fixture begins at capture 2s, with audio beginning at capture 5s. */
        const int64_t origin = gsr_dynamic_audio_origin(50, (AVRational){1,25}, from->time_base);
        if(packet->pts != AV_NOPTS_VALUE) packet->pts -= origin;
        if(packet->dts != AV_NOPTS_VALUE) packet->dts -= origin;
        av_packet_rescale_ts(packet, from->time_base, to->time_base);
        packet->pos = -1;
        assert(av_interleaved_write_frame(output, packet) >= 0);
        av_packet_unref(packet);
    }
    assert(av_write_trailer(output) >= 0);
    av_packet_free(&packet);
    avformat_close_input(&input);
    assert(avio_closep(&output->pb) >= 0);
    avformat_free_context(output);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            fixture = compile_fixture(path, source, ['libavformat', 'libavcodec', 'libavutil'])
            original = path / 'source.mkv'
            subprocess.run(['ffmpeg', '-v', 'error', '-nostdin', '-f', 'lavfi', '-i',
                            'color=size=32x32:rate=25:duration=6', '-f', 'lavfi', '-i',
                            'sine=frequency=880:sample_rate=48000:duration=3',
                            '-filter_complex', '[0:v]setpts=PTS+2/TB[v];[1:a]asetpts=PTS+5/TB[a]',
                            '-map', '[v]', '-map', '[a]', '-c:v', 'mpeg4', '-bf', '0',
                            '-c:a', 'aac', '-threads', '1', '-filter_complex_threads', '1',
                            '-copyts', str(original)], check=True)
            for extension in ('mp4', 'mkv'):
                with self.subTest(container=extension):
                    saved = path / f'replay.{extension}'
                    subprocess.run([str(fixture), str(original), str(saved)], check=True)
                    info = json.loads(subprocess.check_output(['ffprobe', '-v', 'error',
                        '-show_streams', '-show_packets', '-of', 'json', str(saved)], text=True))
                    video = next(stream for stream in info['streams'] if stream['codec_type'] == 'video')
                    audio = next(stream for stream in info['streams'] if stream['codec_type'] == 'audio')
                    first_video = next(float(packet['pts_time']) for packet in info['packets']
                                       if packet['stream_index'] == video['index'])
                    first_audio = next(float(packet['pts_time']) for packet in info['packets']
                                       if packet['stream_index'] == audio['index'])
                    self.assertAlmostEqual(first_video, 0.0, delta=0.05)
                    self.assertAlmostEqual(first_audio, 3.0, delta=0.05)
                    self.assertIn('App: late-browser', audio.get('tags', {}).values())
                    # aresample aligns decoded PCM to zero, exposing a shifted onset as
                    # audible samples in the first three seconds rather than packet metadata only.
                    decoded = subprocess.check_output(['ffmpeg', '-v', 'error', '-nostdin',
                        '-i', str(saved), '-map', '0:a:0', '-af', 'aresample=48000:first_pts=0',
                        '-ac', '1', '-f', 'f32le', '-'])
                    import array
                    samples = array.array('f', decoded)
                    first_signal = next(i for i, sample in enumerate(samples) if abs(sample) > 0.01)
                    self.assertAlmostEqual(first_signal / 48000, 3.0, delta=0.06)
                    self.assertGreater(len(samples) / 48000, 5.9)


if __name__ == '__main__':
    unittest.main()
