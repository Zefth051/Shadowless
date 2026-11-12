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

The 0.2.4 draft AppImage checksum and native Hyprland UI startup were verified on
2026-11-12. See [six real screenshots and exact artifact provenance](https://github.com/Zefth051/Shadowless/blob/linux/docs/screenshots/linux/README.md).
Library/player/editor images use synthetic demo media; they are not capture benchmarks.

Public-enabled AppImages implement checksum-verified updates after confirmation
and apply them on the next launch. A real in-app update between released versions
is still untested. This private/development preview updates manually.

Developed with AI assistance, including OpenAI Codex. Derived from Captail by
FaulMit; GPL-2.0-or-later attribution is retained. The combined Linux runtime
includes dependencies with GPLv3 requirements; see the dependency audit.
