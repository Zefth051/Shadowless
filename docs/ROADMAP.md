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

- [ ] Refresh the UI and buttons while retaining Shadowless's visual identity.
  Use restrained animations with negligible recording overhead.
- [ ] Improve overlay performance and settings usability. Reproduce and fix laptop
  behavior issues individually; exact cases still need to be described.
- [ ] Refresh icons across both branches.
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
