# Shadowless recording engine

Upstream: https://repo.dec05eba.com/gpu-screen-recorder
Version: 6.1.7
Commit: `1d4946a16139b446e6308f741e9ba99f2ed5b3a7`
Unmodified `git archive HEAD` SHA-256: `a610fb0e5d4119d7f6c30c5c38f19e4017fdc168f7244746de5b6cc6d38a067d`
License: GPL-3.0-only; see `gpu-screen-recorder/LICENSE` and upstream copyrights.

The full upstream source at that commit is kept in `gpu-screen-recorder/`.
`GSR_SOURCES.sha256` records the imported tree including the local patch below;
the build verifies every file before compilation. Update that manifest deliberately
when changing our engine source. Git history records the reviewed changes.
Shadowless builds only the recorder and KMS helper, not `gsr-cli`, services,
capability installation hooks, driver configuration or any graphical GSR project.
The overlay plugin uses the plugin header from this exact tree.

Local modification: `kms/client/kms_client.c` accepts the absolute
`SHADOWLESS_GSR_KMS_SERVER` path supplied by our launcher and refuses to search
PATH for an unrelated helper. The launcher stages the matching helper and private
libdrm outside the user-only AppImage mount for Polkit, preserves signal forwarding,
and removes staged files after the recorder exits. `src/cli/main.c` and
`kms/server/kms_server.c` call `include/shadowless_parent.h` to bind their lifetime
to their parent using Linux PR_SET_PDEATHSIG/SIGKILL with a parent-race check.
The launcher supplies its PID so the recorder also rejects an already-reparented
start under a subreaper.
This prevents a forcibly killed launcher from leaving recording or a privileged
helper running. Capture stages helper files inside its owned runtime directory;
the parent cleans them after process exit, including a hard stop. Standalone
engine invocations use `/tmp`; SIGKILL of the standalone launcher may leave that
temporary directory, but cannot leave its recorder/helper capturing.
No permanent capabilities are set.

Build inputs: `linux/engine/build.sh`, upstream Meson files, system development
dependencies recorded in release `BUILD_PACKAGES.txt`; shared library hashes appear
in the bundle manifest. Future engine patches belong in this source tree, with
this provenance updated and recording regressions rerun.

The KMS Polkit branch emits `shadowless-kms-auth: begin` immediately before
`pkexec`, and `shadowless-kms-auth: end` after the helper connects. Shadowless
uses these markers to release its overlay during required authentication.
Direct privileged/capability launches and portal capture emit neither marker;
failed authentication is cleared when the recorder exits. No authorization
policy or credential handling is added.

Audio track titles are also written as `handler_name` in `src/recorder/recorder.c`
and `src/recorder/muxer.c`, preserving named application tracks in MP4 as well as
Matroska for recording and replay saves. Application capture retains upstream PipeWire inclusion/exclusion links to private
recording streams. `audio_input.c`, `audio_capture.c`, their headers and
`include/recorder/audio_gain.h` keep microphone/device PCM separate from the
application stream until gain is applied and then mix when requested. The app
atomically writes linear PC/system, microphone, Game and Music gains to its private runtime file;
`SHADOWLESS_AUDIO_LEVELS_FILE` and `SHADOWLESS_MICROPHONE_SOURCE` select that file
and microphone role. `SHADOWLESS_FOUR_TRACK_AUDIO=1` enables independent Game/Music
gain selection by the named track; other modes retain system gain for all
application audio. The first two gain values remain compatible with older engine
or app versions; absent Game/Music gains default to unity. Each capture thread
refreshes gains at most every 60 ms.
Malformed updates retain the preceding values. No virtual Audio/Source devices
are created and no real device or playback volume is changed. The hardware-free
PCM and mixed-input regressions are in `tests/packaging/test_audio_gain.py`; live
four-source isolation remains an opt-in hardware test.

Dynamic desktop audio (`SHADOWLESS_DYNAMIC_AUDIO=1`) uses stable application
tracks plus an actual unassigned desktop remainder and separate microphone.
There are at most 64 application identities, with two additional slots reserved
for remainder and microphone. Late PipeWire enumeration and source/codec opening
run on a worker that owns one unpublished track; the capture thread registers
completed tracks without restarting video. Shutdown joins that worker before
track/encoder teardown. Replay saves snapshot tracks and codec parameters, map
stable source IDs to contiguous output streams, and preserve the video clock
origin for late/reopened sources. Replay retention also expires old packets when
the reserved late-source capacity has not yet filled.

Continuous/simultaneous recording writes application stems into private spool
files and performs final remux on a worker. Only validated successful remux
replaces the recording and removes stems. Packet write failures remain sticky
per destination, even if later packets/trailer/close succeed; failure retains
recording and spools. `shadowless-audio-finalizing: progress` is emitted when
finalization begins and at most every two seconds while packet copying advances,
allowing the launcher to distinguish progress from a stalled finalization.
Atomic application choice JSON, structured discovery events and App/Game track
titles follow the dynamic-audio plan. Hardware-free lifecycle, failed-write,
retention and real MP4/Matroska media regressions live in
`tests/packaging/test_dynamic_audio.py`; live PipeWire graph and device timing
validation remains required on a desktop audio server.
