# Windows and Linux roadmap

Requested on 2026-11-12. This is a shared backlog, not a release commitment.
Every product feature targets both branches. Work proceeds gradually; the order
below is a proposal, not an approved implementation schedule. Checked, struck
items are implemented; platform verification limits are recorded separately.

## Immediate distribution work

- [x] ~~Windows: enable public updates in Release builds, accepting only published
  final Windows releases, excluding drafts and prereleases even on `0.x`.~~
  Automated policy tests and native Windows CI pass; installer validation is
  tracked in release status.
- [x] ~~Linux: keep a maintained copy of GSR engine source in the Linux branch
  and bundle its build with Shadowless, without a separate GSR installation or
  graphical interface.~~ GSR 6.1.7 and its matching helper are bundled; Shadowless
  retains subprocess integration. Build, source provenance, engine selection,
  forced-stop handling and AppImage startup checks pass. Live KMS authorization
  and capture with the bundled helper still need interactive validation.

## Minor changes

- [x] ~~Two independent settings: launch Shadowless at login and start replay
  buffering whenever Shadowless launches, including manual launches.~~
  Both branches implement opt-in settings. Recovery preserves active capture.
  Linux supports XDG and the default Hyprland Lua/legacy configuration paths.
- [ ] Let users choose the UI accent color using a HEX value.
- [ ] Organize clips into the correct game folders. Check detection and fallback
  behavior, not just the folder creation toggle; preserve existing files.

## Medium changes

- [ ] Fix black recordings on laptops. Reproduce integrated/discrete GPU and
  hybrid graphics capture on both platforms; validate saved video playback
  before claiming laptop compatibility.
- [ ] Refresh the UI and buttons while retaining Shadowless's visual identity.
  Use restrained animations with negligible recording overhead.
- [ ] Improve overlay performance and settings usability. Reproduce and fix laptop
  behavior issues individually; exact cases still need to be described.
- [ ] Refresh icons across both branches.
  Linux now uses a bundled SVG for recent-clip refresh, matching the existing
  icon stroke and avoiding font-dependent glyph rendering. Other icons and
  Windows parity remain pending.
  The editor also uses a film icon for clips without a thumbnail, keeping it
  distinct from the monitor icon used for capture settings.
- [ ] Linux: fix the app icons shown in audio source lists. Audio routing works,
  but the icons still render incorrectly in the public AppImage; reproduce and
  validate the icon lookup/rendering before closing this item.
- [x] ~~Add an open-containing-folder button between playback and deletion in both
  branches, matching the supplied screenshot.~~ Windows already had this action;
  Linux now opens the clip's actual directory, including game subfolders.
- [ ] Add simple Discord Rich Presence that works without user configuration.

## Major changes

- [ ] Add camera capture from local webcams (V4L2 on Linux) and compatible virtual-camera
  sources, including Camo/DroidCam-style network cameras. Assess source discovery,
  synchronization, permissions and platform support before choosing the capture path.
- [ ] Add a KPS overlay as a separate input-overlay feature. Define the counting window,
  display behavior and privacy controls; derive it from the input event stream without
  storing typed text.
- [ ] Deliver the input overlay in phases: (1) opt-in live Settings preview with an
  allowlisted layout; (2) timestamped sidecar events and editable overlay playback in the
  editor over clean video; (3) layout/style controls and parity across Windows and Linux.
  Validate input permissions, synchronization, privacy and capture overhead at each phase.
- [ ] Add a Medal-style overlay for pressed keys, KPS, and future statistics.
  Investigate moving or removing it in the clip editor. An overlay burned into
  captured frames cannot simply be removed to recover the original image;
  editable overlays need separate event/layer data and clean video underneath.
  Assess storage, synchronization, input permissions, privacy and overhead first.
- [ ] Offer background clip compression only while no game is open. Preserve
  originals until successful validation. Distinguish mathematically lossless
  compression from visually similar lossy compression; reduced size without
  any quality loss is not guaranteed for already compressed footage.
- [ ] Allow saving replay clips during continuous recording. Linux already exposes
  this through one GSR process; verify behavior and Windows parity before
  implementing additional capture pipelines.
