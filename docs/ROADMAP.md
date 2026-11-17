# Windows and Linux roadmap

Requested on 2026-11-12. This is a shared backlog, not a release commitment. Work
proceeds gradually; the order below is a proposal, not an approved schedule.
Reconciled on 2026-11-17 against both branches' changelogs and logs.

## How to read statuses

- Every item lists a status for each platform: `done`, `partial`, `open` or `n/a`.
  `done` means implemented in that branch. Verification limits (real hardware, a real
  desktop, a Windows host) are written next to it and are never inherited from the
  other platform: a feature that works on Linux is not marked on Windows because of it.
- The checkbox is checked, and the text struck, only when every applicable platform is
  `done`. Open items name what is still missing per platform.
- Implementation checks do not establish full hardware coverage. The dated sections
  after the backlog are platform logs (Linux sections in the Linux branch, Windows
  sections in the Windows branch) and hold the evidence behind each status.

## Immediate distribution work

- [x] ~~Enable public updates in Release builds, accepting only published final
  releases, excluding drafts and prereleases even on `0.x`.~~
  - Windows: done. Automated policy tests and native Windows CI pass; installer
    validation is tracked in release status.
  - Linux: done. The public 0.3.3 updater's discovery, download, SHA-256 check and
    atomic install passed with isolated files. Upgrading the installed AppImage to the
    next published release is still unverified. A gap found 2026-11-17: the 0.x updater also accepted
    prereleases; fixed in the 0.3.8 development branch and covered by tests.
- [x] ~~Keep a maintained copy of the GSR engine source in the Linux branch and bundle
  its build with Shadowless, without a separate GSR installation or graphical interface.~~
  - Linux: done. GSR 6.1.7 and its matching helper are bundled; build, provenance,
    engine selection, forced-stop handling and AppImage startup checks pass. Live KMS
    authorization and capture with the bundled helper still need interactive validation.
  - Windows: n/a.

## Minor changes

- [x] ~~Two independent settings: launch Shadowless at login, and start replay
  buffering whenever Shadowless launches, including manual launches.~~
  - Linux: done. XDG and the default Hyprland Lua/legacy configuration paths; recovery
    preserves active capture. A real logout/login cycle and custom Hyprland config
    locations are unverified.
  - Windows: done. Startup/config tests pass; runtime on a Windows host is unverified.
