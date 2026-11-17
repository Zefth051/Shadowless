# Recording and audio parity plan

**Goal:** Save Windows replay clips during continuous recording, add honest pause controls, and complete Linux four-source audio and editor selection usability before Discord RPC.

**Design choices:** Reuse the current capture graphs, encoders and native IPC. Windows runs replay and recording outputs on one OBS graph; pausing recording pauses the shared encoded stream, so clipping is unavailable while paused. Linux exposes the engine's pause only for standalone continuous recording; replay-plus-recording stays supported without a misleading pause control. Linux's explicit four-track mode names PC/Game/Music/Microphone, uses application routing to exclude game/music from the PC remainder, and requires genuine source assignment. Existing configurations and old media are preserved. Keep editor selectors on the left of each track, as clarified by the user, with waveforms alongside them. No release/version changes.

## Tasks

- [x] Windows: shared-output lifecycle, save action/hotkey/tray/UI, pause state and errors, stop/finalization, localization and executable QA added. Execution on Windows remains pending.
- [x] Linux: fixed four-track routing/setup/persistence and named media metadata, with source isolation and validation tests.
- [x] Linux: supported standalone pause via IPC, actual-state confirmation, dashboard/tray/indicator and lifecycle fixtures.
- [x] Editor: left-side selectors and real four-track MP4 probe/export regression; preserve track toggles, keyboard interaction and layout.
- [x] Review and run available native/media/UI/source checks; record hardware/Windows/Plasma limits.
- [x] Discord RPC: public Shadowless Application ID supplied and implemented on both branches. No credentials in source; fixtures do not contact Discord.

## Open information

The user supplied Application ID `1571906955054948393` and clarified that editor
selectors belong on the left, like Windows. The source of their centering was
the layout distributing empty space after hiding the waveform; a terminal
stretch preserves their position in both modes.
Live Plasma scroll crash and Windows hybrid-GPU capture remain prior unresolved
runtime reports; these tasks do not establish their resolution.

Review also fixed loss of Windows recording stop errors during disposal and
repeated Linux pause clicks while the engine was still confirming state.
The updated pending-state UI regression was observed failing before the fix.
MP4 handler metadata is retained through probing, editor labels and subset export.

Missing tools: .NET/PowerShell prevent Windows build and executable QA; Meson
prevents rebuilding the full bundled engine here. Unix socket binding is blocked
for adapter IPC fixtures; socket-free adapter checks and socket-pair RPC fixtures
run without contacting the real recorder/Discord services.
