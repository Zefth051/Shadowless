# Capture area

## Scope

Replay buffer, continuous recording, desktop/game source selection, encoder
capabilities, output lifecycle, recovery, and capture notifications.

## Flow and seams

- `Config.CaptureMode` selects `replay` or `recording`.
- `App.xaml.cs` owns start/stop/restart policy and serializes pipeline work.
- `SingleThreadTaskScheduler` isolates OBS graph mutations.
- `ObsReplayEngine` builds one graph from normalized `Config`.
- `Config.GpuPreference` selects the OBS graphics adapter through DXGI GPU
  preferences: `high-performance` (default, prefers discrete graphics) or
  `power-saving` (prefers integrated graphics). The selected adapter is resolved
  again on each startup; changing it restarts replay/recording. Windows uses an
  available adapter when the requested GPU class is absent. Encoder capability
  detection continues to run on the selected OBS device.
- `ObsReplayEngine.Overlays.cs` composites image/media layers before the existing
  encoder. Overlay changes use the OBS scheduler without restarting outputs or
  clearing replay history. Disabled layers release their sources; no active
  layers bypass the composition scene. Automatic source switches replace only
  the base layer. Media audio is excluded from every mixer.
- Desktop mode uses monitor capture plus automatic game detection/fallback.
- `Config.DesktopCaptureMethod` defaults to `dxgi`; `dxgi` forces Desktop
  Duplication for fullscreen cursor compatibility and skips the WGC switch.
- Monitor capture starts on DXGI; it switches to WGC only after Windows 11
  explicitly grants borderless access. Denied/unavailable access and Windows 10
  retain DXGI to avoid the system capture border.
- Game mode uses OBS Game Capture directly.
- Replay mode keeps a rolling buffer and saves snapshots on demand.
- AAC replay snapshots use standard MP4 with faststart for Windows player
  duration/seek compatibility; continuous recordings retain fragmented MP4.
- Recording mode writes a continuous file and routes it after clean shutdown.
- `ReplayPaths` validates and organizes completed files.

## Primary files

- `src/Shadowless/App.xaml.cs`
- `src/Shadowless/Config.cs`
- `src/Shadowless/ObsReplayEngine.cs`
- `src/Shadowless/ObsReplayEngine.Overlays.cs`
- `src/Shadowless/CaptureOverlay.cs`
- `src/Shadowless/App.Overlays.cs`
- `src/Shadowless/ObsNative.cs`
- `src/Shadowless/Interop/CaptureInterop.cs`
- `src/Shadowless/ReplayPaths.cs`
- `native/ObsBridge/`

## Invariants

- Never mutate OBS concurrently.
- Internal game-stretch and process-audio scenes are private; drain their
  destruction queue before shutting down OBS so the canvas cannot retain them.
- A failed restart must restore a usable previous configuration when possible.
- Output-folder and replay-organization changes must not restart active capture.
- Stretched game resolutions fill the configured OBS canvas.
- Notifications must not activate or steal input from a game.
- Capture recovery and save completion must update both dashboard and tray.

## Validation

```powershell
.\tools\TestCapturePlayerRegression.ps1
.\tools\TestIssue41Fixes.ps1
.\tools\TestFullscreenOverlayHotkeys.ps1
.\tools\TestGpuSelection.ps1
dotnet run --project .\tools\AudioSourceQa\AudioSourceQa.csproj -c Release
dotnet build .\Shadowless.sln -c Release --no-restore
```

Use relevant `--qa-game-*`, `--qa-replay-*`, or recording QA entry points for
runtime changes. Real Game Capture changes require a real game test.
