# Install Shadowless

**Current status: private validation drafts only.** Setup, Portable and AppImage
assets exist in draft releases; no release is published. Draft access requires
sufficient repository permissions. See [release status](RELEASE_STATUS.md).
Windows native package upgrades passed controlled-transport QA; see
[the evidence and limits](WINDOWS_UPDATE_VALIDATION.md). Neither platform has
completed the full in-app upgrade through two published releases. The
public-update flows below still await that test.

Private testers must sign in with a GitHub account granted repository access.
Public downloads become available after reviewed releases are published.
Source ZIPs are for developers, not installers.

## Windows x64

In Settings, **Start with Windows** launches Shadowless at sign-in. The separate
**Start Instant Replay when Shadowless launches** option starts replay buffering
on any normal launch, including a manual one; it is off until explicitly enabled.

Open [Releases](https://github.com/Zefth051/Shadowless/releases), choose a Windows
`vX.Y.Z` release and download `Shadowless-X.Y.Z-Setup-win-x64.exe`. Run the
installer, then launch Shadowless. Windows 10 version 2004 or Windows 11 x64
and a supported hardware encoder are required; .NET is included.

Windows capture uses bundled OBS components (`libobs`); installing OBS Studio
separately is not required.

For Portable, extract the entire ZIP into a writable directory and run the
top-level `Shadowless.exe`; keep `runtime/` and `docs/` beside it.
Existing private drafts update manually. New Windows Release builds enable
in-app, SHA-256-verified public-release updates by default. Until the repository
and a newer release are public, no update is accessible without GitHub credentials;
none are embedded in the application. Store installations use Microsoft Store updates.
Preview binaries are unsigned; review the release source and checksum.

## Linux x86_64 / Hyprland

Linux capture uses GPU Screen Recorder (GSR) as an external process. GSR must be
installed separately on the host; it is not included in the AppImage. Shadowless
starts and manages it, so you do not need to open the GSR GUI or start it manually.

Open [Linux releases](https://github.com/Zefth051/Shadowless/releases?q=linux-v)
and download an AppImage and its checksum file. Match the release's glibc baseline
to your system; local CachyOS/Arch previews are not validated on older distributions.

Install GSR 6.1.5 with IPC/plugin support, FFmpeg/ffprobe, Hyprland/hyprctl,
PipeWire/pipewire-pulse, `pactl` and `pw-loopback` through your distribution.
Graphics drivers and capture permissions must already work; the AppImage does not
install or configure privileged helpers.

Keep the AppImage in a permanent location, replace `X.Y.Z` below, and run:

```sh
sha256sum -c SHA256SUMS.txt
chmod +x Shadowless-X.Y.Z-linux-x86_64.AppImage
./Shadowless-X.Y.Z-linux-x86_64.AppImage --check-dependencies
./Shadowless-X.Y.Z-linux-x86_64.AppImage
```

Without FUSE, add `--appimage-extract-and-run` before app arguments. Public-enabled
builds check Linux releases at startup; **tray → Check for updates** checks manually.
After confirmation, Shadowless verifies GitHub's SHA-256 digest and atomically
replaces the writable AppImage in place. Quit and reopen when recording is finished
to use the update. Private/development builds update manually. Settings and recordings
are outside the AppImage. Symlinks and read-only installations require manual updates.
