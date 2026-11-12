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
- `OverlayNotificationWindow`: click-through transient notifications.
- `ReplayStatusIndicatorWindow`: optional capture status surface.
- `DisplayIdentifierWindow`: numbered display overlay.
- `Themes/Theme.xaml` and `ThemeManager`: shared resources and accent selection.
- `Languages/Strings.*.xaml` and `Localization`: dynamic localized strings.

## Invariants

- Settings edits are staged. Done applies; Cancel discards. Closing dirty
  settings shows the save/discard notice and keeps the window open.
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
- Main replay/recording selector applies immediately and shows only controls
  relevant to the selected mode.
- Dashboard audio icons mute/unmute without disabling capture or resetting the
  buffer. Adjacent labels/percentages open live volume sliders; microphone controls
  also remain available in advanced mode. Enabling a disabled source still rebuilds capture.
- Every localization key exists in all dictionaries with matching placeholders.
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
