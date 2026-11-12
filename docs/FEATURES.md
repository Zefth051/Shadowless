# Platform features

Shadowless has two maintained platform branches in one repository. Releases are
shared by the repository, so select the tag for your operating system.

| Feature | `windows` | `linux` |
| --- | --- | --- |
| Platform | Windows 10 2004+ / 11 x64 | Hyprland on native Wayland, x86_64 |
| Interface / capture | WPF/.NET 10 and libobs | Qt 6 and external GPU Screen Recorder |
| Packages | Inno Setup installer and Portable ZIP | AppImage |
| Replay / recording | RAM replay buffer and continuous recording | GSR replay buffer and continuous recording, including simultaneous modes |
| Video | Desktop / direct game capture, supported hardware encoders | Monitor capture through GSR, supported hardware encoders |
| Audio | Desktop/game, microphone, per-application routing and separate tracks | PipeWire desktop/microphone, application tracks and optional unassigned-PC track |
| Clip library | Recent clips, game folders, playback and deletion | Recent clips, game-folder filters, playback and desktop Trash |
| Editing | Trimming, audio-track selection and configurable export | Trimming, audio-track selection/merge and stream-copy export |
| Overlays | Composited media layers, live preview editor and optional passive capture status indicator | Hyprland control panel, optional passive indicator and composited media layers |
| Shortcuts | Configurable Windows global hotkeys | Desktop portal and temporary Hyprland bindings |
| Updates | Confirmed in-app download/install for public Setup/Portable builds | Confirmed, SHA-256-verified AppImage replacement for public builds; takes effect next launch |

Private/development builds update manually; no GitHub access token is embedded.
Windows Store builds use Store-managed updates. The Linux AppImage still needs
host GSR, PipeWire, FFmpeg tools and compatible drivers; it is not a promise of
KDE/GNOME/X11 or older-glibc compatibility. See [installation](INSTALL.md) and
[dependency audit](DEPENDENCY_AUDIT.md) before distributing binaries.

Windows tags: `vX.Y.Z`, with `Shadowless-X.Y.Z-Setup-win-x64.exe`.
Linux tags: `linux-vX.Y.Z`, with `Shadowless-X.Y.Z-linux-x86_64.AppImage`.
Linux releases must use `--latest=false` to avoid replacing the Windows latest release.

See [AI development disclosure](AI_DEVELOPMENT.md) and the platform README for details.
