# Native Captail overlay for Hyprland

Status: approved by the user on 2026-11-10; implement locally.

## Intent

Bring Captail's Windows control dashboard, settings, and capture overlays to the
existing native C++17/Qt 6 Linux application. Prioritize Hyprland on Wayland.
GPU Screen Recorder UI is a behavioral and technical reference; Captail owns
its interface and configuration. The existing external GSR capture process
remains the recording backend. Renaming the project to Shadowless is deferred.

The user explicitly requests access while a game is running, capture overlays
and their settings, and a cleaner code organization than the Windows version.
The approval also requests performance measurements and an optional passive
capture/save indicator. Keep the indicator disabled by default and defer enabling
it if it causes meaningful overhead or input/window problems.

## Approaches

1. **Native Qt overlay through Wayland layer-shell, Hyprland global shortcuts,
   and a Captail GSR composition plugin. Recommended.** Builds on the current
   native application and separates desktop interaction from recorded content.
   Requires a Wayland shell integration and an independently validated plugin.
   The user selected Hyprland first after the native layer-shell proposal;
   the composition mechanism is an engineering proposal, not yet user-approved.
2. **Qt through XWayland with window-manager rules.** Could retain Qt widgets,
   but adds dependence on X11 window behavior, compositor rules, and focus
   workarounds. Does not by itself provide recorded media composition. Less
   suitable for the requested native Hyprland-first application.

Reusing GSR's own dashboard or replacing the capture backend with OBS is outside
the requested approach. Porting WPF binaries is not possible in this native target.

## User-facing behavior

- Alt+Z toggles Captail's control overlay on the active monitor, including over
  fullscreen games. It opens with the existing dark/mint Captail appearance,
  translucent backdrop, replay/recording controls, library, and settings access.
- The visible panel receives keyboard and pointer input. Escape first exits
  an editor/settings subpage, then hides the dashboard. Hiding returns input to
  the game and never stops capture. Explicit Quit retains pending-save shutdown.
- Captail remains available in the background. A tray entry provides Open and
  Quit; a command to toggle the existing process provides a recovery path.
- Shortcut conflicts are reported. Never silently claim a binding succeeded.
  The first target uses Hyprland's global-shortcut protocol, not raw input grabs
  or privileged keyboard helpers. The hotkey is configurable in Captail, with
  a narrow Hyprland binding adapter if required by the protocol's capabilities.
  Do not rewrite unrelated Hyprland configuration. Provide a concrete binding
  instruction if the compositor requires user-owned configuration.
- Capture settings retain capability filtering, replay/recording mode, source,
  codec, FPS, buffer duration, bitrate, container, folder, and audio choices.
  Unsupported Windows-only options are not presented. Settings edits are staged;
  Save validates/applies and Cancel discards. Capture-changing settings require
  stopping capture in this initial version; showing settings must remain possible.
- Capture overlay editor supports the Windows formats: PNG, JPEG, BMP, WebP,
  GIF, MP4, WebM, MOV, MKV. Animated assets loop silently. Preserve up to 16
  ordered layers, visibility, master enable, position, fit-box size, aspect ratio,
  and opacity. Dragging and numeric controls keep each layer inside the canvas.
- Capture overlay edits apply to future frames in replay and recording, with
  persistence after successful application. Existing buffered frames and clips
  retain their original content. Closing the panel/editor does not remove layers.
- Missing or undecodable assets report the affected layer and do not fabricate
  a successful preview or saved state. Disabling composition releases media work.

## Organization

Use small concrete modules; avoid an extensible platform framework before a
second desktop is implemented. Proposed boundaries, with filenames left to the
builder:

```text
linux/
  app/                  entry point and application lifetime/controller
  platform/hyprland/    layer-shell, monitor choice, global shortcut integration
  ui/                   dashboard, settings, library, overlay editor, shared theme
  settings/             validated options, persistent settings and layer storage
  capture/              existing GSR process and IPC adapter
  overlays/             layer model, media loading and composition logic
  plugins/              GSR plugin entry points and runtime composition transport
  tests/                focused unit, integration and optional hardware tests
```

