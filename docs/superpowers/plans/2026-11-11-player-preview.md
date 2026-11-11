# Linux Player and Preview Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Implement the approved native player/editor parity, full capture preview, and transparent moon branding without burdening replay capture.

**Architecture:** Keep GSR capture separate from playback. Embed libmpv in a Qt OpenGL widget; asynchronous, single-job media analysis/export uses bounded FFmpeg work. Preview requests are visible-only, throttled, bounded, and served independently from scene mutations.

**Tech Stack:** C++17, Qt 6, libmpv, FFmpeg, GSR/OpenGL.

**Spec:** ../specs/2026-11-11-linux-player-preview-design.md (approved 2026-11-11, with explicit clipping-performance constraint).

## Global Constraints

- Preserve existing local modifications; work in the existing Linux linked worktree.
- Keep native Qt/GSR architecture, source clips, English repository text, current versions.
- No player decoding or analysis while hidden; one bounded analysis/export job, cancellation and CPU thread limits.
- No push, publish, release, or commits that include the user's pending changes.

## Review Focus

- Switching/hiding during analysis must cancel work and discard stale results.
- Export cancellation/failure must preserve source and destination.
- Zero audio tracks and multiple selected tracks must play/export correctly.
- Preview must work with zero layers and disabled composition, stop when hidden, preserve GL state.
- Fullscreen must remain within the Wayland overlay and Escape must first exit fullscreen.

### Task 1: Asynchronous media tools

**Files:** Create linux/player/media_tools.{h,cpp}, linux/tests/player/{PlayerTests.pro,tst_player.cpp}.
**Interfaces:** MediaTools owns probe/thumbnail/waveform/export jobs and emits ready/progress/error/cancelled; explicit stop() cancels all work. Root integrates the exact resulting API after handoff.
- [x] Add tests using real generated clips for metadata, bounded thumbnails/waveforms, selected-track trim and mix, output collision, cancellation and source preservation; observe failures.
- [x] Implement asynchronous QProcess jobs, one decoder thread, atomic temporary export and bounded analysis output.
- [x] Run focused tests and report exact interface for player integration.

### Task 2: Capture preview

**Files:** linux/plugins/{plugin,compositor}.{cpp,h}, new linux/overlays/preview_client.{cpp,h}, linux/ui/overlay_editor.{cpp,h}, linux/app/controller.cpp; focused preview tests separate from tst_ui.cpp.
**Interfaces:** OverlayEditor::setPreviewSocket(QString) selects active capture preview; existing scene/edit API remains compatible. Dedicated preview socket is derived from overlaySocketPath().
- [x] Test inactive state, actual aspect ratio, empty-layer preview and hide lifecycle; observe failures.
- [x] Implement on-demand bounded frame transport and throttled GL capture, avoiding capture-thread socket I/O, blocking locks, and stale-frame queues.
- [x] Run offline tests and live frame/orientation comparison if the compositor is available.

### Task 3: Player and editor UI

**Files:** linux/ui/clip_player.{cpp,h}, new linux/player/mpv_player.{cpp,h}, linux/ui/dashboard.{cpp,h}, linux/tests/tst_ui.cpp, build .pro files.
**Interfaces:** ClipPlayer::open(QString), back() preserved; fullscreenChanged(bool) resizes dashboard panel. MpvPlayer exposes transport state and mixed track selection.
- [x] Add UI tests for native transport, navigation boundaries, fullscreen Escape, stop/analysis cancellation on hide; observe failures.
- [x] Implement lazy hardware-accelerated libmpv OpenGL playback, selected audio mixing, navigation, speeds, shortcuts, idle fullscreen controls.
- [x] Integrate media analysis timeline, waveforms, trim/export with explicit audio merge and cancellation.
- [x] Run player/UI tests with generated audio/video fixtures and live Wayland visual checks.

### Task 4: Icon and integration verification

**Files:** Linux desktop/resources/theme and Windows icon assets/generators; linux/README.md.
- [x] Connect generated PNG/ICO while preserving accent and inactive distinction; derive variants reproducibly and update generators to use the moon source.
- [x] Run alpha/multi-resolution asset checks, complete native build/test suite, visual checks, and scoped clipping/playback resource observations.
- [x] Request independent final review; fix important findings and rerun affected checks. Report measured limits, especially Windows runtime and hardware-specific performance.
