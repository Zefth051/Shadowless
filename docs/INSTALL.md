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

Open [Releases](https://github.com/Zefth051/Shadowless/releases), choose a Windows
`vX.Y.Z` release and download `Shadowless-X.Y.Z-Setup-win-x64.exe`. Run the
installer, then launch Shadowless. Windows 10 version 2004 or Windows 11 x64
and a supported hardware encoder are required; .NET is included.

For Portable, extract the entire ZIP into a writable directory and run the
top-level `Shadowless.exe`; keep `runtime/` and `docs/` beside it.
Private builds update manually. Public-enabled desktop builds offer an in-app,
SHA-256-verified update. Store installations use Microsoft Store updates.
Preview binaries are unsigned; review the release source and checksum.

## Linux x86_64 / Hyprland

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