UI events call controller operations; widgets do not directly build recorder
arguments or own recorder lifetime. Models and settings contain no widget
ownership. Extract the current main.cpp UI/settings/library responsibilities
only as needed by these features. Retain qmake unless a verified integration
constraint requires a build-system change.

## Platform and composition contracts

Layer-shell is responsible for displaying the dashboard above applications.
LayerShellQt is the preferred integration if compatible with the installed Qt;
its availability has not been established. Use the smallest supported Qt
integration instead if necessary. Define keyboard/pointer behavior, monitor
selection, popup/dialog handling, and mapping/unmapping before claiming support.
Do not make a fullscreen xdg-toplevel workaround look like a proven overlay.

GSR's installed header `/usr/include/gsr/plugin.h` exposes init/draw/deinit
callbacks, framebuffer dimensions, graphics API and color depth. Its manual
documents `-p plugin.so`. This establishes an extension point, not proven media
composition. First verify a minimal Captail plugin on the actual capture path.

The plugin draws media into captured frames independently of desktop windows.
It must support the actual EGL/OpenGL context and preserve GL state needed by
GSR. Decode animated assets outside the recorder's render callback and bound
queued work; no synchronous file/network reads or decoding in draw(). Use
existing Qt/FFmpeg facilities when suitable, rather than implementing codecs.
No audio from overlay media enters recorded audio tracks.

The plugin needs a private per-session configuration/update channel. Validate
message sizes, normalized numeric values, layer count, and local asset paths;
do not load arbitrary user-supplied plugin binaries. Track acknowledgement or
failure of layer updates before reporting application success. The builder
chooses a simple transport after validating how GSR launches plugins. No new
general plugin system is exposed to users.

The preview shares layer geometry/order with recorded composition. Its first
version can preview the media composition against a labeled canvas; a live
screen preview is not required and must not be simulated as live capture.
Do not promise exclusion of the open dashboard from monitor recordings: the
current monitor-capture path can include desktop surfaces, including the panel.
Recorded media layers must persist even while the dashboard is hidden.

Keep the current Linux settings namespace and preserve existing options during
reorganization. Add versioned layer settings only as needed; do not import
Windows configuration or perform the later Shadowless migration now.

## Validation and acceptance

1. Existing offline GSR tests continue to pass; no recording is started by default.
2. Tests cover layer normalization/bounds/order, malformed updates, settings
   round trips, save/cancel behavior, shortcut conflicts, and capture lifecycle
   while the dashboard is repeatedly opened and hidden.
3. Verify a real Hyprland layer surface, transparency, target monitor, Alt+Z,
   Escape, pointer/keyboard handoff, tray Open/Quit, and reactivation of one
   running instance. Test popups and file dialogs for correct stacking/input.
4. In a representative game, open/close the dashboard without stopping capture;
   verify input returns to the game. Record exactly which native Wayland or
   XWayland game was tested. No universal game-compatibility claim follows.
5. Save replay and recording samples with a static image and animated media;
   verify order, opacity, position, mute, visibility and live edits. Decode the
   files with FFmpeg, inspect representative frames, and confirm layer animation
   continues while the panel is hidden. Test missing media and plugin failures.
6. Compare composition off/on with the editor closed. Report observations;
   do not claim zero overhead or unchanged game performance without measurements.

Tests needing active screen capture, game interaction, or compositor-owned
shortcut setup remain explicitly distinguished from offline tests. Document
unavailable validation rather than declaring it passed.

## Deferred work

KDE, GNOME, X11 portability, project-wide renaming, packaging/distribution,
webcam sources, HDR composition, a new video editor, and universal focus handling
across every game are not part of this first Hyprland implementation. Existing
capture modes remain available; unsupported composition modes must fail clearly.

## References

- Existing implementation: `linux/app/main.cpp`, `linux/capture/`.
- Windows behavior: `src/Captail/SettingsWindow*` and `docs/CAPTURE_OVERLAYS.md`
  in the sibling `captail-fork` worktree; these pending Windows features are not
  all present on the Linux branch's older Windows baseline.
- GSR plugin ABI: installed `/usr/include/gsr/plugin.h` and recorder manual.
- GSR UI behavior: https://git.dec05eba.com/gpu-screen-recorder-ui/about/
- Qt layer-shell integration: https://github.com/KDE/layer-shell-qt
