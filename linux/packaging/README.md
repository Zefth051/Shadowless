# Linux AppImage preview

This packages the native Qt application, its overlay plugin, the maintained GSR
6.1.7 engine and matching KMS helper, and dynamically
discovered Qt/FFmpeg/libmpv libraries. It is an x86_64 **private-test artifact**,
not a claim of compatibility with every Linux distribution.

Build and validate first, then package:

```sh
./linux/build.sh
python3 -m unittest discover -s linux/tests/packaging
python3 linux/packaging/build_appimage.py --version 0.2.4
```

The packager downloads SHA-256-pinned linuxdeploy, appimagetool
and the AppImage runtime from their official repositories, extracts them without
FUSE, bundles libraries and smoke-tests `--help` offscreen. Output is under
`artifacts/release/linux-<version>/`. Existing releases are never overwritten.
The bundle records its glibc baseline and file hashes in `BUNDLE_MANIFEST.txt`.
Build on the oldest supported system; a build on current CachyOS/Arch does not
support older glibc distributions. Verify Wayland, SVG and multimedia plugins
on a clean target host before distributing.
Packaging requires and includes Qt's OpenSSL TLS backend and the matching OpenSSL
runtime libraries, including their versioned loader aliases, for HTTPS update
checks and downloads. TLS uses the system certificate trust store.

## Host requirements

Hyprland on native Wayland, compatible graphics drivers, PipeWire/pipewire-pulse,
`ffmpeg`, `ffprobe`,
`hyprctl`, `pactl` and `pw-loopback` must be installed through the distribution.
GSR does not need to be installed. Only the recording engine and helper are
included, without GSR UI/services. The overlay uses the bundled source header.
Direct KMS capture may ask for Polkit administrator authentication. The managed
launcher copies its matching helper and private libdrm to a fresh private `/tmp`
directory so root can access them outside FUSE. Helper RUNPATH resolves the private
library after Polkit clears environment paths. Staged files are removed on exit;
no permanent capabilities or system installation are performed.
The host tools run with the original host library search path, separate from
the bundled application's libraries. The host FFmpeg executable remains necessary
for editor/thumbnail subprocesses; the bundled FFmpeg libraries serve Qt/libmpv
and the application. The managed recorder uses bundled libraries, while editor/audio tools use host
libraries. Engine source, licenses, provenance, hashes and build inputs are under
`usr/share/doc/shadowless/` in the AppImage.

Download the AppImage and `SHA256SUMS.txt` from the Linux release:

```sh
sha256sum -c SHA256SUMS.txt
chmod +x Shadowless-0.2.4-linux-x86_64.AppImage
./Shadowless-0.2.4-linux-x86_64.AppImage --check-dependencies
./Shadowless-0.2.4-linux-x86_64.AppImage
```

If FUSE is unavailable, use `--appimage-extract-and-run`. Keep the AppImage in
a permanent location before first launch; the portal desktop identity must refer
to that file, not its temporary mount. Public builds offer verified updates through
the tray menu and apply them next launch. Private/development builds update manually.
For a public build, set `SHADOWLESS_PUBLIC_UPDATES=true` and
`SHADOWLESS_VERSION=X.Y.Z` when invoking `./linux/build.sh`.

## Source and license gate

Preserve Shadowless/Captail attribution and all component notices. Before sharing
binaries, complete `docs/DEPENDENCY_AUDIT.md` for the exact bundled files, include
their license/copyright texts and provide corresponding source and build inputs
to recipients. A source archive of Shadowless alone does not cover bundled Qt,
libmpv, FFmpeg or their transitive libraries. The generated manifest is an
inventory, not certification that those obligations are satisfied.
