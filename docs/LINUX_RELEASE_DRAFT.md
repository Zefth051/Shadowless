# Linux / Hyprland AppImage validation draft

This draft is not cleared for distribution. Complete the exact bundled dependency
notices, corresponding source/build inputs and asset provenance in
`docs/DEPENDENCY_AUDIT.md` before sharing or publishing its binaries.

Native Qt/Wayland edition: replay and continuous recording through GPU Screen
Recorder, PipeWire application/microphone tracks, recent clips and game folders,
embedded playback, trimming/audio-track selection, media overlays and shortcuts.
See [platform features](https://github.com/Zefth051/Shadowless/blob/linux/docs/FEATURES.md).

Download the AppImage and `SHA256SUMS.txt`, verify with `sha256sum -c SHA256SUMS.txt`,
make the AppImage executable and keep it in a permanent writable location.
Hyprland, compatible glibc/drivers, GSR, PipeWire and FFmpeg host tools are required.
This is not a generic Linux desktop package; clean-host capture validation remains pending.

Public-enabled AppImages offer verified updates after confirmation and apply them
on the next launch. Private/development previews update manually.

Developed with AI assistance, including OpenAI Codex. Derived from Captail by
FaulMit; GPL-2.0-or-later attribution is retained. The combined Linux runtime
includes dependencies with GPLv3 requirements; see the dependency audit.
