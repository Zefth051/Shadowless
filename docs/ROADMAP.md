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

## Windows appearance update — 2026-11-16

- [x] ~~Support validated #RRGGBB accents alongside the existing five presets.~~ Edits remain staged; invalid values block Done, Cancel discards edits, and accent-only changes do not restart capture.
- [x] ~~Keep custom-accent text, icons and focus states readable on dark surfaces.~~ Literal colors remain on filled controls, with automatic contrasting foregrounds; preset colors and packaged icon assets are retained.
- [ ] Run the new custom-accent WPF QA and visually verify settings and the editor on Windows. The current Linux environment lacks .NET and PowerShell; static XML/localization checks do not establish a passing Windows build.
- [ ] Validate laptop capture with Auto and denied/unavailable borderless access on Windows 11. The local backend-selection fix retains explicit DXGI compatibility mode; real hybrid-GPU capture remains unverified.

Linux editor waveform readability, lighter borders and sidebar navigation are
being updated independently on the Linux branch. Its reported Plasma settings
scroll crash remains under investigation, without a speculative fix.

## Recording, audio and Discord follow-up — 2026-11-16

- [x] ~~Implement Windows replay saves during continuous recording using shared encoders.~~ Native lifecycle and WPF control QA added; actual Windows execution remains pending.
- [x] ~~Add Windows recording Pause/Resume and retain the same recording file.~~ Pause also pauses shared replay capture; clip saves are disabled until resume. Forced/failed finalization is reported as an error.
- [x] ~~Implement basic Discord Rich Presence on both platform branches.~~ Uses public Application ID `1571906955054948393` and sends capture state, a recognized game name and public artwork through local IPC, without a timer or periodic unchanged updates. Client absence does not block capture. Protocol fixtures exist; real Discord verification is pending.
- [ ] Run Windows Release build, `--recording-replay`, `--recording-controls`, and `tools/DiscordPresenceQa` on a supported host. The current environment lacks .NET and PowerShell.

Linux adds explicit PC/Game/Music/Microphone routing for new captures and
standalone recording pause. Pause during simultaneous replay/recording is not
supported by the bundled GSR engine. Old mixed/two-track clips are preserved.

## Windows presence and button refresh — 2026-11-16

- [x] ~~Limit Windows Discord presence to active capture of a recognized game.~~
  Stopped, paused and unrecognized capture clears activity and disconnects.
  There is no idle activity or elapsed timer. Updates wake on changes, coalesce
  and suppress unchanged payloads; an absent Discord client never blocks capture.
- [x] ~~Add an enabled-by-default, persisted Windows Discord presence setting.~~
  Done applies the staged preference immediately: disabling clears/disconnects
  and cancels connection retries; enabling re-evaluates capture. All 11 language
  dictionaries include the label and explanation.
- [x] ~~Make a restrained Windows shared-button refresh.~~ Accent, subtle and
  icon buttons gain press/focus feedback and consistent disabled states while
  preserving custom accents, layout and Shadowless styling. No new animations,
  effects or continuously running UI work are introduced.
- [x] ~~Refresh safe Windows dashboard icons with vector Path data.~~ Library
  refresh replaces a font glyph; close buttons share existing close geometry.
- [ ] Complete the cross-platform UI/icon refresh. This pass changes only the
  Windows shared buttons and the safe icons above; packaged branding and other
  icons are retained. The shared Medium changes remain open.
- [ ] Reproduce specific laptop/overlay runtime issues on Windows. The scoped
  implementation follow-up below does not establish hardware validation.
- [ ] Run the Windows Release build, Discord protocol fixtures, real Discord
  stop/pause/recognition/disable checks and visual/localized WPF checks. This host
  has neither .NET nor PowerShell. Static XML, localization parity and source
  review are the available validation; they do not establish runtime success.

## Windows medium follow-up — 2026-11-16

- [x] ~~Replace remaining decorative notification/help/layer-action glyphs with
  bundled vectors using one rounded stroke style.~~ Branding, accents and
  shortcut key legends are retained; the shared cross-platform item stays open.
- [x] ~~Reduce overlay background work and improve layer settings.~~ Hidden or
  minimized editors stop preview polling; normal capture status no longer
  rotates continuously. Unchanged native placement and opacity allocations are
  skipped. Settings writes debounce; position sliders stay live, percentage
  readouts stay visible, and unavailable layer-order actions disable.
- [x] ~~Audit Windows adapter/output selection and add a guarded fallback.~~
  Canvas/source share a monitor snapshot. Native DXGI enumeration matches its
  HMONITOR to the output-owning adapter. Confirmed cross-adapter DXGI requests
  fall back to preflight-ready WGC or OBS Auto without changing saved settings.
  Unknown ownership (including an older bridge) preserves selection and logs
  the limitation. Selection/fallback diagnostics add no frame polling/readback.
- [ ] Black laptop recordings are **not verified fixed**. Test native bridge
  enumeration, Auto/DXGI, denied borderless permission, internal/external displays,
  both GPU preferences, monitor unplug/replug and real Game Capture on Windows.
- Validation here: 20 XAML files parse, 50 icon resource references resolve,
  all 11 localization key sets match, and 119 extracted existing source-regex
  assertions pass using Python; `git diff --check` passes. These are static
  checks, not PowerShell script or runtime test execution.
- Pending: Release build, `tools/CaptureSelectionQa` (10 portable policy cases),
  `tools/CaptureOverlayQa -- --unit`, notification accent QA and existing Windows
  regressions. Neither .NET nor PowerShell exists here; build/context commands
  and the portable fixture attempt return command-not-found. Visual/localized
  WPF checks and recording/overlay CPU/GPU overhead measurements remain unrun.

Files: shared theme and notification/editor XAML/code; indicator and OBS overlay
code; OBS engine, GPU helper, native bridge and new capture policy; capture and
accent QA plus portable CaptureSelectionQa; UI/capture guidance and this roadmap.
All changes stay local to the Windows worktree, uncommitted and unpublished.
