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
  Duplication for fullscreen cursor compatibility. A confirmed mismatch between
  the OBS adapter and selected display adapter instead requests WGC (successful
  preflight) or OBS Auto. Unknown ownership preserves the requested backend.
  Detection uses the display's HMONITOR, not its enumeration index; adapter and
  backend decisions are logged once at source creation. The saved setting is
  unchanged. Canvas/source selection uses one startup monitor snapshot.
- Monitor capture starts on DXGI unless confirmed incompatible with the display
  adapter, in which case it starts with the fallback backend. In Auto mode it switches to WGC after Windows
  11 grants borderless access; otherwise it delegates backend selection to OBS
  Auto, including cross-GPU display handling on hybrid laptops. Windows 10 and
  denied/unavailable borderless access may show the system capture border.
- Game mode uses OBS Game Capture directly.
- Replay mode keeps a rolling buffer and saves snapshots on demand.
- AAC replay snapshots use standard MP4 with faststart for Windows player
  duration/seek compatibility; continuous recordings retain fragmented MP4.
- Recording mode keeps the replay buffer and a continuous file output on the
  same capture graph and encoders. Save action/hotkey always saves a replay;
  start/stop controls keep their capture-mode role.
- Recording pause pauses the shared encoded stream. Clip saving is disabled
  until resume, which continues the same file. Stop finalizes the recording
  output before releasing replay and the shared encoders. Forced or failed
  recording stops retain their error instead of reporting a clean save.
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
dotnet run --project .\tools\CaptureSelectionQa -c Release
dotnet build .\Shadowless.sln -c Release --no-restore
```

Use relevant `--qa-game-*`, `--qa-replay-*`, or recording QA entry points for
runtime changes. Real Game Capture changes require a real game test.
CaptureSelectionQa is a portable policy fixture; it does not exercise native
DXGI enumeration or establish that hybrid laptop recordings contain video.

Recording/replay QA: `dotnet run --project tools/ProcessAudioFoundationQa --
--recording-replay` checks shared encoders, clips during recording, pause/resume
and finalization. `--recording-controls` checks the dashboard controls separately.

Discord RPC uses the public application ID and connects only while actively
capturing an unpaused game recognized from the engine's active executable.
Stopped, paused or unrecognized capture and the persisted opt-out clear activity
and disconnect; no idle state is published. The enabled-by-default preference
applies on Done without restarting capture. Changed state/game/artwork wakes the
client; updates are coalesced and unchanged payloads suppressed. It sends no
elapsed-time line or timestamp. The large artwork is the public Shadowless logo;
the small badge uses the game's Discord icon from the same revision-pinned public
source as the embedded catalog. Artwork lookup is optional, once per session,
bounded to 10 seconds and 16 MiB; capture does not depend on a successful lookup.
`dotnet run --project tools/DiscordPresenceQa` validates the payload and IPC
without contacting Discord. Actual client display remains a runtime check.
