# Hyprland overlay implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Port Captail's dashboard and media overlays to native Hyprland with structured code and measured composition overhead.

**Architecture:** Qt widgets on layer-shell, desktop-global shortcuts, a background application controller, and the existing supervised GSR process. A Captail plugin composites media into captured frames; worker decoding and cached GL textures keep work off the capture callback.

**Tech Stack:** C++17, Qt 6 Widgets/Network/DBus/Gui, LayerShellQt, Wayland, GSR plugin ABI, FFmpeg, OpenGL.

**Spec:** `docs/superpowers/specs/2026-11-10-hyprland-overlay-design.md`

## Global constraints

- Hyprland/Wayland first; retain the current Linux config and external GSR backend.
- No Windows changes, release metadata changes, project rename, push, or publication.
- Native C++17/Qt 6; preserve existing capture tests and safe save/stop behavior.
- Up to 16 media layers; animated assets loop silently; changes affect new frames.
- The optional passive indicator starts disabled and must not receive input.

## Review focus

- Missing/invalid media must report failure without stopping otherwise valid capture.
- Shutdown or panel hiding during a save must not truncate clips.
- Layer updates must not stall the recorder render callback.
- Global shortcuts and native dialogs must not strand input above games.
- Invalid persisted geometry or update messages must not trigger excessive allocation.

### Task 1: Layers, media decoding and settings

**Files:** `linux/overlays/layer.{h,cpp}`, `linux/overlays/media.{h,cpp}`, `linux/settings/settings.{h,cpp}`, `linux/tests/tst_overlays.cpp`, `linux/tests/OverlayTests.pro`.

**Interfaces:** `OverlayLayer` holds normalized media path/geometry/visibility/opacity; `OverlayScene::fromJson/toJson` validates a bounded versioned message. `Media::open/advance/frame` returns bounded RGBA frames. `Settings` reads/writes capture and overlay options with QSettings.

- [ ] Add/run tests for 16-layer limits, invalid geometry/messages, ordering and image loading; observe missing implementation failure.
- [ ] Implement shared scene validation and media decoding, preserving aspect ratio and bounded dimensions. Static frames decode once; animation advances according to timestamps and loops without audio.
- [ ] Extract persistent capture settings from main.cpp, retain all keys, add scene/indicator settings, test round trips and invalid config.
- [ ] Build and run offline overlay tests; require all pass.

### Task 2: Recorded composition plugin

**Files:** `linux/plugins/compositor.{h,cpp}`, `linux/plugins/CaptailOverlay.pro`, `linux/overlays/overlay_client.{h,cpp}`, `linux/capture/gsr_capture.{h,cpp}`.

**Interfaces:** `OverlayClient::apply(scene)` sends a bounded scene to a private session socket and reports success/error; `GsrOptions` carries only trusted app plugin/socket paths. Plugin implements installed `gsr_plugin_init/deinit` ABI.

- [ ] Add/run real socket tests for fragmented/oversized/malformed commands, failed layer loads, acknowledgement and disconnect.
- [ ] Implement private session transport and worker-owned media decoding. Rendering consumes cached frame snapshots and updates textures only for changed frames. Preserve GL state, layer ordering, opacity and fit-box semantics.
- [ ] Wire a trusted adjacent plugin into GSR startup using per-process environment; no plugin means explicit overlay unavailability, never a success claim.
- [ ] Run existing GSR tests and overlay tests. Verify real static/animated composition using saved clips and complete FFmpeg decode.

### Task 3: Hyprland surfaces and shortcuts

**Files:** `linux/platform/hyprland/overlay_surface.{h,cpp}`, `linux/platform/hyprland/shortcuts.{h,cpp}`, build/run helper under `linux/`.

**Interfaces:** `OverlaySurface::show/hide` maps an overlay layer on a chosen QScreen. `Shortcuts` emits toggle/save/capture actions and registration errors. The passive indicator uses no keyboard or pointer input.

- [ ] Verify LayerShellQt against installed Qt; prepare a local development dependency without privileged system changes if absent.
- [ ] Implement native layer surface and global shortcut registration. Use a supported portal or compositor integration; preserve user bindings and expose conflicts.
- [ ] Verify active-monitor selection, transparency, repeated mapping, Escape, input handoff and dialog/popover behavior on the real compositor.
- [ ] Verify the disabled-by-default status surface renders only on state changes and receives no input.

### Task 4: Modular dashboard and editor

**Files:** `linux/app/controller.{h,cpp}`, `linux/ui/{dashboard,settings_page,library,overlay_editor,theme}.{h,cpp}`, `linux/app/main.cpp`, `linux/app/CaptailLinux.pro`, `linux/Captail.pro`.

**Interfaces:** `Controller` owns engine/settings/plugin transport/tray and background lifetime. UI emits user intent; controller applies options and scenes. `OverlayEditor` shares scene geometry with composition and supports drag/numeric editing; staged settings Save validates and Cancel discards.

- [ ] Extract theme/library/settings from monolithic main.cpp; retain existing asset resources and capture behavior.
- [ ] Build the fullscreen dashboard, embedded settings and media editor; include add/remove/reorder, per-layer/master enable, opacity, position/size and labeled composition preview.
- [ ] Implement single-instance toggle/save/quit, tray entry, hide-without-stop, pending-save shutdown and optional indicator.
- [ ] Exercise UI using long paths, empty library, failed capture/media and repeated open/close; verify screenshot and real interactions.

### Task 5: Integration, performance and review

**Files:** `linux/tests/`, `linux/README.md`, `docs/LINUX_HYPRLAND_OVERLAY.md`, plan/ledger.

- [ ] Run complete offline suites and fresh release build; inspect every failure.
- [ ] Run live capture cases with composition off, static and animated assets with panel hidden; inspect frames and fully decode clips.
- [ ] Measure CPU/GPU/memory and recorded frame timing across those cases; separately inspect panel/indicator cost. Do not infer game FPS from recorder output.
- [ ] Test against a real available fullscreen application/game, report exact coverage and any unavailable game validation.
- [ ] Request a fresh whole-change code review, fix important findings with regression checks and rerun affected suites.
- [ ] Document build/run/shortcuts, tested results, dependencies and remaining limitations. Keep changes local and reviewable.

## Execution decisions

The user approved implementation, so continue inline without another permission
gate. The existing `captail-linux-gsr` checkout is already an isolated linked
worktree on `experiment/linux-gsr`. Leave changes uncommitted for review unless
committing becomes necessary; do not bundle unrelated Windows work.
