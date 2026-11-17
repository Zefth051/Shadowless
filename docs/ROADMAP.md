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
    next published release is still unverified.
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

## Proposed delivery order

1. Finish and validate the immediate distribution work.
2. Check existing behavior in both branches; finish game-folder organization on Windows.
3. Fix reproducible laptop/overlay issues, then refresh the UI.
4. Validate concurrent clipping/recording and audio separation on real hardware; test the
   input overlay on a real desktop before designing KPS or the Windows port.

Public visibility and release publication remain separate owner decisions.

## Platform logs

The dated sections below are implementation logs for one platform. They record the
evidence and verification limits behind the statuses above.

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
