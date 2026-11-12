# Linux playback audio backend

The clip player uses libmpv with its PulseAudio output backend. This works with
the existing PipeWire/pipewire-pulse prerequisite and with a PulseAudio server;
capture and audio routing continue to use their existing paths. Playback audio
is not disabled or replaced by an untimed null output in tests.

libmpv 0.41's native PipeWire backend can deadlock after failing to connect to a
PipeWire server. Audio-track reloads make repeated initialization more likely.
The player then hangs when leaving the clip: `mpv_render_context_free()` waits
for the mpv core, which is stuck in `pw_thread_loop_stop()` during audio cleanup.
An isolated CI session with a PulseAudio server and no native PipeWire server
exposed this race; the same scenario reproduced locally.

Upstream fixed the initialization ordering in
[mpv commit 1a78f9fa8e6903bb51c9e43eefd64456a880ed13](https://github.com/mpv-player/mpv/commit/1a78f9fa8e6903bb51c9e43eefd64456a880ed13).
Until native PipeWire output is needed and the bundled libmpv includes that fix,
using the already-required PulseAudio interface avoids this failure without
changing rendering teardown or extending test timeouts.

`PlaybackTests::usesPulseAudioForPlayback` checks actual libmpv backend-selection
diagnostics while decoding a clip. `UiTests::playerAudioEditorAndPickerLifecycle`
continues to exercise track selection, editing, the export picker, and leaving
the player. The regression was also stress-tested with an unavailable native
PipeWire endpoint and a functioning PulseAudio endpoint.
