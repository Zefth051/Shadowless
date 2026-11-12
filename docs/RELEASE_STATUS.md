# Release and validation status

Last checked: 2026-11-12. This is a private validation stage, not a published
Shadowless release. Draft assets require repository access and permission to view
draft releases; being able to browse source does not guarantee draft access.
Open Releases and select the matching draft title/tag. GitHub may change a
draft's temporary `untagged-*` URL when its notes are edited.

## Available build candidates

| Platform | Draft tag | Attached binaries | Publication |
| --- | --- | --- | --- |
| Windows x64 | [`v0.2.4` draft](https://github.com/Zefth051/Shadowless/releases) | Setup EXE, Portable ZIP, `SHA256SUMS.txt` | Draft, not published |
| Linux x86_64 / Hyprland | [`linux-v0.2.4` draft](https://github.com/Zefth051/Shadowless/releases) | AppImage, `SHA256SUMS.txt`, `BUILD_PACKAGES.txt` | Draft, not published |

The Linux asset is `Shadowless-0.2.4-linux-x86_64.AppImage` (97,430,008 bytes).
It is attached to the Linux draft under
[GitHub Releases](https://github.com/Zefth051/Shadowless/releases).
It is not stored in the source tree or included in GitHub's source ZIP.
The default `windows` branch does not contain the native Linux source:
select the [`linux` branch](https://github.com/Zefth051/Shadowless/tree/linux).
Complete the [dependency/source audit](DEPENDENCY_AUDIT.md) before sharing binaries.

## Platform differences

These editions use separate interfaces, capture backends and packaging.
Windows uses WPF/.NET and libobs, with desktop/direct game capture and Setup or
Portable packages. Linux uses Qt and external GPU Screen Recorder, targets
Hyprland on Wayland, captures monitors, and requires host GSR, PipeWire, FFmpeg
tools and compatible graphics drivers. An AppImage does not install those tools
or establish support for GNOME, KDE, X11 or older glibc systems.
See the [feature comparison](FEATURES.md) and [installation guide](INSTALL.md).

## Hardware validation scope

Shadowless hardware testing is limited to the `linux` branch on an NVIDIA GeForce
GTX 1650 with Hyprland / Wayland and GSR. Other GPUs and Windows capture remain
unvalidated by this project. RTX test reports belong to upstream Captail on
Windows and must not be presented as Shadowless test results.
The Windows package QA below checks installation and updates, not GPU capture.

## Screenshots

The inherited Windows screenshots show Captail or an earlier upstream build.
They do not validate the current Shadowless interface or the Linux edition.
They are excluded from the current product presentation. Six real Linux captures
from the draft AppImage are available in [the Linux gallery](screenshots/linux/README.md).
Windows captures remain pending; renaming image files or editing branding is insufficient.
See the [capture requirements](SCREENSHOTS.md).

## Updates: implemented, end-to-end validation pending

Neither Linux nor Windows has completed a real in-app upgrade between two
released Shadowless versions. Automated tests and package installation checks
are separate evidence and must not be described as that validation.
Existing private drafts and development builds have in-app updates disabled and
update manually. New Windows Release builds enable public-release updates by
default, including candidates built before the repository becomes public; this
does not change the existing draft binaries.
Windows Store updates use a separate Store-managed channel.

| Platform | Existing evidence | Still required |
| --- | --- | --- |
| Windows Setup / Portable | Native Setup and Portable upgrades between real QA packages, SHA-256 rejection/recovery, launcher start and settings preservation; [evidence and limits](WINDOWS_UPDATE_VALIDATION.md) | Real in-app discovery, download, confirmation, upgrade and relaunch between two versions on native Windows, for both packages |
| Linux AppImage | Offline updater tests; AppImage checksum/startup verification; real Hyprland UI, synthetic playback/editor and six screenshots on this host | Real in-app discovery, download, confirmation, AppImage replacement and relaunch between two versions on a supported Hyprland host |

Package-level Windows upgrade QA passed on 2026-11-12; it used controlled
package transport and does not complete the real published-release/UI checks below.
The private installer still has updates disabled.

Before claiming the full in-app update flow is verified:

- [ ] Test the complete Windows Setup upgrade and confirm the new running version.
- [ ] Test the complete Windows Portable upgrade and confirm the new running version.
- [ ] Test the complete Linux AppImage upgrade and confirm the new running version.
- [ ] Confirm settings and recordings survive each upgrade.
- [ ] Exercise cancellation, network interruption, checksum rejection and unwritable/locked targets; record recovery behavior.
- [ ] Record starting/target versions, source commits, package hashes, OS and results.

A public-update-enabled candidate and a reachable matching release are required
to test the production network path. Draft/manual upgrades do not prove it.
Do not change repository visibility or publish releases solely to bypass this
test prerequisite without explicit authorization.

Fresh Windows screenshots, clean-host Linux capture and the dependency audit
also remain release gates. Detailed prior build evidence is recorded in
[the publication review](PUBLICATION_REVIEW.md).


See [updates and local development](UPDATES_AND_LOCAL_WORK.md) for the private/public
transition and why a Git push alone does not update installed applications.
