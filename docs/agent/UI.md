# UI area

## Scope

Dashboard, settings, replay library, localization, accent themes, hotkeys,
overlay notifications, and recording indicator.

## Structure

- `SettingsWindow`: interactive fullscreen control overlay and staged settings
  editor. Uses the foreground monitor, a translucent scrim, Replay/Recording
  tiles, and a side-by-side replay library. A configurable hotkey (Alt+Z by
  default) toggles it when available; edit it in Settings > Hotkeys.
  Escape returns from settings or closes the dashboard. The tray remains an
  entry point if another app owns the overlay hotkey.
- `ProcessAudioRoutingWindow`: advanced audio assignment.
- `ClipEditorWindow`: preview and trim UI; see `PLAYER.md`.
- `CaptureOverlayEditorWindow`: capture composition editor, opened from the
  dashboard header as an owned modal over the control overlay, without a taskbar
  entry. Escape returns to the dashboard. The dashboard card toggles composition
  live without removing layers. Shares theme tokens and previews the OBS texture directly.
  Master/layer toggles apply live; hidden/minimized editors detach their display.
- `OverlayNotificationWindow`: click-through transient notifications. Both native
  `WM_NCHITTEST` (`HTTRANSPARENT`) and WPF hit testing pass input through,
  including to the Settings button underneath a topmost notification.
- `ReplayStatusIndicatorWindow`: optional capture status surface.
- `DisplayIdentifierWindow`: numbered display overlay.
- `Themes/Theme.xaml` and `ThemeManager`: shared resources and accent selection.
- `Languages/Strings.*.xaml` and `Localization`: dynamic localized strings.

## Invariants

- Opening the control overlay and entering Settings never wait for the capture
  pipeline gate. Capture actions disable individual operation controls only;
  Settings navigation and the window stay enabled during replay shutdown.
- Settings edits are staged. Done applies; Cancel discards. Closing dirty
  settings shows the save/discard notice and keeps the window open.
- Entering settings dismisses the previous dashboard notice. Validation and
  save failures raised inside settings remain visible normally.
- Appearance supports preset accents and validated #RRGGBB values. Custom
  colors remain staged until Done; invalid input blocks saving. Filled controls
  retain the selected color while readable accent tokens serve text, icons and
  focus indicators. Custom colors use the existing violet application icon.
- Windows-login autostart and replay-on-launch are independent. Replay startup
  requires explicit opt-in, including for older configurations; manual and
  background launches use the same preference and start replay rather than a
  continuous recording. Capture-recovery relaunches retain the active capture
  state. Changing the preference only affects the next normal launch.
- Recording bitrate includes 35 Mbps and custom whole numbers from 1 to 100 Mbps;
  zero retains adaptive mode. Invalid custom input blocks Done without discarding edits.
- Settings apply failures name the failing option or operation and retain the
  original error. Capture restart failures list the changed capture options when
  the encoder cannot identify a single cause. File write failures identify the config file.
- Dashboard Instant Clip opens an enable/disable and save menu. Recording opens
  a menu with start, pause, resume and stop actions; both outputs can run
  together. Menu availability follows active outputs and paused state. Enter or
  Space opens each menu; arrow keys and Escape use native WPF menu navigation.
- Dashboard audio icons mute/unmute without disabling capture or resetting the
  buffer. Adjacent labels/percentages open live volume sliders; microphone controls
  also remain available in advanced mode. Enabling a disabled source still rebuilds capture.
- Every localization key exists in all dictionaries with matching placeholders.
- Discord Rich Presence is enabled by default and staged with other settings.
  Done immediately re-evaluates it without restarting capture. It connects only
  for active, unpaused capture of a game recognized from the capture engine's
  active executable; stopped/paused/unknown capture and opt-out clear activity
  and disconnect. Missing Discord never blocks capture. No elapsed timer or
  periodic presence polling is used; changed payloads are coalesced and
  unchanged payloads suppressed. `tools/DiscordPresenceQa` covers the IPC contract.
- Shared accent, subtle and icon buttons use accent-aware keyboard focus and
  press feedback without added animations. Dashboard refresh/close icons use
  shared vector geometry; packaged branding remains unchanged.
- Notifications, help badges and layer-order controls use bundled vector paths
  with a shared rounded stroke style. Shortcut key legends remain text.
- The overlay editor detaches preview and stops statistics polling while hidden
  or minimized. Position sliders update live; persistence is debounced and
  flushed on explicit Apply/close. Percentage readouts remain visible and layer
  ordering buttons disable at the list edges. Normal active status is static;
  native indicator placement skips unchanged bounds unless topmost repair is
  needed. Performance still requires measurement on a Windows capture host.
- Replay saves play a bundled confirmation sound only after the file is saved,
  independently of notification visibility. Sound failures do not fail the save.
- Frame rates of 120, 144, and 240 FPS appear separately as experimental with
  a playback warning; 30 and 60 FPS remain the standard choices.
- Capture overlay position sliders span the available travel: 0% aligns to the
  top/left edge and 100% to the bottom/right edge. Stored positions remain canvas
  coordinates. Dragging, resizing, and loading keep the entire layer inside the
  canvas; resizing preserves its top-left position until it reaches an edge.
- Public repository UI copy is authored in English, then localized.
- Accent changes use shared dynamic resources, not isolated hard-coded colors.
- About links use the packaged-safe external URI launcher.
- Passive notification/indicator overlays never activate, capture pointer input,
  or appear in taskbar. The control overlay intentionally activates and accepts
  input, stays above desktop windows, and remains visible to desktop capture.
  It is a desktop-composed WPF surface; exclusive fullscreen and capture of
  only the game's render target require an in-game rendering integration.
- Switching to another application closes the control overlay. An open capture
  overlay editor finishes its normal Escape/save path before closing the dashboard.
  Normal unsaved-settings and save-failure guards still apply.
  Moving focus between Shadowless windows and native dialogs keeps the overlay open.

## Validation

```powershell
.\tools\TestReportedUiRuntimeRegressions.ps1
.\tools\TestStartupUiSurfaces.ps1
.\tools\TestLocalizedLayout.ps1
.\tools\TestNextFeatureRegression.ps1
dotnet run --project .\tools\ProcessAudioFoundationQa -- --bitrate
dotnet build .\Shadowless.sln -c Release --no-restore
```

For visual changes, inspect real windows at supported localized layouts. A
successful build alone is not UI verification.
