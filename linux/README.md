# Shadowless for Linux

Native C++17 / Qt 6 application for **Hyprland and Plasma 6 on Wayland**. Shadowless owns its
control overlay, settings, library, and media-layer editor. A bundled, source-built
`gpu-screen-recorder` process performs capture/encoding/replay buffering; a
Shadowless plugin composites media layers.
The initial capture experiment is documented in `docs/LINUX_GSR_SPIKE.md`.
Release 0.3.4 includes the Plasma startup and shortcut implementation. Live Plasma capture, layer surfaces, tray and shortcut approval still
need validation in a real Plasma session.

## Build and run

Required: GNU Make, a C++17 compiler, qmake6, Qt 6 Core/Gui/Widgets/Network/DBus/
Concurrent/Test/Multimedia/MultimediaWidgets/OpenGLWidgets, libmpv development files,
the Qt SVG and WebP image plugins (`qt6-svg` / `qt6-imageformats` on Arch), the native Qt Wayland plugin, LayerShellQt (tested 6.8.2), OpenGL
headers, FFmpeg development libraries (`libavformat`, `libavcodec`, `libavutil`,
`libswscale`), FFmpeg/ffprobe executables, Meson, Ninja, patchelf, Vulkan headers,
and the GSR development dependencies in its Meson file (X11 extensions, Wayland,
libdrm, libva, libcap, PulseAudio, PipeWire and D-Bus). No GSR package is needed:
the recorder, helper and matching plugin header come from the maintained
6.1.7 source in `linux/third_party/gpu-screen-recorder/`.
Audio capture uses PipeWire and pipewire-pulse. `pactl` supplies application icons;
no loopback command or virtual audio device is required.

```sh
./linux/build.sh
./linux/run.sh
./linux/run.sh --background
```

`build.sh` builds the recording engine, app, plugin, and offline tests under `.qa/linux-build`.
Set `SHADOWLESS_BUILD_DIR` to select another output directory. An installed
LayerShellQt is used normally; `SHADOWLESS_LAYERSHELL_PREFIX=/path/to/prefix` selects
a development installation with `include/` and `lib/`. An existing local
`.qa/deps/usr` development prefix is detected automatically. Dependencies are
not downloaded or installed by the build script. This target does not build
the Windows solution.

Closing/hiding the panel keeps the application and capture running. Use the
tray menu or `./linux/run.sh --quit` to exit cleanly. Capture starts off.
See [startup settings](../docs/STARTUP.md) for the independent login and replay options.

Shadowless always launches its own engine, even if another GSR is installed.
No GSR graphical interface or service is included. Direct KMS capture can request
administrator authentication through Polkit for the matching helper only. The
launcher stages that helper and its private libdrm outside AppImage's FUSE mount,
then removes them on exit. It does not run Shadowless as root or install permanent
capabilities. When this helper requests Polkit authorization, the control overlay
hides so the system prompt can receive input, then restores its previous
visibility after authorization or failure. Background startup stays hidden.
Portal capture and direct capture with existing privileges do not trigger this
handoff. Engine changes are released together with Shadowless; provenance,
local modifications and build input hashes are in `linux/third_party/`.

## Controls and configuration

The dashboard follows the Windows fork's two-column layout. The Instant Clip and
Recording buttons start/stop capture directly and reflect its actual state.
Capture overlays and audio sit below them; recent clips are on the right.
The video summary and save path open their respective settings. Recent clips
open inside Shadowless, with hardware-assisted libmpv playback, play/pause, seeking,
volume/mute, selected audio-track mixing, speeds, recent-clip navigation, and fullscreen.
The progress bar seeks directly where clicked. Dragging previews the target time,
pauses playback until release, and resumes only if the clip was playing before the drag.
Each recent clip has a Trash button available before playback. Deletion requires
confirmation and moves only that clip to the desktop Trash; failures leave the
file in place and show an error.
The player sidebar reuses library thumbnails and finishes pending thumbnails while open.
Its game-folder and recording-mode filters use the complete scanned library,
independently of the dashboard's 20 recent clips. Navigation follows the filtered
list; filtering keeps the current playback and trim state. Unknown/imported
filenames appear under Other clips. Audio waveforms use dark surfaces and readable
accent colors, including silence and dark custom accents.
Returning to the dashboard or hiding the panel stops playback. Directory scans run off the UI
thread and unchanged clips reuse thumbnails while Shadowless is running.

