# Rendering status and audio routing implementation plan

> Implement inline with executing-plans; preserve the existing dirty Linux worktree.

Goal: Show rendering activity throughout the control overlay, add a residual PC track
without duplicating selected applications, and remove recording-only virtual devices
from desktop/Discord device selectors while preserving microphone gain and mute.

Architecture: Reuse ClipPlayer export lifecycle, dashboard header and application
track table. Add an optional PC track with one combined inverse application selector.
Investigate replacing Audio/Source loopback endpoints with disconnected playback
streams, retaining only a solution that passes real gain/mute checks.

Constraints: Linux Qt/GSR only; no added dependencies, default device changes,
version bumps, commits, or changes to Windows. Existing routing remains the default.
Music isolation follows the audio application identity, not the website URL.

- [x] Add an export activity signal and shared overlay label. Extend the export/hide/reopen
  regression and cover completion, cancellation and failure clearing the label.
- [x] Add PC track selection to options/settings/UI and emit a single inverse list
  excluding all explicit applications. Test validation, persistence and argument
  mapping, including the prohibition on mixing inverse and explicit apps in a track.
- [x] Investigate private playback streams. Rejected after real recordings produced
  silent standalone desktop and microphone tracks on GSR 6.1.7 / PipeWire 1.6.11.
  Ruling: retain existing virtual inputs and private gain/mute controls; optional
  device removal remains unimplemented. Do not keep failed routing code.
- [x] Build and run the complete offline suite, run synthetic live audio checks,
  document limitations, and request a scoped independent review.

Review focus: No recursive PC capture through private helpers; no microphone echo;
no global volume changes; late applications and multiple inverse exclusions;
rendering label resets on every terminal export outcome.

Validation: linux/build.sh passed (97 Qt test results, 9 opt-in skips). Synthetic
live checks passed four-track isolation, PC-only capture, music starting after
capture, desktop gain and microphone mute, including shared microphone tracks
(7 Qt results, no skips). Scoped independent review found no important defect.
Real Spotify/YT Music clients and long capture sessions were not exercised.
