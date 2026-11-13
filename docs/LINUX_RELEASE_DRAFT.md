# Shadowless Linux 0.3.3

Native replay recording for Hyprland on Wayland, with the maintained GSR engine
included and public updates enabled. This is a validation draft: the workflow
uploads the AppImage, checksums and build inventory, but not the matching
dependency source-materials archive. Keep it unpublished until that archive is
matched to the exact build inventory and uploaded. The application-source archive
matches the build commit.

Native Qt/Wayland edition: replay and continuous recording through GPU Screen
Recorder engine maintained in this branch, PipeWire application/microphone tracks, recent clips and game folders,
embedded playback, trimming/audio-track selection, media overlays and shortcuts.
See [platform features](https://github.com/Zefth051/Shadowless/blob/linux/docs/FEATURES.md).

Settings now provide a direct link to the independent login-launch and Instant
Clip-on-launch options. The application-audio list also displays icons from the
Linux icon theme when available.

Download the AppImage and `SHA256SUMS.txt`, verify with `sha256sum -c SHA256SUMS.txt`,
make the AppImage executable and keep it in a permanent writable location.
The GSR 6.1.7 engine and matching helper are included, without GSR's UI or a
separate GSR installation. Hyprland, compatible glibc/drivers, PipeWire and FFmpeg
host tools are required. Direct KMS capture may request administrator authorization
through Polkit; the Shadowless interface stays unprivileged.
This is not a generic Linux desktop package; clean-host capture validation remains pending.

The earlier 0.2.4 draft AppImage checksum and native Hyprland UI startup were verified on
2026-11-12. See [six real screenshots and exact artifact provenance](https://github.com/Zefth051/Shadowless/blob/linux/docs/screenshots/linux/README.md).
Library/player/editor images use synthetic demo media; they are not capture benchmarks
and do not validate this candidate's new engine or packaging.

Public-enabled AppImages implement checksum-verified updates after confirmation
and apply them on the next launch. A real in-app update between released versions
is still untested. Builds with public updates disabled update manually. This release includes Qt's TLS backend and OpenSSL runtime for HTTPS and uses
the required pipewire-pulse endpoint for playback to avoid the libmpv native
PipeWire initialization deadlock. Older update-disabled builds need one manual
upgrade; subsequent updates are downloaded and installed inside Shadowless.

Developed with AI assistance, including OpenAI Codex. Derived from Captail by
FaulMit; GPL-2.0-or-later attribution is retained. The combined Linux runtime
includes dependencies with GPLv3 requirements; see the dependency audit.

Windows releases use their own tags and installer; this Linux release does not
replace the repository's latest Windows release. Native Windows GPU capture and
full cross-platform hardware coverage are not established by these package tests.
