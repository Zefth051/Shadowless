# Captail Linux / GSR spike

Experimental native Linux application on `experiment/linux-gsr`. This is a
feasibility prototype, not a complete port or release. The Linux executable uses
C++17 and Qt 6 Widgets/Network; it does not use .NET, WPF, Wine, or OBS.

Captail owns an installed `gpu-screen-recorder` process and controls it using
GSR's newline-delimited JSON protocol over a private Unix socket. Actual screen
capture, GPU color conversion, encoding, audio capture, and replay buffering are
performed by GSR. This is process integration of the real GSR pipeline, not a
new FFmpeg screen-capture implementation or an in-process GSR library.

## Build and run

Required: a C++17 compiler, GNU Make, `qmake6`, Qt 6 Core/Gui/Widgets/Network/Test,
GPU Screen Recorder **with IPC support** (tested with 6.1.5), and FFmpeg/ffprobe.
Qt's Wayland platform plugin is needed for a native Wayland window.

From the repository root:

```sh
mkdir -p .qa/linux-build
cd .qa/linux-build
qmake6 ../../linux/Captail.pro
make -j4
./tests/captail-gsr-tests
./app/captail-linux
```

Use `QT_QPA_PLATFORM=wayland ./app/captail-linux` to explicitly select native
Wayland. An X11 Qt session is also possible, but was not tested in this spike.
This prototype has no install step or startup service. It starts with capture
off; press **Start** to begin and **Stop** to finish. Closing the window stops
capture and waits for a pending save.

GSR itself must be correctly installed for the chosen capture method. Direct
monitor capture uses GSR's existing KMS helper and permissions; portal capture
uses the desktop's screen-selection/consent dialog. Captail does not run as root,
change helper capabilities, or alter desktop settings.

## Included

- Captail's dark/mint visual palette, status card, replay/recording selector,
  save action, recent clip cards, and thumbnails, rebuilt with native Qt widgets.
- Existing Captail icon, color tokens, and English strings embedded as data.
  The XAML files are read as XML; no Microsoft UI runtime is loaded.
- Monitor/portal choices and H.264/HEVC/AV1 options filtered by the current
  `gpu-screen-recorder --info` hardware report. No silent CPU fallback.
- Replay in RAM, continuous recording, frame rate, automatic/CBR bitrate,
  MP4/MKV, default system/microphone inputs, and mixed/separate audio tracks.
- Asynchronous capture start/save/stop. Saved paths are accepted after GSR's
  completion reply and a non-empty output file is present.
- Clips open in the system video player. `Ctrl+Shift+F10` saves a replay while
  the Captail window is focused; this is not a desktop-global shortcut.

Settings are separate from Windows: Qt writes
`$XDG_CONFIG_HOME/Captail/LinuxGsrSpike.conf` (normally
`~/.config/Captail/LinuxGsrSpike.conf`). The initial output directory is the
desktop's Videos directory under `Captail`; the microphone is off initially.
Configuration changes require stopping capture first.

## Intentionally outside this spike

The WPF UI is not binary-portable. This native reconstruction preserves a subset
of its design and workflow; it is not feature parity. Windows code remains in
the repository and is not compiled into the Linux target.

The Linux UI does not expose DXGI/WGC, Windows GPU preferences, Windows process
loopback audio, Microsoft Store updates, Windows game hooks, or OBS overlays.
Desktop-global shortcuts, tray behavior, app-specific audio routing, game
detection, recovery/watchdog policy, the embedded player/editor, export/trim,
localization beyond reused English strings, HDR, scaling, packaging, and bundled
GSR are not implemented. Some of these are supported by GSR itself and can be
added to Captail after this experiment.

High frame rates above 60 FPS are experimental. The installed GSR capability
report does not establish real performance or game compatibility. This spike
does not claim FPS/latency superiority over OBS. Replay clip duration can exceed
the requested number of seconds because a decodable clip needs a keyframe.

## Verification

Offline tests run real Unix socket peers and cover capability filtering,
validation, argument translation, mixed/separate audio arguments, fragmented
replies, engine errors, wrong request IDs, malformed replies, timeouts, and
failed process launch.

The live suite **records the selected screen and default system audio**. It
never enables the microphone. To run it, from `.qa/linux-build`:

```sh
CAPTAIL_GSR_LIVE=1 \
CAPTAIL_GSR_SOURCE=DP-1 \
CAPTAIL_QA_OUTPUT="$PWD/../linux-gsr/native" \
./tests/captail-gsr-tests
```

Replace `DP-1` with a source reported by
`gpu-screen-recorder --list-capture-options`. Choosing `portal` requires
interacting with the desktop permission dialog; that path was not tested.
H.264 and HEVC live cases skip codecs the GPU does not advertise. Each case
saves two replays, records during replay, then restarts the same adapter in
continuous mode. One replay save is immediately followed by Stop to exercise
pending-save shutdown. Files are checked with ffprobe and fully decoded with
FFmpeg. Logs and probe JSON remain beside the test clips.

See [the experiment report](../docs/LINUX_GSR_SPIKE.md) for measured results and
the next integration decisions.

## License

Captail source retains the repository's GPL-2.0-or-later license. Qt and other
libraries retain their licenses. GSR is an external GPL-3.0-only program and has
not been copied into this repository or bundled by this experiment. A future
distributable must include the required third-party notices, licenses, and
corresponding source/offers applicable to the actual package.

References: [GSR documentation](https://git.dec05eba.com/gpu-screen-recorder/about/),
the installed `man gpu-screen-recorder` IPC section, and
[Qt local sockets](https://doc.qt.io/qt-6/qlocalsocket.html).
