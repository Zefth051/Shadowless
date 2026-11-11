# Linux library and feedback Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Optional game collections and passive save feedback on Linux.
**Architecture:** Reuse Qt settings, Windows catalog/sound, and passive LayerShellQt surfaces. Query game identity on demand off the UI thread; preserve the original on routing failure.
**Tech Stack:** C++17, Qt 6, LayerShellQt, existing Hyprland IPC.
**Spec:** docs/superpowers/specs/2026-11-11-linux-library-feedback-design.md

## Global Constraints

- No cursor changes, new dependencies, old clip migration, version changes or publishing.
- Preserve the existing dirty worktree.
- Notification must never activate or receive keyboard/pointer input.

## Review Focus

- Unknown games stay in root; ambiguous catalog entries must not misidentify games.
- Unsafe names, symlink directories and duplicate filenames preserve existing files.
- Collection filtering happens before the display limit.
- Repeated feedback replaces its timer; hidden feedback does no periodic work.
- Query/save failures and quitting during game lookup must not leave capture stuck.

### Task 1: Collections

Files: capture/replay_paths.{h,cpp}, settings, settings_page, library, controller, qmake projects and tst_ui.cpp.
Interfaces: gameForClients(QJsonArray), currentGame(), routeClip(root,path,game,error).
- [x] Add failing tests for optional settings and recursive filtering; cover safe routing.
- [x] Implement optional setting, bounded on-demand detection, safe routing, recursive scan and selector.
- [x] Run UI tests.

### Task 2: Passive feedback

Files: ui/clip_notification.{h,cpp}, controller, resources.qrc, qmake projects and tst_ui.cpp.
Interface: ClipNotification::present(title,detail,timeoutMs), dismiss().
- [x] Add failing test for passive notification and resource sound; cover replacement timeout.
- [x] Reuse passive surface; connect save lifecycle and asynchronous QSoundEffect.
- [x] Run linux/build.sh and native Wayland focus checks.

## Verification and review

- `./linux/build.sh`: 87 passes, zero failures, seven pre-existing opt-in/environment skips, plus desktop-entry check.
- Native Wayland: notification mapped in `shadowless-notification`; existing focus remained unchanged during show/replacement/hide.
- Native fullscreen fixture: retained both focus and fullscreen during all three transitions.
- Wayland trace: keyboard interactivity 0, empty pointer input region (region created and assigned without any rectangle).
- Independent reviewer found background-game selection, completed-but-undelivered lookup ownership, and overlapping replay/recording routing issues. Fixed: require focused client ID 0; keep callback ownership until delivery; attach recording identity to the engine's save completion signal.
- Regression tests reproduced wrong background identification and lost replay/recording identity before the fixes. Queued-stop test uses synthetic IPC without capturing the desktop or audio.
- Collection discovery and sorting run in the scan worker; filtering precedes the 20-clip display limit.
- Not measured: Rocket League itself, game FPS/GPU impact, and real recorder/audio save notification together. Existing live capture/audio tests remain opt-in.
- No commits, publishing, clip migration or cursor changes.
