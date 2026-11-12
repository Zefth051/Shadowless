# Install Shadowless

**Current status: public source; no published binary release yet.** Setup,
Portable and AppImage assets exist in unpublished validation drafts. Draft access requires
sufficient repository permissions. See [release status](RELEASE_STATUS.md).
Windows native package upgrades passed controlled-transport QA; see
[the evidence and limits](WINDOWS_UPDATE_VALIDATION.md). Neither platform has
completed the full in-app upgrade through two published releases. The
public-update flows below still await that test.

Authorized draft testers must sign in with a GitHub account allowed to view drafts.
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
in-app, SHA-256-verified updates from published final Windows releases by default.
The repository is public, but no eligible release is published yet. No GitHub
credentials are embedded in the application. Store installations use Microsoft Store updates.
Preview binaries are unsigned; review the release source and checksum.

## Linux x86_64 / Hyprland

New Linux builds include Shadowless's maintained GSR 6.1.7 recording engine and
matching KMS helper. No separate GSR installation or GSR interface is required.
The older Linux 0.2.4 draft still needs an installed GSR; replacing source does
not change that existing binary.

Open [Linux releases](https://github.com/Zefth051/Shadowless/releases?q=linux-v)
and download an AppImage and its checksum file. Match the release's glibc baseline
to your system; local CachyOS/Arch previews are not validated on older distributions.

Install FFmpeg/ffprobe, Hyprland/hyprctl,
PipeWire/pipewire-pulse, `pactl` and `pw-loopback` through your distribution.
Graphics drivers must work. Direct KMS capture with the managed engine may request
administrator authorization through Polkit; Shadowless's interface stays unprivileged.

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
