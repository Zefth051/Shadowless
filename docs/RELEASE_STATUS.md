# Release and validation status

Last checked: 2026-11-12. This is a private validation stage, not a published
Shadowless release. Draft assets require repository access and permission to view
draft releases; being able to browse source does not guarantee draft access.
Open Releases and select the matching draft title/tag. GitHub may change a
draft's temporary `untagged-*` URL when its notes are edited.

## Available build candidates

| Platform | Draft tag | Attached binaries | Publication |
| --- | --- | --- | --- |
| Windows x64 | [`v0.3.0` draft](https://github.com/Zefth051/Shadowless/releases/tag/untagged-ac8a93b4af9367377e3a) | Setup EXE, Portable ZIP, `SHA256SUMS.txt`; final-only public updates enabled | Draft, not published |
| Windows x64 (older) | [`v0.2.4` draft](https://github.com/Zefth051/Shadowless/releases) | Setup EXE, Portable ZIP, `SHA256SUMS.txt`; updater disabled | Draft, not published |
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
Portable packages. New Linux builds use Qt and a bundled, maintained GSR 6.1.7
engine with its matching helper, without GSR's UI. The older Linux 0.2.4 draft
still needs host GSR. Hyprland on Wayland, PipeWire, FFmpeg tools and compatible
graphics drivers remain host requirements. An AppImage does not install those tools
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
The older 0.2.4 draft builds and Linux development builds update manually. New
Windows Release builds enable anonymous public updates and only accept published
final Windows releases, excluding drafts and pre-releases even on 0.x versions.
Windows Store updates use a separate Store-managed channel.

| Platform | Existing evidence | Still required |
| --- | --- | --- |
| Windows Setup / Portable | Native Setup and Portable upgrades between real QA packages, SHA-256 rejection/recovery, launcher start and settings preservation; [evidence and limits](WINDOWS_UPDATE_VALIDATION.md) | Real in-app discovery, download, confirmation, upgrade and relaunch between two versions on native Windows, for both packages |
| Linux AppImage | Offline updater tests; AppImage checksum/startup verification; real Hyprland UI, synthetic playback/editor and six screenshots on this host | Real in-app discovery, download, confirmation, AppImage replacement and relaunch between two versions on a supported Hyprland host |

Package-level Windows upgrade QA passed on 2026-11-12; it used controlled
package transport and does not complete the real published-release/UI checks below.
The older 0.2.4 installer still has updates disabled; install an enabled build
manually once before relying on later public-release updates.

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

## Managed engine and shared startup verification

Linux source now includes GSR 6.1.7 at commit
`1d4946a16139b446e6308f741e9ba99f2ed5b3a7` with documented local helper/lifetime
patches. The local 0.3.0 AppImage includes the engine source and passes startup,
capability detection, source-hash and private-helper-library checks. The new
engine is not retroactively included in the old uploaded 0.2.4 draft.

Automated Linux verification: 108 Qt passes, nine skips, no failures; separate
desktop/autostart checks pass; 14 packaging tests pass, including forced recorder
shutdown. Live KMS capture and interactive Polkit authorization/cancellation with
the managed helper, and a real logout/login cycle, remain pending. Source and
package checks do not establish clean-host or older-glibc compatibility.

Both branches implement independent opt-in login startup and replay-on-launch
settings and a containing-folder action in clip rows. Windows config tests include
a persisted-state recovery regression. Completed roadmap work and remaining
features are tracked in [the shared roadmap](ROADMAP.md).

## Windows 0.3.0 installer candidate

[Release build 43275206850](https://github.com/Zefth051/Shadowless/actions/runs/43275206850)
completed successfully from `b810d33f7f9649fd77ed70529f755aa982001ba2`, with
`enable_public_updates=true`, `prerelease=false`, `publish=false`. Native checks
passed for public/private updater policy, Portable layout, installation, running
application shutdown during uninstall, and SHA-256.
[Windows CI 43275192665](https://github.com/Zefth051/Shadowless/actions/runs/43275192665)
also passed, including native build, analyzers and UI/localization checks.

The uploaded Setup is 167,116,033 bytes, SHA-256
`ea32328f910c414245e84361042f511ba32a807915eabd0a1f42a92c7219800a`.
The Portable ZIP is 216,667,723 bytes, SHA-256
`104222e29423ee917853f957658a36b368ab9af57afab5427a00003277e65247`.
The draft is not publicly downloadable and cannot itself be selected by the
updater. Once accessible final releases exist, this enabled build can discover
them without another manual installation merely for the visibility transition.
