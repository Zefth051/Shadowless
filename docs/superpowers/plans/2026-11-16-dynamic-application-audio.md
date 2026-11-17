# Dynamic Application Audio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Buffer applications discovered during Linux capture and expose PC/Game/Microphone groups plus More tracks without duplicate audio.

**Architecture:** Extend the bundled GSR engine's application capture inside its existing process. Keep audio track objects stable across registration, preserve capture-clock timing, and build saved replay stream maps from immutable snapshots. The Qt app displays discovered sources and the editor groups actual recorded streams; no UI-only or placeholder channels.

**Tech Stack:** C11 bundled GSR, C++17/Qt6, PipeWire/Pulse recording streams, FFmpeg, libmpv.

**Spec:** ../specs/2026-11-16-dynamic-application-audio-design.md

## Global constraints

- User approved PC as the group of all non-game desktop audio, with Game and Microphone separate. There are three main rows.
- No Rust migration, new dependency, version change, commit, publication or installed AppImage replacement.
- Preserve dirty changes, current settings/devices, gain/mute/boost and existing clip files.
- Discovery must continue after video buffering starts. Never restart capture merely to add an application.
- Sources must be buffered from discovery; later UI inclusion cannot reconstruct unrecorded history.
- Maintain shared-clock onset and pause timing. Existing legacy/two-track clips keep their real tracks.
- New implementation is compiled into .qa/linux-build/app and matching engine for linux/run.sh.

## Stream/app contract

- Enable native dynamic mode with `SHADOWLESS_DYNAMIC_AUDIO=1`; keep legacy paths available for old explicit application/four-track configurations.
- Dynamic mode becomes the normal desktop-audio path when system audio is enabled, replacing PC+mic capture with per-application capture plus an actual remainder.
- Native track titles/MP4 handler names: `PC (other)`, `App: <application.name>`, `Game: <application.name>`, `Microphone`. Off-by-default inclusion may use `App (off): ` / `Game (off): ` when explicitly deselected. Prefixes describe actual streams, never placeholders.
- PC group = PC (other) + App streams; Game group = Game streams. Mic remains independently controlled. Group expansion affects visibility only.
- Game classification uses optional initial `SHADOWLESS_GAME_AUDIO_APP`; live app selection writes atomic JSON to `SHADOWLESS_AUDIO_APPS_FILE`, keyed by application name with `{game: bool, included: bool}` properties. Default included=true. Root/engine implement the same contract; game selection is explicit when automatic identity is unavailable.
- Native structured discovery logs: `gsr info: shadowless-audio-app: ` followed by a JSON object containing string `name` and boolean `game`. Escape names with the existing JSON helper. Track identity is never reset/reused for another application during a session.
- A discovered application may be quiet/closed and resume later. Shared application.name identities are one audio source; per-browser-tab separation is not claimed.
- Recording/clip files are not marked successfully saved until any required final mux completes; preserve source/spool data on failure.

## Review focus

- An app beginning halfway through a replay must not be shifted to the beginning or erase earlier video.
- Adding/closing/reopening app sources must not invalidate pointers held by capture or save threads.
- PC must include app audio exactly once and exclude game, including new applications and manual game changes.
- More tracks/parent checkbox changes must not duplicate or unexpectedly mute playback/export.
- Continuous and simultaneous recording must preserve late app stems and survive finalization errors.

## Tasks

### 1. Native dynamic application recording

Files: engine include/src recorder/audio_capture, replay_save, muxer, encoder/replay storage, pipewire_audio; source manifest/provenance; hardware-free engine tests.

- [x] Add tests for source discovery after start, stable lifetime, source/output maps and late timestamps.
- [x] Implement stable dynamic tracks and bounded replay retention.
- [x] Capture one independent source per audio app, plus microphone and real unassigned remainder; update exclusions safely.
- [x] Consume live selection/game state and emit discovery events.
- [x] Handle fixed-header recording using private aligned spools/segments and validated finalization as necessary.
- [x] Test real saved media fixtures, error paths and matching engine build/hash.

### 2. Grouped editor/playback/export

Files: ui/clip_player.{h,cpp}, ui/clip_waveform.h, player/media_tools as needed; player/UI tests.

- [x] Test dynamic stream classification, legacy clips, late tracks and selection/export without duplication.
- [x] Render PC/Game/Microphone primary groups for dynamic clips; More tracks reveals actual application rows with accessible keyboard controls.
- [x] Parent and child selectors share actual stream selection. Expansion changes no audio state. Preserve unknown/legacy tracks.
- [x] Keep waveform timing and export mappings intact.

### 3. App integration and capture source controls

Files: capture/gsr_capture.{h,cpp}, capture/audio.{h,cpp}, app/controller.{h,cpp}, settings/settings.cpp, ui/dashboard.{h,cpp}, focused tests.

- [x] Activate dynamic mode for ordinary desktop capture while preserving devices/microphone privacy and explicit legacy modes.
- [x] Parse structured application events, retain discovered source state and expose live Track/Game controls.
- [x] Persist source inclusion/game choices, write atomic engine state, preserve independent gain controls.
- [x] Keep dashboard track summary honest before/after source discovery.

### 4. Integration and verification

- [x] Review whole change against spec, build engine/app/UI/media targets, and run meaningful routing/recording/editor fixtures.
- [x] Stage engine plus matching provenance into the run.sh build using atomic replacement.
- [x] Document implemented behavior and actual verification gaps; do not claim live PipeWire validation from headless tests.

## Verification and remaining limits

Implemented and staged locally. Native engine build and 11 hardware-free dynamic
audio tests pass, including delayed media, nonblocking preparation/finalization
and sticky write-failure recovery. The editor/media implementation passed 11
media tests; focused Qt editor/dashboard/settings checks pass. Adapter checks
cover app discovery during a session, old mixed-device settings, progress-based
command timeouts and failed-finalization state recovery. Source manifests and
diff whitespace checks pass. Full adapter socket-server fixtures remain blocked
by sandbox restrictions; real PipeWire and mpv audio playback are not verified.

Ruling: automatic PC audio spans all output devices, matching the user's explicit
all-PC requirement. The previous physical output selector and system/mic mixing
control are hidden in automatic mode; microphone input and privacy preferences
are preserved. Explicit advanced static routing remains available.

The implementation is in the normal native build used by linux/run.sh. No
release, version change, installed AppImage replacement or Rust migration occurs.
