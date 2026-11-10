# Capture overlays

Open **Capture overlays** from the dashboard header. Add a local PNG, JPEG,
BMP, WebP, GIF, MP4, WebM, MOV, or MKV. GIFs/videos loop with their audio muted.
English and Spanish copy is provided; other dictionaries currently use English
fallback copy for this editor.

The top entry in the layer list renders above later entries. Select a layer,
drag anywhere in the preview to move it, or use the percentage sliders. Width
and height define a fit box: the media retains its aspect ratio. Settings save
automatically. The master switch and each layer's visibility switch affect only
new frames. Previously buffered frames keep their original composition.

Start recording or replay capture from the dashboard to enable the preview.
The editor is excluded from desktop capture on supported Windows sessions. A
hidden, minimized, or closed editor releases its preview display; active media
continues to appear in captured frames. A disabled layer releases its decoder.
Toggling a layer back on restarts its animation. Full-opacity layers avoid the
opacity filter; with no active layers the video uses the original direct path.
The editor does not rewrite existing clips or change encoder/output settings.

## Performance and validation

Overlay composition and animated-media decoding consume resources. Prefer
assets sized for their displayed area. The editor reports OBS render rate and
render lag, not game FPS. The preview itself also adds work; close it for a
meaningful gameplay comparison.

Run the local QA harness with `tools/TestCaptureOverlays.ps1`. Its runtime modes
need an interactive Windows session and access to the ordinary OBS cache.
Generated assets/clips and metrics stay under `.qa/capture-overlays` by default.
The harness never loads or saves the user's configuration.

```powershell
.\tools\TestCaptureOverlays.ps1 -Mode unit
.\tools\TestCaptureOverlays.ps1 -Mode replay
.\tools\TestCaptureOverlays.ps1 -Mode recording
.\tools\TestCaptureOverlays.ps1 -Mode ui
```

Runtime checks exercise overlay loading, output/encoder identity, disabled
source cleanup, missing-file fallback, muted media audio, opacity, repeated
toggles, replay saving, and recording finalization. Every generated clip is
decoded with FFmpeg using `-xerror`. The UI mode uses disposable QA configuration
and deliberately allows screenshots of its editor window for visual validation.

For actual gameplay, compare the same scene and FPS cap with the editor closed:
overlays disabled, a static image, then the intended GIF/video. Compare game FPS
and frame times, OBS render lag, and saved-clip playback. Short synthetic tests
do not establish gameplay performance or behavior in exclusive-fullscreen games.
