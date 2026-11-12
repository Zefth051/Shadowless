# Shadowless - An app for clipping

**Developed with AI assistance, including OpenAI Codex.** This is a fork of
Captail by FaulMit; original attribution and licensing are retained.
See [AI disclosure](docs/AI_DEVELOPMENT.md) and the
[Windows / Linux feature comparison](docs/FEATURES.md).

[Windows installer / releases](https://github.com/Zefth051/Shadowless/releases) ·
[Linux AppImage releases](https://github.com/Zefth051/Shadowless/releases?q=linux-v) ·
[Installation guide](docs/INSTALL.md)

## Current release status

**Private validation drafts only; no published release.** The Windows draft
contains Setup and Portable packages. The Linux draft contains
`Shadowless-0.2.4-linux-x86_64.AppImage`; binaries are attached to Releases,
not included in the source ZIP. Viewing drafts requires sufficient repository
permissions. See [available packages and pending validation](docs/RELEASE_STATUS.md).

| | Windows edition | Linux edition |
| --- | --- | --- |
| Interface / capture | WPF/.NET and libobs | Qt 6 and external GPU Screen Recorder |
| Target | Windows 10/11 x64 | Hyprland on native Wayland, x86_64 |
| Capture | Desktop and direct game capture | Monitor capture through GSR |
| Distribution | Setup EXE / Portable ZIP | AppImage; host GSR, PipeWire and FFmpeg required |
| Real in-app upgrade | Not yet tested | Not yet tested |

The editions have separate interfaces and platform requirements. See the
[full comparison](docs/FEATURES.md). Private builds update manually. Automated
updater tests do not establish a successful upgrade between two released versions.
[Windows native package upgrades passed](docs/WINDOWS_UPDATE_VALIDATION.md);
the complete published-release/UI path remains pending.
See [updates and local work](docs/UPDATES_AND_LOCAL_WORK.md) for the release process.

Shadowless saves recent gameplay and makes continuous recordings. This branch
contains the native Linux edition for Hyprland on Wayland, powered by GPU
Screen Recorder. The [Windows edition](../../tree/windows) uses WPF and libobs.

See the [Linux build and usage guide](linux/README.md) for dependencies,
shortcuts, capture settings, and validation. See [publishing both branches](docs/GITHUB.md)
for repository setup.

Shadowless is based on [Captail by FaulMit](https://github.com/FaulMit/captail)
and retains its [GPL-2.0-or-later license](LICENSE).

## Linux preview screenshots

Real captures of the `linux-v0.2.4` AppImage on Hyprland. Capture is off; the
library/player/editor use a synthetic demo pattern. These images do not claim
recording performance or Windows behavior. [All six captures and provenance](docs/screenshots/linux/README.md).

![Shadowless Linux dashboard and clip library](docs/screenshots/linux/dashboard.png)

<p>
  <img src="docs/screenshots/linux/settings-video.png" alt="Linux video settings with H.264 and 60 FPS" width="400">
  <img src="docs/screenshots/linux/settings-audio.png" alt="Linux audio settings with capture disabled and default devices" width="400">
</p>

![Shadowless Linux player with two synthetic demo audio tracks](docs/screenshots/linux/player.png)
