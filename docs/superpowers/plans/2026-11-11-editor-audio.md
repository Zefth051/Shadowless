# Linux editor audio implementation plan

> Execute inline with executing-plans in the existing isolated Linux worktree.

**Goal:** Display every recorded audio track with usable trim/selection controls
and find a viable capture route without public virtual inputs, retaining gain/mute.

**Architecture:** Reuse ClipPlayer, libmpv track discovery, MediaTools export and
CaptureAudio. Keep existing capture behavior unless a replacement passes real
signal checks. Qt 6 / C++17, GSR and installed PipeWire tools; no dependencies.

**Spec:** ../specs/2026-11-11-editor-audio-design.md

## Constraints and review focus

Preserve the dirty worktree and old clips; no version changes or commits.
Check four-track visibility, silent/unselected tracks, long clips, narrow layouts,
and capture cleanup. Do not change hardware volume/default devices.

## Tasks

- [x] Extend the real FFmpeg/libmpv UI regression to four tracks and assert all
  four controls fit the visible editor viewport; test timestamp entry and toggle
  selection through playback/export. Run the regression before editing production.
- [x] Refine clip_player.cpp and theme.cpp with stable audio rows and compact
  timestamp controls. Reuse existing widget names/signals and export selection.
  Rerun UI regression and inspect a rendered screenshot.
- [x] Investigate GSR/PipeWire routing with synthetic audio. Adopt a replacement
  in capture/audio.* only after nonzero audio, independent gain/mute and cleanup
  are verified. Add offline coverage for the chosen routing and document limits.
- [x] Run linux/build.sh, review the scoped diff, and record actual results here.

## Investigation and rulings

- The four-stream UI fixture was detected before implementation. The failing
  control-size regression and screenshot confirmed tiny native toggles; reserve
  four complete row heights and keep waveform geometry stable instead of claiming
  that stream detection was broken. Current user clips inspected with ffprobe
  contain two streams, consistent with the saved device-capture configuration.
- Private output streams initially remain virtual because pw-loopback sets
  node.virtual=true. Explicit false makes the route appear in GSR's application
  list, but standalone capture remains silent. Active PipeWire graph inspection
  shows no recording connection from that route. Capture-side passive flags and
  an independent pw-cat raw pipe did not resolve it. A simultaneous direct
  application/device comparison recorded the synthetic tone at -21.2 dB, while
  the private route measured -91 dB. No replacement was adopted; public temporary
  input removal remains unresolved. Preserve working gain/mute as explicitly
  requested rather than shipping an unproven capture path.
- Independent scoped UI review found a real hit-testing defect in the new button
  appearance. A right-edge mouse regression failed, then passed after overriding
  the checkbox hit area to match its painted rectangle. Keyboard toggling remains
  native and is covered as well.
- Existing synthetic liveAudioLevels checks pass for desktop devices, application
  plus microphone, and application mixed with microphone (5 Qt results, no skips).
  Verified desktop 50% volume reduces amplitude by about 6 dB and microphone mute
  removes its signal. No physical default devices or global volumes were changed.
- Final linux/build.sh run after the hit-area and keyboard regressions: 100 Qt
  results passed, 9 skipped, zero failures; build exit status 0. Expected offscreen
  and disabled-DBus warnings remain. git diff --check passed for tracked touched
  files. Temporary probe devices/processes were cleaned up. The running desktop
  app still has the previous executable loaded; no forced restart or capture stop.