- [ ] Complete separate audio tracks and independent controls for PC, game, music,
  and microphone. Define PC as the remainder when app sources are separated
  to avoid duplicating game/music audio. Check platform routing support and
  distinguish capture controls from editor playback/export controls.
- [ ] Linux: bring multitrack audio capture, editor playback and export to Windows
  parity. Investigate the reported editor showing only two audio tracks; expose
  every recorded track and preserve independent selection and export.

## Proposed delivery order

1. Finish and validate the immediate distribution work.
2. Check existing behavior in both branches; finish startup options, game-folder
   organization, the containing-folder button and HEX accent customization.
3. Fix reproducible laptop/overlay issues, then refresh UI and add Discord RPC.
4. Validate concurrent clipping/recording and audio separation; prototype editable
   input overlays and idle compression before committing to their designs.

Linux already documents optional organization by game and concurrent replay /
recording. These remain unchecked until the requested behavior is reviewed across
both branches. Implementation checks do not establish full hardware coverage.

Startup/config tests pass on both platforms; Linux UI and command-execution
fixtures pass. A real Linux logout/login cycle, custom Hyprland config locations,
Windows GPU capture and public-release UI upgrades remain unverified.
Public visibility and release publication remain separate owner decisions.

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
- [ ] Medal-style input overlay: assessed, not implemented. Findings: capturing global keys on Wayland needs either evdev access (`input` group, privacy-sensitive) or a compositor shortcut/portal that cannot report arbitrary key state, so a prototype must be opt-in, show only an allowlisted key set, and never persist raw keystrokes. Editable overlays require a separate timestamped event track (small JSON sidecar next to the clip) with the video left clean; a burned-in overlay cannot be removed later. Overhead of a sidecar is negligible; synchronization needs the recording start timestamp. Next step is an evdev-based prototype behind a setting.
- [x] Replay saves during continuous recording and the multitrack audio parity items were already implemented on Linux; they remain open only for live hardware validation (see sections above).
- [ ] KDE Plasma Settings scroll crash: a `QGraphicsOpacityEffect` wrapped the whole capture-settings container (combo boxes, table with cell widgets) inside the scroll area; graphics effects over scrolled child widgets force offscreen re-rendering and are a known crash source on Wayland. The effect is removed; locked fields still dim through the existing `:disabled` styles. This is a probable cause, not a confirmed fix: it stays open until someone scrolls Settings on a real Plasma session.
- Known failing before this pass and unrelated to it: `UiTests::fourTrackDashboardHasIndependentRoleLevels` and `UiTests::dashboardKeepsRecordingVolumeAvailableDuringCapture`.

## Follow-up — 2026-11-16 (compression quality, audio menu)

- Compression is now near-lossless rather than size-first: HEVC CRF 19 / preset slow, same resolution and frame rate, and a replacement happens only if `ffprobe` checks pass and an `ssim` comparison against the original is at least 0.985. Mathematically lossless output is not offered because it would make GPU-encoded clips larger, not smaller; the achieved saving depends on how inefficient the original encode was and is not guaranteed.
- The main menu no longer lists applications as Track/Game rows. It shows only the PC / Game / Microphone groups. Games are classified automatically (known-game catalog or the currently detected game), and Shadowless's own audio is excluded. Per-application tracks appear only in the editor under More tracks.
- Editor audio rows: the PC group is no longer duplicated in More tracks (a group with one source is shown only as its group row). Clicking a group whose members were changed individually asks for confirmation (Cancel / Ok) and Ok turns all of its tracks on; when all are on, one click turns them all off. Waveforms are drawn for every row in playback and editing; clip analysis now starts when a clip opens instead of only in edit mode. Verified with offscreen fixtures only.
- Linux overlay restyled toward the Windows look: no outer window frame on the home page; the header (brand, Capture overlays, Settings, Esc) floats at the screen edges, content is translucent cards over a darker dim, and Settings/editor pages keep a solid surface. Layout checked in an offscreen render only; layer-surface behavior on a real Hyprland/Plasma session is unverified.
- The player shows no audio tracks; they appear only while editing. "PC (other)" is never listed under More tracks. The two failing dashboard tests were outdated (grouped PC/Game audio is now the default) and were updated; UI suite 76/76.