**Settings → Storage → Organize clips into folders by game** optionally groups new
replays and recordings by game (off by default). Existing clips are left in place.
The library scans nested folders and offers **All games**, individual game folders,
and **Desktop / Unidentified** filters, showing the newest 20 clips in the selected
collection. Detection queries the focused Hyprland client only when saving a replay
or starting a recording. It uses the bundled Windows executable catalog (including
Wine/Proton process metadata) and native Steam window identities. Unknown or conflicting identities
stay in the main folder; no game hooks or continuous detection are installed. Plasma currently saves to
the main folder because reliable focused-game detection is unavailable there.

Saving shows a rounded passive card with a status icon and distinct title/detail,
even with the dashboard hidden, and plays
the same confirmation sound as Windows only after the recorder confirms the file.
The overlay has no keyboard interactivity, pointer input region, activation,
animation or periodic polling. It does not restore or open the dashboard. Failed
saves show an error without playing the sound. If organization fails, the saved
file is preserved in the main folder. Monitor capture may include this feedback.

The player supports Space (play/pause), Left/Right (seek five seconds), Up/Down
(speed), M (mute), Page Up/Page Down (newer/older clip), and F or a double click
(fullscreen). Escape first exits fullscreen. Fullscreen controls hide after 2.4
seconds of inactivity while playing. Playback owns a separate decoder from GSR,
limits its demux buffers, and stops/releases its clip when the panel is hidden.
Playback audio uses the existing pipewire-pulse/PulseAudio interface to avoid a
native PipeWire initialization deadlock in libmpv 0.41; see the
[playback backend note](../docs/LINUX_PLAYBACK.md).
Fullscreen video fills the player, with auto-hiding controls overlaid at the bottom.
Showing or hiding the controls keeps the video size fixed. Space pauses or resumes
playback even when a transport button has focus.

**Edit clip** loads timeline thumbnails and per-track waveforms only on request.
Editing expands the panel to nearly fill the screen. Opening a clip or returning
from the dashboard starts in playback mode, with editing off.
Analysis and export run one low-priority FFmpeg process at a time, with one
decoder/filter thread. Hiding the player cancels analysis and releases playback,
but an active export continues in the background. Opening another clip waits
until export finishes. Exiting Shadowless or using **Cancel job** cancels the job.
The control overlay displays **rendering...** on every page while exporting,
including after hiding and reopening it. Completion, cancellation and failure
clear this status.
Drag timeline handles or use In/Out and I/O to choose a range.
The Start/End fields accept `mm:ss.mmm` or a number of seconds; arrow keys
adjust by 0.1 seconds. Audio rows use full-area activation buttons and keep their
waveforms in place when deselected. Up to four track controls fit without
scrolling; additional recorded tracks remain available by scrolling.
Playback and seeking stay within the selected range while editing. Playback pauses
at the out point; pressing Play there starts again at the in point.
Export keeps selected audio tracks separate unless **Merge selected audio tracks into one** is checked.
Video and separate audio are copied without re-encoding; stream-copy cuts can
retain content from the preceding keyframe. Exporting to an existing path requires
confirmation. A temporary output must pass metadata and decoding validation before
atomically replacing the destination; failure/cancellation preserves the original.

- Alt+Z: close the overlay from any page or reopen the dashboard. Escape returns
  from a subpage, then hides it.
- Ctrl+Shift+F10: save a replay using **Settings → Instant Replay → Clip duration**.
  The dashboard and tray use the same duration.
- Alt+F10: start/stop capture using the last mode.
- **Instant Clip** starts/stops replay buffering; **Save** saves the configured
  duration. **Recording** starts/stops a continuous recording, including while
  Instant Clip remains active. GSR saves both from the same capture process.
  Stopping Instant Clip also finalizes any active recording. A recording started
  alone must be stopped before enabling Instant Clip.
- Settings exposes configurable shortcuts, capture options, and the optional
  passive status indicator.
- `--toggle`, `--show`, `--hide`, `--save`, `--capture`, and `--quit` address the
  running instance. A second process does not create a second recorder.

Shortcuts register through the desktop portal using the configured triggers.
On Plasma, the desktop approves bindings and Settings reports the assigned
shortcuts. The control overlay temporarily hides during approval so it cannot cover
the desktop dialog, then restores its previous visibility. Changing keys creates a
fresh portal session. Shortcuts reconnect automatically after a portal restart.
On Hyprland, Shadowless installs temporary
Hyprland bindings using the current Lua API or the legacy keyword API. Existing
conflicting bindings are preserved and reported in Settings. Bindings owned by
Shadowless are removed on clean exit and restored after compositor config reload.
No unrelated Hyprland config file is edited. Legacy Captail Lua bindings are
removed when no other binding shares their key combination, so old registrations
cannot keep a previous shortcut reserved. The first launch creates a hidden
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

