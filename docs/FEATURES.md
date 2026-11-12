# Platform features

Shadowless has two maintained platform branches in one repository. Releases are
shared by the repository, so select the tag for your operating system.
These are implemented capabilities, not a statement of completed end-to-end
validation. See [release status and pending tests](RELEASE_STATUS.md) and
[Windows native package QA](WINDOWS_UPDATE_VALIDATION.md).

| Feature | `windows` | `linux` |
| --- | --- | --- |
| Platform | Windows 10 2004+ / 11 x64 | Hyprland on native Wayland, x86_64 |
| Interface / capture | WPF/.NET 10 and libobs | Qt 6 and a Shadowless-managed GSR engine |
| Packages | Inno Setup installer and Portable ZIP | AppImage |
| Replay / recording | RAM replay buffer and continuous recording | GSR replay buffer and continuous recording, including simultaneous modes |
| Video | Desktop / direct game capture, supported hardware encoders | Monitor capture through GSR, supported hardware encoders |
| Audio | Desktop/game, microphone, per-application routing and separate tracks | PipeWire desktop/microphone, application tracks and optional unassigned-PC track |
| Clip library | Recent clips, game folders, playback and deletion | Recent clips, game-folder filters, playback and desktop Trash |
| Editing | Trimming, audio-track selection and configurable export | Trimming, audio-track selection/merge and stream-copy export |
| Overlays | Composited media layers, live preview editor and optional passive capture status indicator | Hyprland control panel, optional passive indicator and composited media layers |
| Shortcuts | Configurable Windows global hotkeys | Desktop portal and temporary Hyprland bindings |
| Startup | Separate login startup and opt-in replay-on-launch settings | Separate login startup and opt-in replay-on-launch settings; XDG / default Hyprland configs |
| Updates | Native package upgrades tested; public-release/UI flow pending | SHA-256-verified AppImage replacement implemented for public builds; real upgrade untested |

Existing private drafts and development builds update manually; new Windows
Release builds enable public-release updates by default, including before the
public transition. No GitHub access token is embedded.
Windows Store builds use Store-managed updates. New Linux builds bundle their GSR
engine and helper without GSR's UI; the old 0.2.4 draft still needs host GSR.
PipeWire, FFmpeg tools and compatible drivers remain host requirements; it is not a promise of
KDE/GNOME/X11 or older-glibc compatibility. See [installation](INSTALL.md) and
[dependency audit](DEPENDENCY_AUDIT.md) before distributing binaries.

Project hardware testing is limited to the `linux` branch on an NVIDIA GeForce
GTX 1650 with Hyprland / Wayland and GSR. Windows capture and other GPUs remain
unvalidated by Shadowless. RTX test reports are from upstream Captail on Windows;
automated Windows package QA is separate from hardware/capture validation.

Windows tags: `vX.Y.Z`, with `Shadowless-X.Y.Z-Setup-win-x64.exe`.
Linux tags: `linux-vX.Y.Z`, with `Shadowless-X.Y.Z-linux-x86_64.AppImage`.
Linux releases must use `--latest=false` to avoid replacing the Windows latest release.

See [AI development disclosure](AI_DEVELOPMENT.md) and the platform README for details.