- [x] ~~Let users choose the UI accent color using a HEX value.~~
  - Linux: done (validated HEX, persisted; shipped in 0.3.4).
  - Windows: done (validated #RRGGBB beside the presets; shipped in 0.3.2). The custom-accent
    WPF QA and a visual check of settings and the editor still need a Windows host.
- [ ] Organize clips into the correct game folders. Check detection and fallback
  behavior, not just the folder creation toggle; preserve existing files.
  - Linux: done. Conflicting game identities fall back to the output root, Plasma falls
    back safely without hyprctl, and existing files are preserved.
  - Windows: partial. Per-game folders with friendly names exist and replay folder
    routing is checked in CI; the detection and fallback review has not been done.

## Medium changes

- [ ] Refresh the UI and buttons while retaining Shadowless's visual identity. Use
  restrained animations with negligible recording overhead.
  - Linux: partial. QSS only (rounder buttons and sections, accent hover and scrollbar
    states, no animations); not visually verified on a running build.
  - Windows: partial. Shared buttons gained press/focus feedback and safe icons moved to
    vector paths; no new animations; not visually verified.
- [ ] Improve overlay performance and settings usability. Reproduce and fix laptop
  behavior issues individually; exact cases still need to be described.
  - Linux: open. The suspected Plasma Settings scroll crash (a graphics effect over the
    scrolled capture settings) was removed but is not confirmed on a real Plasma session.
  - Windows: partial. Hidden editors stop preview polling and settings writes debounce;
    no CPU/GPU overhead measurements.
- [ ] Fix black recordings on laptops. Reproduce integrated/discrete GPU and hybrid
  graphics capture; validate saved video playback before claiming laptop compatibility.
  - Linux: open. Needs real laptop hardware.
  - Windows: partial. The adapter/output selection audit adds a guarded fallback; it is
    **not verified fixed** (needs Auto/DXGI, denied borderless access, both GPU
    preferences and monitor unplug/replug on Windows 11).
- [ ] Refresh icons across both branches.
  - Linux: partial. Bundled SVGs for clip refresh and trash; film icon for clips without
    a thumbnail.
  - Windows: partial. Refresh, close and notification glyphs replaced by vector paths.
- [ ] Linux: fix the app icons shown in audio source lists.
  - Linux: partial. Icons come from PulseAudio metadata with a bundled fallback; real
    themed icons in the public AppImage remain unvalidated.
  - Windows: n/a.
- [x] ~~Add an open-containing-folder button between playback and deletion.~~
  - Linux: done (opens the clip's real directory, including game subfolders).
  - Windows: done.
- [x] ~~Add simple Discord Rich Presence that works without user configuration.~~
  - Linux: done. Shown only while capturing a recognized game, with a Settings switch.
    Real Discord verification is pending.
  - Windows: done. Same behavior and a persisted setting in all 11 languages. Real Discord
    checks and `tools/DiscordPresenceQa` still need a Windows host.

## Major changes

- [ ] Add a Medal-style overlay for pressed keys, mouse and gamepad inputs, then KPS and
  future statistics. Editable overlays need separate event data and clean video
  underneath; an overlay burned into captured frames cannot be removed afterwards.
  - Linux: partial. Phase 1 is implemented (2026-11-17): five layouts in Capture overlays,
    live preview, canvas placement, opt-in `<clip>.inputs.json` and playback over clean
    video. Real keyboard/mouse/DualSense, hotplug and video timing are unverified.
  - Windows: open. The layout JSON and sidecar schema (`docs/LINUX_INPUT_OVERLAY.md`) are
    shared; a Windows input source (`WH_KEYBOARD_LL`/`WH_MOUSE_LL` with scan codes,
    gamepad API) and WPF rendering are pending.
  - Open on both: burn the overlay into captured frames only if the cost is low; export
    with the overlay; dragging the overlay in the player.
- [ ] KPS overlay on the same input event stream. Define the counting window, display
  and privacy controls first; to be designed after the current overlay is tested on
  real hardware.
  - Linux: open. Windows: open.
- [ ] Offer background clip compression only while no game is open. Preserve originals
  until validation succeeds. Distinguish mathematically lossless from visually similar
  lossy compression; a size reduction is not guaranteed for already compressed footage.
  - Linux: done. Opt-in, near-lossless HEVC with `ffprobe` and SSIM checks before a
    replacement; long-run behavior is unverified.
  - Windows: open.
- [x] ~~Allow saving replay clips during continuous recording.~~
  - Linux: done (one GSR process).
  - Windows: done (shared encoders, shared pause). Native lifecycle QA exists; actual
    Windows execution is pending.
- [ ] Complete separate audio tracks and independent controls for PC, game, music and
  microphone. Define PC as the remainder when app sources are separated.
  - Linux: done. Four-track mode with independent Game and Music levels, dynamic
    application discovery, and editor playback and export of every track. Live PipeWire
    isolation and a real four-track capture are unverified.
  - Windows: partial. Per-application routing to tracks 1-6, a System track for "all
    audio except the selected apps", and microphone fallback; there is no PC/Game/Music/
    Microphone role model. Real audio verification is pending.
- [ ] Add camera capture from local webcams (V4L2 on Linux) and compatible virtual-camera
  sources, including Camo/DroidCam-style network cameras. Assess source discovery,
  synchronization, permissions and platform support before choosing the capture path.
  - Linux: open (design). Windows: open.

- **Beta testing update channel (planned, not implemented):** Settings → Updates → Beta testing, available on Windows and Linux.
  - Off by default. Off: only final releases are offered. On: final releases and compatible prereleases.
  - Never downgrades automatically; Store builds stay Store-managed.
  - Requirements and acceptance tests: `docs/BETA_TESTING_CHANNEL.md`. Not started; needs an explicit go-ahead.

## Compatibility (added 2026-11-17)

Policy, requirements and evidence levels: `docs/COMPATIBILITY.md`; rule:
`docs/PRODUCT_PHILOSOPHY.md`. Nothing here replaces an item above.

- [ ] Priority systems: Windows 10, Windows 11, Arch Linux, CachyOS (Wayland, Hyprland,
  KDE Plasma 6 on Linux). Every feature must state its behavior on the four systems.
  - Windows 0.3.4 (hotfix): per-application audio is unavailable below build 20348 and
    0.3.3 aborted capture start on Windows 10 with saved advanced/separate audio. Fix:
    mixed-audio fallback, unchanged saved settings, one-time notice, real error shown in
    recovery messages. CI-tested with simulated builds (19045, 22631); real Windows 10 and
    11 verification pending before release.
  - Windows: open. Clean-PC behavior without the Visual C++ runtime is unverified (the
    runtime is not bundled); independent of the 0.3.3 regression.
  - Linux 0.3.8 (in development): optional input access must never affect core capture;
    accurate failure messages; helper security review; hotplug; Arch/CachyOS audit.
- [ ] More Linux distributions, in stages, each subject to validation. No official
  support is declared before the acceptance criteria in `docs/COMPATIBILITY.md` pass.
  - Stage 1 Fedora, Stage 2 Ubuntu, Stage 3 Debian, Stage 4 others (for example openSUSE).
  - Acceptance per distribution: install, dependencies, capture, audio, permissions,
    updater and desktop behavior, with evidence.
  - Linux: open. Windows: n/a.

## Proposed delivery order

0. Windows 0.3.4 hotfix and Linux 0.3.8 stabilization take priority over new features.
1. Finish and validate the immediate distribution work.
2. Check existing behavior in both branches; finish game-folder organization on Windows.
3. Fix reproducible laptop/overlay issues, then refresh the UI.
4. Validate concurrent clipping/recording and audio separation on real hardware; test the
   input overlay on a real desktop before designing KPS or the Windows port.

Public visibility and release publication remain separate owner decisions.

## Platform logs

The dated sections below are implementation logs for one platform. They record the
evidence and verification limits behind the statuses above.

## Linux QOL implementation — 2026-11-15

Current Linux source adds validated HEX accents with persistent settings,
compact capture cards, consistent control states, adaptive clip names and visible
clip actions at narrow widths. Conflicting game identities fall back to the
output root. Audio application icons now use available PulseAudio metadata with
a bundled generic fallback; real themed icons in the public package remain an
open validation item.

Plasma 6 Wayland startup and portal shortcuts are implemented alongside Hyprland.
Plasma no longer requires hyprctl; its game-folder detection safely falls back to
the output root. Desktop/portal fixtures and package tests pass. Real Plasma
capture, layer surfaces, tray and desktop shortcut approval still need testing.

The 0.3.3 public updater's full HTTPS discovery, download, SHA-256 verification
and atomic installation passed using isolated temporary files and the installed
AppImage runtime. The reported update-check failure was not reproduced; new
HTTP/TLS diagnostics retain the actual failure category for follow-up.

Shared checklist items above stay open until Windows parity and the stated
platform checks are complete. This work does not publish or modify a release.


## Linux QOL completion checklist — 2026-11-15

This platform-specific checklist closes completed Linux implementation work;
shared items above still track Windows parity and the stated live checks.

- [x] ~~Support validated HEX accent colors and persist the selected value.~~
- [x] ~~Keep recent clip actions visible at narrow widths and provide an open-containing-folder action.~~
- [x] ~~Handle conflicting game identities with a safe output-root fallback while preserving files.~~
- [x] ~~Refresh Linux capture cards, button/focus states and checked/disabled checkbox readability.~~
- [x] ~~Keep engine notices on the dashboard instead of settings and the editor.~~
- [x] ~~Resolve application audio icons from metadata, use a bundled fallback and package the app ICO reader.~~
- [x] ~~Automatically release and restore the control overlay around required KMS authorization, including cancellation and errors.~~ Recorder and controller fixtures pass; real desktop authorization remains to be tested.
- [ ] Validate real application theme icons and required KMS authorization interaction in the packaged build.
- [ ] Validate Plasma capture, layer surfaces, tray and shortcut approval on a real Plasma Wayland session.
- [ ] Verify upgrading the installed public 0.3.3 AppImage to the next published release.

See `CHANGELOG.md` under Unreleased for the user-facing change list. Final release
version and publication remain owner decisions.

- [x] ~~Keep the active-capture settings notice visible while scrolling and slightly dim locked capture fields.~~ Included in Linux 0.3.4.

## Editor usability update — 2026-11-16

- [x] ~~Keep waveform backgrounds dark and render readable audio/silence, including dark custom accents.~~ Rendering regression reproduced and covered by Qt tests.
- [x] ~~Remove decorative audio-row frames and lighten the timeline outline while retaining keyboard focus and draggable trim handles.~~
- [x] ~~Add editor sidebar filters by game folder and recording mode, preserve thumbnails, and navigate within the filtered results.~~ Includes an explicit empty state and bounded filter controls for long folder names.
- [ ] Diagnose the reported KDE Plasma crash when scrolling Settings. Offscreen scroll/render checks with active/inactive capture, narrow layouts and fractional scaling did not reproduce it. A real Plasma session or crash backtrace remains required; this report is not closed by a smoke test.

These complete a scoped part of the medium UI/usability roadmap, not the whole
cross-platform refresh. The editor and media targets compile locally; live
Plasma behavior and packaged playback audio remain separate verification gates.
No release version or published package is changed by this work.

## Recording and audio parity — 2026-11-16

- [x] ~~Add an explicit four-track mode: PC 1 / Game 2 / Music 3 / Microphone 4.~~ Requires explicit game/music application assignments and microphone capture. PC excludes those applications; old modes and files are preserved.
- [x] ~~Retain track names in MP4 recording/replay metadata and selected-track exports.~~ Both title and handler metadata are written by the managed engine; the editor reads MP4 handler names after analysis.
- [x] ~~Keep editor track selectors on the left in playback and editing.~~ Hidden waveforms no longer center the remaining selector; keyboard and full-row click behavior are covered.
- [x] ~~Add Pause/Resume for standalone continuous recording.~~ Actual recorder log markers confirm state; pending/unknown states prevent repeated commands. Simultaneous replay/recording does not expose unsupported pause.
- [x] ~~Implement basic Discord Rich Presence with the supplied public Application ID.~~ Capture state, a recognized game name and public artwork are sent; unchanged activities are not refreshed; reconnect and protocol handling are covered by isolated socket-pair fixtures without contacting Discord.
- [ ] Validate live PipeWire isolation of the four sources, standalone pause/finalization and actual Discord presence on a supported desktop.
- [x] ~~Build the managed engine and package a private AppImage.~~ Cached local build tools were located; engine build, packaging smoke tests and source hashes pass. Socket-binding restrictions still prevent the full adapter IPC fixture suite; live capture is separate.

Windows implements clip saves while recording and shared capture pause in its
branch. Its build/runtime verification remains pending on a Windows host. The
earlier KDE Plasma Settings scroll crash remains open.

## Four-source recording controls — 2026-11-16

- [x] ~~Add independent Game and Music recording volume/mute beside PC and Microphone.~~ Four-track role gains are applied before encoding and preserve independent microphone boost; older settings keep 100% defaults.
- [x] ~~Show the actual configured recording tracks on the dashboard.~~ Desktop PC/Microphone mode is explicitly distinguished from four-track recording.
- [x] ~~Expose a direct Set up four tracks action.~~ It stages the named four-track mode in Audio settings while capture is off; Cancel preserves the existing setup. Game and Music must still be assigned to real applications before Save.
- [ ] Verify a real four-track capture with explicit Game and Music applications. Desktop mode records PC and Microphone; changing its labels cannot separate the existing mix.

These changes are available in the local build used by `linux/run.sh`. They do
not migrate existing source assignments, alter existing recordings, publish a
release or replace the installed AppImage.

## Dynamic application audio — 2026-11-16

- [x] ~~Discover and buffer application audio after capture starts without restarting video or discarding replay history.~~ Native track preparation and continuous-recording finalization run off the video loop.
- [x] ~~Expose per-application Track/Game controls and persist choices.~~ Automatic mode replaces ordinary desktop capture; advanced static routing remains available.
- [x] ~~Present PC/Game/Microphone groups and More tracks.~~ PC contains non-game sources once; expansion changes visibility only and selection/export use physical stream IDs.
- [x] ~~Keep late-source timing in playback, waveforms and exports.~~ Real multistream fixtures cover delayed audio, high stream counts and selected exports.
- [x] ~~Finalize dynamic standalone/simultaneous recordings while preserving originals on failure.~~ Native fixtures cover asynchronous finalization and recoverable write failures; Qt watchdogs renew only on actual progress.
- [ ] Validate live application discovery, game classification, PipeWire synchronization and long-recording recovery on Hyprland/Plasma. Headless fixtures do not establish hardware coverage.

The local native build is used through `linux/run.sh`; no Rust migration, release
publication or installed AppImage replacement is part of this change.

## Linux Discord and appearance update — 2026-11-16

- [x] ~~Show Discord Rich Presence only while capturing a recognized game, and add a Settings switch to disable it.~~ Idle now clears the activity instead of showing an idle card; the switch defaults on and is stored as `discord/presence`. Discord protocol fixtures pass (14); real Discord verification is pending.
- [x] ~~Restrained UI refresh: rounder buttons and sections, accent-colored hover and scrollbar states, no animations.~~ QSS only, no recording overhead. Not visually verified on a running build.
- [x] ~~Unify the trash icon with the other icon strokes.~~ The other icons already shared one stroke style.
- [ ] Overlay performance and laptop issues remain open until concrete cases are described.

## Major changes, first Linux pass — 2026-11-16

- [x] ~~Offer background clip compression only while no game is open (Linux).~~ Opt-in setting `compression/idle` (off by default). While `currentGame()` is empty and nothing is saving or recording, one niced `ffmpeg` job at a time re-encodes H.264 clips older than 10 minutes to HEVC (lossy, CRF 26). The result is written to a hidden temp file, validated with `ffprobe` (HEVC, same stream counts, duration within 0.5 s) and must be at least 10% smaller before it atomically replaces the original; otherwise the original is untouched. A game appearing cancels the job and removes the temp file. Already-HEVC clips are skipped. Fixtures cover candidates, validation, cancel and no-gain paths (6 pass). Game detection only recognizes games that expose audio, and real long-run behavior and Windows parity are unverified.
- [x] ~~Medal-style input overlay assessment.~~ Superseded by phase 1 (2026-11-17, see below). Original findings: Findings: capturing global keys on Wayland needs either evdev access (`input` group, privacy-sensitive) or a compositor shortcut/portal that cannot report arbitrary key state, so a prototype must be opt-in, show only an allowlisted key set, and never persist raw keystrokes. Editable overlays require a separate timestamped event track (small JSON sidecar next to the clip) with the video left clean; a burned-in overlay cannot be removed later. Overhead of a sidecar is negligible; synchronization needs the recording start timestamp. Next step is an evdev-based prototype behind a setting.
- [x] Replay saves during continuous recording and the multitrack audio parity items were already implemented on Linux; they remain open only for live hardware validation (see sections above).
- [ ] KDE Plasma Settings scroll crash: a `QGraphicsOpacityEffect` wrapped the whole capture-settings container (combo boxes, table with cell widgets) inside the scroll area; graphics effects over scrolled child widgets force offscreen re-rendering and are a known crash source on Wayland. The effect is removed; locked fields still dim through the existing `:disabled` styles. This is a probable cause, not a confirmed fix: it stays open until someone scrolls Settings on a real Plasma session.
- Known failing before this pass and unrelated to it: `UiTests::fourTrackDashboardHasIndependentRoleLevels` and `UiTests::dashboardKeepsRecordingVolumeAvailableDuringCapture`.

## Follow-up — 2026-11-16 (compression quality, audio menu)

- Compression is now near-lossless rather than size-first: HEVC CRF 19 / preset slow, same resolution and frame rate, and a replacement happens only if `ffprobe` checks pass and an `ssim` comparison against the original is at least 0.985. Mathematically lossless output is not offered because it would make GPU-encoded clips larger, not smaller; the achieved saving depends on how inefficient the original encode was and is not guaranteed.
- The main menu no longer lists applications as Track/Game rows. It shows only the PC / Game / Microphone groups. Games are classified automatically (known-game catalog or the currently detected game), and Shadowless's own audio is excluded. Per-application tracks appear only in the editor under More tracks.
- Editor audio rows: the PC group is no longer duplicated in More tracks (a group with one source is shown only as its group row). Clicking a group whose members were changed individually asks for confirmation (Cancel / Ok) and Ok turns all of its tracks on; when all are on, one click turns them all off. Waveforms are drawn for every row in playback and editing; clip analysis now starts when a clip opens instead of only in edit mode. Verified with offscreen fixtures only.
- Linux overlay restyled toward the Windows look: no outer window frame on the home page; the header (brand, Capture overlays, Settings, Esc) floats at the screen edges, content is translucent cards over a darker dim, and Settings/editor pages keep a solid surface. Layout checked in an offscreen render only; layer-surface behavior on a real Hyprland/Plasma session is unverified.
- The player shows no audio tracks; they appear only while editing. "PC (other)" is never listed under More tracks. The two failing dashboard tests were outdated (grouped PC/Game audio is now the default) and were updated; UI suite 76/76.

## Input overlay, phase 1 — 2026-11-17

- [x] ~~Medal-style peripheral overlay on Linux inside Capture overlays: five layouts (QWERTY, QWERTY full, arrows, AZERTY, DualSense), live preview, position/size/opacity, drawn on the preview canvas.~~ Opt-in; reads input only while the editor shows it or while saved events are on. Allowlist-only evdev source with hotplug and DualSense mapping; permissions are guided with a narrow `udev` rule, never changed automatically. Unit and simulated-device tests pass; real hardware (keyboard, hotplug, DualSense) is unverified.
- [x] ~~Editable overlay in the clip player over clean video (`<clip>.inputs.json`, schema v1).~~ Opt-in sidecar, atomic writes, removed with the clip. Recorded timing is aligned to the buffer window or recording start and still needs measurement against real clips; paused recordings are skipped.

Phase 2 (burning the overlay into captured frames), KPS, the Windows port and export/drag are tracked in the shared backlog above.
