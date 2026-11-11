# Linux player, capture preview, and moon icon

Status: approved by the user on 2026-11-11, including complete player/editor scope.
Additional requirement: protect clipping performance; no hidden player/analysis work.

## Intent

Bring the native Linux player and capture-overlay preview closer to the current
Windows application. Replace the project mark with a transparent pixel-art moon
based on the supplied reference. Preserve existing local changes and clips.

## Evidence

Linux uses Qt Multimedia: play/pause, seek, volume, and one active audio track.
Windows uses libmpv and FFmpeg: recent-clip navigation, fullscreen with idle
controls, playback speeds and shortcuts, timeline thumbnails and audio waveforms,
simultaneous selected audio tracks, and trim/export with explicit track merging.

Linux OverlayCanvas currently paints layers over a fixed 1920x1080 grid.
Windows previews the capture composition from OBS. Linux already loads a GSR
plugin in the capture process, but its private socket only exchanges scene JSON
and application acknowledgments; it does not transport captured frames.

## Approach

Keep the native C++/Qt/GSR architecture and port the user-visible behavior.
Copying WPF code is incompatible with the Linux target. Opening a second desktop
capture is an alternative, but duplicates capture work and can disagree with the
selected recording source. Prefer an on-demand, bounded preview channel from the
GSR plugin, after validating the framebuffer contents and orientation live.

For playback, evaluate the locally available libmpv integration before expanding
QMediaPlayer: selecting one audio track cannot provide simultaneous track mixing.
Reuse existing FFmpeg libraries and clip discovery rather than adding a second
library or shell-based media command construction.

## Proposed behavior

- Player: previous/next clips, fullscreen and idle controls, keyboard transport,
  speed controls, timeline thumbnails, waveforms, and selected audio-track mixing.
- Editor: choose in/out points and export a new clip; preserve selected streams
  by default and make mixing explicit. Replacement must use a temporary output,
  validate success, and preserve the source on cancellation or failure.
- Overlay editor: show the full capture image with the actual source aspect
  ratio; keep layer positioning consistent with recorded output. Suspend preview
  transport when hidden, bound frame size/rate, and avoid queuing stale frames.
  When capture is off or unavailable, show an explicit state and retain editing.
  Do not silently start recording to obtain a preview.
  Preview must also work with composition disabled or zero layers: the plugin's
  current early return and layer-based damage check must not suppress preview.
  Validate these cases live. A preview with capture stopped would need a separate
  preview-only source and remains a user-visible scope decision.
- Icon: use the generated transparent moon as the source asset, derive PNG/ICO
  sizes, and update Linux desktop/tray/window and Windows icon generation assets.
  Preserve active/inactive indication and accent behavior where currently used.

## Validation

Run the native build and existing offline Qt/GSR suites. Add focused coverage for
navigation boundaries, fullscreen cleanup, selected audio streams, export failure
and cancellation, preview lifecycle, frame dimensions/orientation, and icon alpha.
Use generated media fixtures to validate playback and FFmpeg decoding of exports.
Inspect the real Wayland UI and compare a live preview against a recorded frame.
Windows runtime checks require a Windows environment; report that limit.

## Current deliverable and open decisions

The generated moon PNG is saved at linux/app/assets/shadowless-moon.png; a
multi-resolution shadowless-moon.ico sits beside it (nine sizes, 16–256 pixels).
Both have verified alpha transparency. Derived accent/inactive assets are connected
to application resources in both repositories; Linux desktop identity installs the PNG.
Generated with the built-in image generator from the reference using this brief:
transparent pixel-art crescent, periwinkle outline, pale lavender interior,
plus-shaped sparkles, square stars, no text or background.

The user approved the full scope, including trim and simultaneous audio mixing.
The preview remains unavailable with capture stopped, as proposed; layer editing
continues without implicitly starting capture.