**Automatic application tracks** is the normal desktop-audio mode. It captures
applications across output devices and keeps discovering sources during replay
and recording. Applications appear on the dashboard when they first produce
audio; opening one later does not restart the video encoder or clear the buffer.
Sources sharing an application audio identity share a track, including browser
tabs that use the same identity.

The dashboard gives each discovered source **Track** and **Game** controls.
**Track** chooses its default inclusion in saved clips; the independently buffered
source remains available in the editor. **Game** moves that application's audio
out of PC and into Game. Known catalog names are suggested automatically; use
Game to correct an unidentified game or application. Choices persist between
launches. Closed or quiet applications retain their identity for the session.

The editor presents **PC**, **Game** and **Microphone** groups for new automatic
captures. PC combines non-game applications and any unidentified desktop audio;
Game is excluded from PC. **More tracks** reveals actual source tracks. Expanding
or collapsing does not change playback or export selection. Group and individual
selectors control the same physical streams, so an application is never mixed
twice. A group with no recorded source is disabled. Existing mixed/two-track files
retain their real tracks and cannot be separated retroactively.

Microphone capture remains opt-in and uses the selected input device, including
system-default following. PC/Game/Microphone volume and mute are independent;
microphone boost supports up to 20 dB. Gain is applied only to capture PCM before
encoding. Playback volume, real device levels and default devices are unchanged;
Shadowless creates no virtual input/output devices and needs no pw-loopback.

Explicit **Selected applications** and **Four tracks: PC / Game / Music /
Microphone** routing remain available as advanced modes. They preserve existing
assignments. Automatic mode does not use the old physical output selector or
system/microphone mixing option; these values are retained for legacy modes.
AAC/Opus and automatic/fixed bitrate settings still apply.

Application sources use the shared capture clock, retaining silence before a
late source begins. Replay buffers reserve capacity for up to 64 application
identities plus PC remainder and microphone; exceeding capacity is reported,
not silently truncated. The editor accepts up to 128 audio streams and reports
larger files explicitly. Real PipeWire/device synchronization still needs testing
on the target desktop.

Continuous recordings keep private audio stems under a private temporary directory
and finalize them into the requested MP4/MKV without re-encoding video. Saving a
simultaneous recording runs in the background while replay continues. The stop
watchdog follows actual finalization progress rather than a fixed total duration.
Failed writes/finalization preserve the base recording and stems and report
failure; the recovery paths are included in recorder diagnostics. Extra disk space
is required until successful finalization completes.

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
the panel is hidden. The dashboard previews custom layers. While capture runs, the
editor's **Live capture + overlays** view shows the full captured source beneath
editable layers, with its actual aspect ratio. With composition disabled, this
view shows the capture without layers, matching new recorded frames. **Layers only**
isolates the media layers for editing and stops capture-preview requests; switching
views does not change the recorded scene. The preview uses a separate private socket, requests one small frame
at a time at up to 4 FPS, and uses asynchronous GPU downscale/readback. It also
works with composition disabled or no layers. Closing the editor stops requests
and releases preview media/timers. With capture off, the editor explicitly shows
that state and still allows layer editing; it never starts recording implicitly.

Static media decodes once and reuses its GPU texture. Animation decoding runs
on a worker; render callbacks consume frame snapshots without file reads or
blocking on decoder locks. Layer frames are capped at 1920x1080 to bound upload
and cache costs. Larger/animated assets and multiple layers still cost resources.

## Status indicator and current limits

The passive indicator is experimental and **disabled by default**. Settings offers
Left up, Right up, Left down and Right down positions; the choice is saved. When enabled,
it shows buffering, recording, and replay-save state in a small surface with no
keyboard or pointer input. It uses a compact animated ring and center dot, with a check mark after a
successful save. Animation stops while the indicator is hidden. It is separate from the recorded media-layer composition.

GSR 6.1.5's plugin path cannot draw certain external cursor textures. Shadowless
passes `-cursor no` whenever it loads the composition plugin. The controller always
loads that plugin for composition and live preview, so this also excludes the cursor
when composition is disabled or the scene has no layers. This does not disable mouse input. Monitor capture can include the open
control overlay and status indicator; exclusion is not promised.

