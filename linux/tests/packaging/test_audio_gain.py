"""Exercise the recorder's real PCM gain and mixed-source counting without hardware."""
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[3]
ENGINE = ROOT / 'linux/third_party/gpu-screen-recorder'


class RecordingAudioGainTests(unittest.TestCase):
    def test_mixed_application_and_microphone_have_independent_capture_inputs(self):
        source = r'''
#include "recorder/audio_input.h"
#include <assert.h>
int main(void) {
    gsr_audio_input inputs[3] = {0};
    inputs[0].type = GSR_AUDIO_INPUT_TYPE_APPLICATION;
    inputs[1].type = GSR_AUDIO_INPUT_TYPE_APPLICATION;
    inputs[2].type = GSR_AUDIO_INPUT_TYPE_DEVICE;
    gsr_merged_audio_inputs track = {0}; track.items = inputs; track.num_items = 3;
    assert(gsr_audio_inputs_num_capture_sources(&track) == 2);
    assert(gsr_audio_inputs_should_use_amix(&track));
    track.num_items = 2;
    assert(gsr_audio_inputs_num_capture_sources(&track) == 1);
    assert(!gsr_audio_inputs_should_use_amix(&track));
    inputs[0].type = GSR_AUDIO_INPUT_TYPE_DEVICE;
    assert(gsr_audio_inputs_num_capture_sources(&track) == 2);
    assert(gsr_audio_inputs_should_use_amix(&track));
    inputs[1].type = GSR_AUDIO_INPUT_TYPE_DEVICE;
    assert(gsr_audio_inputs_num_capture_sources(&track) == 2);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'inputs.c').write_text(source)
            subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-ffunction-sections', '-Wl,--gc-sections',
                            '-I', str(ENGINE / 'include'), str(path / 'inputs.c'),
                            str(ENGINE / 'src/recorder/audio_input.c'), '-o', str(path / 'inputs')], check=True)
            subprocess.run([str(path / 'inputs')], check=True)

    def test_gain_mute_boost_and_invalid_updates(self):
        source = r'''
#include "recorder/audio_gain.h"
#include <assert.h>
#include <unistd.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    gsr_audio_gains gains = {1.0, 1.0, 1.0, 1.0};
    FILE *file = fopen(argv[1], "w"); assert(file);
    fputs("0.25 5\n", file); fclose(file);
    assert(gsr_audio_gain_read(argv[1], &gains));
    assert(gains.system == 0.25 && gains.microphone == 5.0);
    float left[] = {0.8f, -0.4f}, right[] = {0.4f, -0.8f};
    AVFrame frame = {0}; frame.format = AV_SAMPLE_FMT_FLTP;
    frame.nb_samples = 2; frame.ch_layout.nb_channels = 2;
    uint8_t *planes[] = {(uint8_t*)left, (uint8_t*)right}; frame.extended_data = planes;
    gsr_audio_gain_apply(&frame, gains.system);
    assert(fabs(left[0] - 0.2f) < 0.00001 && fabs(right[1] + 0.2f) < 0.00001);
    gsr_audio_gain_apply(&frame, 0.0);
    assert(left[0] == 0 && left[1] == 0 && right[0] == 0 && right[1] == 0);
    int16_t samples[] = {20000, -20000, 1000, -1000};
    frame.format = AV_SAMPLE_FMT_S16; planes[0] = (uint8_t*)samples;
    gsr_audio_gain_apply(&frame, gains.microphone);
    assert(samples[0] == 32767 && samples[1] == -32768 && samples[2] == 5000 && samples[3] == -5000);
    int32_t wide[] = {1000000000, -1000000000, 100, -100};
    frame.format = AV_SAMPLE_FMT_S32; planes[0] = (uint8_t*)wide;
    gsr_audio_gain_apply(&frame, gains.microphone);
    assert(wide[0] == INT32_MAX && wide[1] == INT32_MIN && wide[2] == 500 && wide[3] == -500);
    float interleaved[] = {0.8f, -0.8f, 0.4f, -0.4f};
    frame.format = AV_SAMPLE_FMT_FLT; planes[0] = (uint8_t*)interleaved;
    gsr_audio_gain_apply(&frame, gains.system);
    assert(fabs(interleaved[0] - 0.2f) < 0.00001 && fabs(interleaved[3] + 0.1f) < 0.00001);
    file = fopen(argv[1], "w"); fputs("nan 1\n", file); fclose(file);
    assert(!gsr_audio_gain_read(argv[1], &gains));
    assert(gains.system == 0.25 && gains.microphone == 5.0);
    file = fopen(argv[1], "w"); fputs("1\n", file); fclose(file);
    assert(!gsr_audio_gain_read(argv[1], &gains));
    file = fopen(argv[1], "w"); fputs("-1 20\n", file); fclose(file);
    assert(!gsr_audio_gain_read(argv[1], &gains));
    assert(gains.game == 1.0 && gains.music == 1.0);
    file = fopen(argv[1], "w"); fputs("0.25 5 0.4 0\n", file); fclose(file);
    assert(gsr_audio_gain_read(argv[1], &gains));
    assert(gsr_audio_role_gain("PC", true, false, &gains) == 0.25);
    assert(gsr_audio_role_gain("Game", true, false, &gains) == 0.4);
    assert(gsr_audio_role_gain("Music", true, false, &gains) == 0.0);
    assert(gsr_audio_role_gain("Microphone", true, true, &gains) == 5.0);
    assert(gsr_audio_role_gain("Game", false, false, &gains) == 0.25);
    assert(gsr_audio_role_gain("Game", false, true, &gains) == 5.0);
    file = fopen(argv[1], "w"); fputs("0.25 5 0.4\n", file); fclose(file);
    assert(!gsr_audio_gain_read(argv[1], &gains));
    file = fopen(argv[1], "w"); fputs("0.25 5 2 0.8\n", file); fclose(file);
    assert(!gsr_audio_gain_read(argv[1], &gains));
    assert(gains.game == 0.4 && gains.music == 0.0);
    unlink(argv[1]); assert(!gsr_audio_gain_read(argv[1], &gains));
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'gain.c').write_text(source)
            flags = shlex.split(subprocess.check_output(['pkg-config', '--cflags', '--libs', 'libavutil'], text=True))
            subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-I', str(ENGINE / 'include'),
                            str(path / 'gain.c'), '-o', str(path / 'gain'), *flags, '-lm'], check=True)
            subprocess.run([str(path / 'gain'), str(path / 'levels')], check=True)


if __name__ == '__main__':
    unittest.main()
