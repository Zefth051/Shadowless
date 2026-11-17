# Shadowless Linux 0.3.6 — four-track recording and refreshed dashboard

This release adds optional four-track recording (PC / Game / Music / Microphone),
dynamic application audio, pause/resume for continuous recording, a Windows-style
audio dashboard and several editor improvements. Existing clips and settings are
preserved; new features are opt-in.

### Highlights

- **Four-track recording** with independent volume/mute per source and named MP4 tracks.
- **Dynamic application audio:** apps that start after capture begins are picked up without restarting video.
- **Pause/Resume** for standalone continuous recording; basic **Discord Rich Presence**.
- **Dashboard:** stacked full-width audio cards with volume popups, new Game/Music icons, larger capture tiles and a frameless overlay.
- **Editor:** filters by game/mode, waveforms on every row.
- **Opt-in idle compression** of old clips to HEVC with validation.
- **KDE Plasma:** removes the probable cause of the Settings scroll crash (unconfirmed on a real session).

### Known limitations

- Live four-track capture, dynamic discovery, pause/finalization, Discord presence and Plasma behavior are verified only with fixtures.

### Download and compatibility

Download `Shadowless-0.3.6-linux-x86_64.AppImage` and `SHA256SUMS.txt`, verify with
`sha256sum -c SHA256SUMS.txt`, then make the AppImage executable. Keep it in a
permanent writable location for in-app updates.

The recorder and matching KMS helper are bundled; no separate GSR installation is
required. Hyprland or Plasma 6 on native Wayland, compatible graphics drivers and
glibc, PipeWire/pipewire-pulse, FFmpeg/ffprobe, pactl and pw-loopback remain host
requirements. Hyprland additionally uses hyprctl. Direct KMS authorization uses the
system Polkit agent; Shadowless never handles your password or runs its UI as root.

The screenshot gallery contains earlier-build UI and synthetic demo clips; it is
not a benchmark or proof of live Plasma capture.

### Sources and attribution

The release provides the exact build inventory and matching dependency source
materials with checksums. Application and modified GSR source are available from
this release tag through GitHub's source archives. See the included component
notices and dependency audit for source/build mappings.

Derived from Captail by FaulMit; GPL-2.0-or-later attribution is retained. The
combined Linux runtime includes components with GPLv3 requirements. Developed with
AI assistance, including OpenAI Codex.

This Linux release keeps the Windows update channel unchanged.

[Full changelog](https://github.com/Zefth051/Shadowless/compare/linux-v0.3.3...linux-v0.3.6)