Hyprland and Plasma 6 Wayland are targeted. GNOME/X11 compatibility, HDR
composition, webcam layers and game hooks remain outside this preview. AppImage packaging is available
for local/private staging; see [the packaging guide](packaging/README.md) and
[installation instructions](../docs/INSTALL.md). Native player/editor
controls follow the Windows workflow; Windows runtime code is not used. Fullscreen
application tests do not establish compatibility with every game.

## Verification

`build.sh` runs the GSR offline suite, layer/media/socket tests, and offscreen
UI/controller regression tests, plus native playback, media export, preview, and
desktop identity checks. Media export tests compare copied video packet hashes and
decode generated multi-track fixtures. UI tests isolate settings and disable their
session bus; expected offscreen/DBus warnings do not establish compositor support.
The shortcut migration test also executes its generated commands when a `lua`
interpreter is available; that execution check is skipped otherwise.
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

## Advanced four-track mode and recording pause

In Settings → Audio, select **Four tracks: PC / Game / Music / Microphone**.
Assign the game application to Game and the music application to Music, and
enable the microphone. New captures then contain PC (1), Game (2), Music (3)
and Microphone (4). PC contains other applications, excluding the selected game
and music sources. Selected applications may be quiet or not running yet; their
streams remain separate. Existing two-track or mixed files cannot be separated
retroactively. The editor retains independent track selection and export.

Standalone continuous recording exposes **Pause recording / Resume recording**.
Resume continues the same file. The control waits for the recorder to confirm
the change; Stop remains available if confirmation fails. GSR does not support
pausing only the recording while Instant Clip is also running, so that mode does
not expose Pause.

## Discord Rich Presence

With the Discord desktop client running, Shadowless shows `Clipping "Game"`
(or Recording / Recording paused), with the Shadowless logo and a small game
badge when artwork is available. There is no elapsed-time line and unchanged
activities are not republished. Game detection runs independently of folder
organization. Hyprland uses the focused game; Plasma matches current-user game
processes against the bundled catalog, including Wine/Proton executables.
Ambiguous or unknown games show only the capture state, without a guessed title.
The Shadowless artwork uses a public image pinned to a repository commit. Game
artwork is resolved once per session from the same public, revision-pinned source
as the embedded game catalog, with a 10-second timeout and 16 MiB response limit.
The icon catalog is optional: offline or missing artwork never blocks capture.
Only catalog game names and public image URLs are sent; no unrelated window
titles, clip filenames, audio applications or recording paths are included.
The public Application ID requires no user token or configuration. Discord may
be closed or restarted without interrupting capture. Actual client display still
needs validation; protocol tests use isolated peers.

## Appearance

Settings → Appearance offers the existing named presets and **Custom HEX**. Enter
a six-digit color such as `#39a7f0`; invalid values cannot be saved. The accent is
preserved across launches, with readable text for normal, hover and pressed states.
The dashboard uses compact capture cards and clip names adapt to the available
width while playback, containing-folder and Trash actions remain accessible.

## Updates and AI development

Public-enabled AppImages check `linux-vX.Y.Z` releases at startup. Use the tray
menu to check manually. Downloads require confirmation and a valid GitHub SHA-256
digest. HTTP failures and TLS verification failures are reported distinctly and logged.
For an opt-in real HTTPS discovery/download/checksum/install test against an
isolated temporary AppImage, run
`SHADOWLESS_TEST_PUBLIC_UPDATES=1 .qa/linux-build/tests/updates/shadowless-update-tests publicRuntimeUpdateRoundTrip`.
The writable AppImage is replaced atomically; reopen Shadowless after
recording is finished to use the update. Private/development builds update manually.
The tray About entry and [AI disclosure](../docs/AI_DEVELOPMENT.md) identify
Shadowless as developed with AI assistance, retaining Captail attribution.

## License

Shadowless source retains GPL-2.0-or-later. Qt, LayerShellQt, FFmpeg and other
libraries retain their respective licenses; the local FFmpeg build is GPL-3.0-only,
so the Linux bundle needs a GPLv3-compatible distribution assessment. The bundled GSR
engine is GPL-3.0-only; its complete modified source, license and build inputs are
included in the AppImage's documentation directory. The LayerShellQt
development package in ignored `.qa/` is not a distributable. Review notices,
licenses, and source requirements when preparing an actual package.
