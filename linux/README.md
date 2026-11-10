# Shadowless for Linux

Native C++17 / Qt 6 application for **Hyprland on Wayland**. Shadowless owns its
control overlay, settings, library, and media-layer editor. An external
`gpu-screen-recorder` process performs capture/encoding/replay buffering; a
Shadowless plugin composites media layers.
The initial capture experiment is documented in `docs/LINUX_GSR_SPIKE.md`.

## Build and run

Required: GNU Make, a C++17 compiler, qmake6, Qt 6 Core/Gui/Widgets/Network/DBus/
Concurrent/Test, the Qt SVG image plugin, the native Qt Wayland plugin, LayerShellQt (tested 6.8.2), OpenGL
headers, FFmpeg development libraries (`libavformat`, `libavcodec`, `libavutil`,
`libswscale`), FFmpeg/ffprobe executables, and GSR with IPC/plugin support
(tested 6.1.5, including `/usr/include/gsr/plugin.h`).
Desktop and microphone capture also use PipeWire, pipewire-pulse, `pw-loopback`
and `pactl`.

```sh
./linux/build.sh
./linux/run.sh
./linux/run.sh --background
```

`build.sh` builds the app, plugin, and offline tests under `.qa/linux-build`.
Set `SHADOWLESS_BUILD_DIR` to select another output directory. An installed
LayerShellQt is used normally; `SHADOWLESS_LAYERSHELL_PREFIX=/path/to/prefix` selects
a development installation with `include/` and `lib/`. An existing local
`.qa/deps/usr` development prefix is detected automatically. Dependencies are
not downloaded or installed by the build script. This target does not build
the Windows solution.

Closing/hiding the panel keeps the application and capture running. Use the
tray menu or `./linux/run.sh --quit` to exit cleanly. Capture starts off.

## Controls and configuration

The dashboard follows the Windows fork's two-column layout. Capture, audio,
and the composition switch are on the left, and recent clips are on the right.
The video summary and save path open their respective settings. Recent clips
open in the desktop's default video player. Directory scans run off the UI
thread and unchanged clips reuse thumbnails while Shadowless is running.

- Alt+Z: toggle the overlay. Escape returns from a subpage, then hides it.
- Ctrl+Shift+F10: save the last ten seconds (or the smaller configured buffer).
- Alt+F10: start/stop capture. The dashboard also selects replay or recording.
- **Start recording** appears while replay is active. GSR saves the recording
  and replay clips from the same capture process. Stopping capture finalizes
  an active recording.
- Settings exposes configurable shortcuts, capture options, and the optional
  passive status indicator.
- `--toggle`, `--show`, `--hide`, `--save`, `--capture`, and `--quit` address the
  running instance. A second process does not create a second recorder.

Shortcuts register through the desktop portal. Shadowless installs temporary
Hyprland bindings using the current Lua API or the legacy keyword API. Existing
conflicting bindings are preserved and reported in Settings. Bindings owned by
Shadowless are removed on clean exit and restored after compositor config reload.
No unrelated Hyprland config file is edited. The first launch creates a hidden
`~/.local/share/applications/io.github.zefth051.Shadowless.desktop` identity required
by the portal; it does not enable autostart.

Settings are saved to `$XDG_CONFIG_HOME/Shadowless/Shadowless.conf` (normally
`~/.config/Shadowless/Shadowless.conf`). On first launch, settings from the
earlier Captail Linux build are copied if present. Existing clip paths are
preserved. New installations default to Videos/Shadowless. Microphone capture
is off by default.

Video settings offer FPS and bitrate presets and a Custom field for each.
The mouse wheel scrolls the settings page without changing control values.
Use **Appearance → Accent color** to select a color, then save to apply it.

## Audio

Open **Audio settings** to select the desktop output and microphone. Default
devices are resolved when capture starts. Change routing with capture stopped.
Desktop and microphone can mix into one track or use separate tracks.

**Selected applications** captures the apps assigned to tracks in the table.
Start playback in an app, then refresh to find it. Sources on the same track
mix, unused tracks are omitted, and the microphone can use any track. Choose
AAC or Opus and an automatic or fixed audio bitrate.

The dashboard adjusts volume and mute during capture. These controls affect
the private recording routes, including the microphone in application mode.
Microphone boost is configurable up to 20 dB.

## Capture overlays

Open **Capture overlays**, browse for local media or enter an absolute path,
and add it. PNG, JPEG, BMP, WebP, GIF, MP4, WebM, MOV, and MKV are accepted.
Animated images/videos loop silently; only their video frames are decoded.
The top list item draws above later layers. Enable composition with the master
switch, then use visibility, Up/Down, opacity, position, and fit-box controls.
Drag the selected box to move it. Position percentages describe available travel:
100% aligns the layer to the right/bottom edge. Fit boxes preserve media aspect
ratio and remain inside the canvas. There are at most 16 layers.

Edits apply after a short debounce and save after successful validation/application.
A failed asset load preserves the previous recorded scene. Existing buffered
frames and clips are never rewritten. Layers continue in captured frames while
the panel is hidden. The editor shows a labeled 16:9 composition canvas, not a
live desktop preview. Closing the editor releases preview media and its timer.

Static media decodes once and reuses its GPU texture. Animation decoding runs
on a worker; render callbacks consume frame snapshots without file reads or
blocking on decoder locks. Layer frames are capped at 1920x1080 to bound upload
and cache costs. Larger/animated assets and multiple layers still cost resources.

## Status indicator and current limits

The passive indicator is experimental and **disabled by default**. When enabled,
it shows buffering, recording, and replay-save state in a small surface with no
keyboard or pointer input. It updates on state changes, without animation or a
per-frame timer. It is separate from the recorded media-layer composition.

GSR 6.1.5's plugin path cannot draw certain external cursor textures. This
preview explicitly disables recording the mouse cursor when loading the Shadowless
plugin. This does not disable mouse input. Monitor capture can include the open
control overlay and status indicator; exclusion is not promised.

Only Hyprland is targeted. KDE/GNOME/X11 compatibility, HDR composition, webcam
layers, game hooks, packaging, and the Windows video editor remain outside this
preview. Fullscreen application tests do not establish compatibility with every game.

## Verification

`build.sh` runs the GSR offline suite, layer/media/socket tests, and offscreen
UI/controller regression tests. UI tests isolate settings and disable their
session bus; expected offscreen/DBus warnings do not establish compositor support.
Existing GSR live tests remain opt-in and include system audio.

The separate composition test **records the selected monitor without audio**:

```sh
python linux/tests/live_overlays.py \
  --plugin .qa/linux-build/plugins/libshadowless-overlay.so \
  --source DP-1 --output "$PWD/.qa/overlay-live"
```

It compares baseline, disabled plugin composition, static, animated, and continuous
recording cases; checks fixture position/orientation and full FFmpeg decoding;
and stores short recorder CPU/RSS measurements beside clips. CPU is expressed
as a percentage of one core. It does not measure game FPS or GPU execution time.
See `docs/LINUX_HYPRLAND_OVERLAY.md` for actual coverage and remaining checks.

## License

Shadowless source retains GPL-2.0-or-later. Qt, LayerShellQt, FFmpeg and other
libraries retain their respective licenses. GSR remains an external GPL-3.0-only
program; its source was not copied into this implementation. The LayerShellQt
development package in ignored `.qa/` is not a distributable. Review notices,
licenses, and source requirements when preparing an actual package.
