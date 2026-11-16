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
