# Audio area

## Modes

Simple mixed mode captures device loopback and optional microphone.
Separate mode uses track 1 for system audio excluding the detected game root,
track 2 for that game process tree, and track 3 for the enabled microphone.
Without a microphone, only tracks 1 and 2 are encoded. Without system capture,
microphone-only capture uses track 1.

Separate mode requires the process-loopback capability and captures across
output devices. With no detected game it excludes Shadowless itself and keeps
track 2 silent. For multiple independent instances of the detected executable,
the oldest root is used consistently for inclusion and exclusion; other
instances remain on the system track. PID creation time protects restarts.

In explicit Game Capture, mixed mode routes the executable detected by the
video hook through process loopback. This follows games that use a non-default
Windows output device. If process loopback is unavailable, Shadowless keeps the
device-loopback fallback.

Advanced mode maps executable roots to recording tracks. Selected processes,
currently audible processes, and other processes are presented separately in
the routing UI. Microphone receives its own configurable track assignment.

AAC uses fragmented MP4; Opus uses MKV. Current format capability model allows
up to six tracks for both.

## Flow and seams

- `Config`: persistent mode, devices, levels, routes, and microphone track.
- `AudioDevices`: endpoint enumeration.
- `ProcessAudioSessionMonitor`: current sessions and live levels.
- `ProcessTopology`: executable roots and child-process grouping.
- `ProcessAudioReconciler`: desired routes versus active OBS sources.
- `ProcessAudioMonitor`: health/recovery cadence.
- `ProcessAudioRoutingWindow`: selection and track UI.
- `native/ProcessAudio`: Shadowless OBS process-loopback source.
- `ObsReplayEngine`: mixers, track names, and source ownership.

## Invariants

- Normalize routes by executable; duplicate valid routes use one deterministic
  assignment.
- Unchecked applications are not recorded in advanced mode.
- Process restarts should reconnect without rebuilding unrelated capture state.
- Volumes, microphone boost, and persistent mute flags apply through the OBS
  scheduler without rebuilding sources, encoders, or the replay buffer. Muting
  retains the selected volume; newly created sources honor the saved mute state.
- In mixed desktop mode, device selection and system/microphone capture toggles
  reconcile only their WASAPI sources on the OBS scheduler. They preserve the
  output, encoder, and replay history. Separate, advanced, and explicit game
  audio changes retain pipeline restart behavior.
- Track names describe actual sources and remain short enough for containers.
- Track count follows highest assigned application/microphone track, up to six.
- Unsupported Windows/source states fall back to an explicit unavailable state,
  never silent mixed capture.
- Before releasing a public internal OBS scene, remove its source from the
  canvas. OBS 32 retains canvas scene references; leaving them for global
  shutdown can double-destroy child sources during an audio toggle restart.

## Validation

```powershell
.\tools\TestIssue41Fixes.ps1
.\tools\TestIssue41Followup.ps1
dotnet build .\Shadowless.sln -c Release --no-restore
```

For native routing changes, also run `tools/ProcessAudioFoundationQa` and the
appropriate `--qa-advanced-audio` recording case described in its README.

For dashboard audio toggle/restart regressions, build
`tools/ProcessAudioFoundationQa/ProcessAudioFoundationQa.csproj` with a shared
output directory (`-o .qa/audio-lifecycle`) so native runtime files are present,
then run `.qa/audio-lifecycle/ProcessAudioFoundationQa.exe --audio-toggle-lifecycle`
in an interactive session. It tests eight real mixed/separate audio rebuilds
without saving clips or changing user settings, and rejects leaked sources or
double destruction in the new log segment.

Build `tools/ProcessAudioFoundationQa/ProcessAudioFoundationQa.csproj` with
`-o .qa/live-audio-levels`, then run its executable with `--live-audio-levels`
to verify real OBS gains and replay continuity, or `--dashboard-audio-levels`
to verify the WPF sliders, input coalescing, mute/restore, and advanced microphone.
The dashboard case uses synthetic settings and writes visual-review PNGs beside
the executable. Neither case saves clips or user settings.

Run the same executable with `--live-audio-sources` on Windows to exercise eight
mixed desktop audio toggles and verify source ownership, mute/gain, idempotency,
unchanged output/encoder/replay window, and continued video encoding.
`dotnet run --project tools/AudioSourceQa -c Release` checks the pipeline restart
classification without native OBS or WPF; it also runs on Linux.
