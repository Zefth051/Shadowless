# Shadowless Linux 0.3.6 — four-track recording and refreshed dashboard

Existing clips and settings are preserved; new features are opt-in. Update from inside the app (**Check for updates**), then quit and reopen.

## What changed and why

- **Four-track recording (PC / Game / Music / Microphone).** Streamers and editors wanted to rebalance game, music and voice after the fact. Each source gets its own volume and mute, a named track inside the MP4, and can be exported on its own. It needs Game and Music assigned to real applications; old modes and files are untouched.
- **Dynamic application audio.** Apps that start playing after capture begins (a launcher, Discord, a browser) used to be missed. They are now discovered and buffered without restarting video or losing replay history. Per-application tracks appear in the editor under *More tracks*.
- **Pause / Resume** for standalone continuous recording, so loading screens do not end up in the file.
- **Discord Rich Presence (basic):** capture state and detected game name, via the local Discord client only.
- **Idle clip compression (opt-in, off by default).** While no game is open, old H.264 clips can be re-encoded to HEVC to save disk space. The original is replaced only if validation and an SSIM quality check pass; otherwise it is left untouched.
- **Editor:** filter the sidebar by game folder and recording mode, waveforms on every audio row, track selectors kept on the left.
- **Dashboard and overlay:** the audio sources are stacked, full-width cards in the Windows style (mute icon, name, divider and a `100%` button opening a volume popup) with new Game/Music icons and larger Instant Clip / Recording tiles. The home page is frameless with a floating header. This was done so both platforms look and behave alike.
- **KDE Plasma:** removed a graphics opacity effect over the scrolled Settings page, the probable cause of the reported crash when scrolling Settings on Plasma. Not yet confirmed on a real Plasma session; please report if it persists.
- **Editor waveforms** keep a dark background and stay readable with custom accents.

## Known limitations

- Live four-track capture, dynamic application discovery, pause/finalization and Discord presence were verified with test fixtures only. Live Plasma validation is pending.

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
