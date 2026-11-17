# Dynamic application audio

Status: approved. Prioritize Linux dynamic tracks using the current native engine; defer Rust migration.

## Requested behavior

- Discover audio applications throughout the capture session, including applications opened after buffering begins.
- Give each application a Track control.
- Present three primary editor rows: PC, Game and Microphone. The request lists these three despite referring to two tracks.
- PC includes all desktop playback across output devices except the game, including music, browser audio and notifications. Automatic application capture does not filter by the old output-device selector; the UI hides that selector and preserves its value for legacy modes. The microphone remains independent.
- More tracks expands independently editable application sources without altering the audible selection merely by expanding it.
- Continue using the current native Linux build through linux/run.sh and the existing user settings. Do not install or publish a replacement AppImage.

## Required recording changes

Application stems must be buffered from the first available audio samples, rather than reconstructed from an existing PC mix. An application Track control changes inclusion; it cannot recover independent samples that were never recorded. A closed or silent application's track retains its identity and timing when it resumes. Audio-node discovery identifies producing applications; discovering silent windows also requires process/window identity and an explicit pending-audio state.

Use stable stream identity internally and readable application labels in the UI. application.name alone can combine browser tabs and multiple processes. Shared application identities are not per-tab separation; the UI must state this limit.

New sources must not restart the video encoder or discard the existing replay buffer. Track timestamps use the capture clock, with silence before a late source starts. Do not shift a late application's first packet to the beginning of the saved clip.

## Proposed PC behavior

Preferred: PC is a group of all non-game application stems plus any unassigned desktop remainder. The collapsed group includes the full PC mix, while More tracks exposes its children. A source is included once. Expanding/collapsing changes visibility only; child selection changes the PC group selection. The microphone and game are separate groups/sources.

Alternative: store a complete PC mix as another encoded stream alongside the individual application stems. This duplicates recording data. The editor must provide mutually exclusive mix/stem selection to avoid playing the same application twice, and a selected application's samples cannot be removed cleanly from an already compressed PC mix. The user chose PC as a group; an additional overlapping PC mix is not part of the selected design.

## Engine approach

Extend the bundled engine's recording streams and replay buffer, retaining one video capture process. Its replay packets already carry stream_index, and saved replay muxers construct headers on demand. This permits building the stream list when saving a replay.

Changes required before enabling dynamic tracks:

- Replace reallocating live audio-track storage with stable track allocations; current audio threads keep pointers to track objects.
- Snapshot codec parameters and stream ownership while saves run concurrently with discovery.
- Map stable source IDs to each saved output's contiguous stream indices.
- Preserve capture-clock offsets for late tracks; current replay saving resets each stream's first packet independently.
- Include application identity and editor grouping metadata in the saved clip and preserve it during trimming/export.
- Keep memory/disk usage bounded and surface capture failures rather than silently dropping requested sources.

Continuous recording and simultaneous replay recording already write their container headers at the start. New streams cannot be appended transparently to those outputs. Supporting the same behavior requires private audio stems or segments with shared timestamps and a validated final remux. Preserve originals and temporary recording data until successful finalization. Starting one complete video recorder per application would duplicate capture/encoding and is rejected.

## UI and validation

Use existing Qt controls, left-aligned selectors, keyboard activation, accessible names and restrained static expansion. More tracks must not change audio on its own. Existing two-track clips remain two-track files; do not fabricate Game or application tracks for them.

Regression coverage must include a source appearing after buffering starts, silent/restarted applications, stream identity collisions, game exclusion from the PC group, independent application selection, no duplicate playback/export, saved timing and continuous-recording finalization failures. Exercise real MP4/MKV media output as well as hardware-free routing fixtures. Live PipeWire testing remains necessary for graph transitions and synchronization.
