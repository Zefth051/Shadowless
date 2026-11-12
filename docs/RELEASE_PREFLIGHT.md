# Release distribution preflight

## Windows 0.3.0

Reviewed on 2026-11-12. **Incomplete: keep the candidate a draft.** This is a
technical record for the actual binaries, not a legal certification. Passing
installer/update tests does not resolve the source and notice gaps below.

### Candidate identity

Source: `b810d33f7f9649fd77ed70529f755aa982001ba2`; native release workflow
`43275206850`, with public updates enabled. The release remains a draft.

| Asset | SHA-256 |
| --- | --- |
| `Shadowless-0.3.0-Portable-win-x64.zip` | `104222e29423ee917853f957658a36b368ab9af57afab5427a00003277e65247` |
| `Shadowless-0.3.0-Setup-win-x64.exe` | `ea32328f910c414245e84361042f511ba32a807915eabd0a1f42a92c7219800a` |

Both downloaded binaries match the release's `SHA256SUMS.txt`. The Portable
contains 146 ZIP entries. Its notices consist of the project license,
`THIRD_PARTY_NOTICES.md`, generic GNU texts and the standalone FFmpeg license.
It does not contain the newly collected .NET/NuGet notices below. Installer
contents were not independently extracted in this preflight.

### Recovered evidence

The ignored local directory `artifacts/release/0.3.0-verified/evidence/` holds:

- Exact Shadowless source archive at the candidate commit and Portable file
  hashes, including native modules, acquisition scripts and build scripts.
- [Official OBS 32.2.2 source archive](https://github.com/obsproject/obs-studio/releases/download/32.2.2/OBS-Studio-32.2.2-Sources.tar.gz),
  verified against upstream asset SHA-256
  `ec81fb66b03e75ddb3076b576f62679c39262e0e9960cef3e17a40dc5d68e6b4`.
- [mpv source](https://github.com/mpv-player/mpv/tree/bcde6b93cca4d128d84e1a60d1a6db2cd81c889e)
  and [BtbN FFmpeg source](https://github.com/FFmpeg/FFmpeg/tree/c179411bbb2864cc71323d3055acda25d5c2aace)
  at the full commits matching their pinned revision identifiers.
- [The mpv builder's exact workflow logs](https://github.com/shinchiro/mpv-winbuild-cmake/actions/runs/42927840447)
  and [build recipes](https://github.com/shinchiro/mpv-winbuild-cmake/tree/bda10a2816ce9908d419b3f4b564227330457e76),
  including `mpv.cmake`, `ffmpeg.cmake` and `mpv_clang.yml`. The release's sibling
  FFmpeg archive identifies revision `cd4e96eb3`; its full source commit
  `cd4e96eb314630788b7ff2f9820c395788c023e2` is collected too, but that sibling
  archive alone does not prove the precise FFmpeg revision inside libmpv.
- Binary configuration strings from the actual Portable FFmpeg executable,
  OBS `avcodec-62.dll` and libmpv, and embedded .NET version evidence.
- Exact package nuspec metadata and available notices for locked NuGet packages.

`ACQUISITIONS.json` records URLs, sizes and hashes of downloaded sources/texts;
the supplemental archive also has a complete file hash manifest. Its name and
README explicitly identify it as **partial evidence**, not complete corresponding
source. No newly collected text changes the existing draft binary contents.

### Concrete remaining gaps

1. **libmpv static dependencies.** The upstream recipe uses `--prefer-static` and
   builds FFmpeg with `--enable-gpl --enable-version3`, x264, x265, DVD libraries,
   rubberband and numerous other libraries. Treat this as a GPLv3-enabled build.
   The development archive contains only the DLL, import library and four API
   headers. It provides no dependency source bundle or component notices.
   Recovered workflow logs restore a cached `src_packages` tree; they do not
   enumerate every exact cached dependency revision. Top-level mpv source and
   build recipes therefore do not establish complete corresponding source.
   Obtain that build's source/dependency manifest and matching source trees,
   or rebuild from a completely recorded source closure.
2. **OBS dependency closure.** OBS source is available, but the shipped
   `avcodec-62.dll`, `libx264-164.dll`, `librist.dll`, `srt.dll`, `libcurl.dll`,
   `w32-pthreads.dll`, `zlib.dll` and codec dependencies still need exact
   obs-deps source/build mappings and their component copyright/license texts.
   The actual OBS FFmpeg configuration includes GPL/version3 and additional
   codecs. OBS's top-level source archive does not establish those sources.
3. **Standalone BtbN FFmpeg closure.** Top-level FFmpeg source and actual binary
   configure flags are collected. The shared build still includes statically
   linked dependencies such as fontconfig, FreeType, HarfBuzz, libxml2, rav1e,
   libvmaf and others. Exact build recipe revision, patches, dependency revisions,
   source archives and required notices remain to be mapped to this binary.
4. **Deliver verified notices with the distributed artifacts.** Eleven upstream
   notice/license texts plus a source/hash manifest are now in
   `licenses/third-party/windows/`. They cover NAudio, H.NotifyIcon, .NET runtime/WPF/WinForms
   10.0.15 and available System.Drawing.Common/Microsoft.Win32.SystemEvents
   10.0.0 texts. The actual executable's embedded runtime configuration confirms
   10.0.15. Existing `BuildRelease.ps1` copies `licenses/` recursively, so future
   builds include them. The current draft needs a reviewed supplemental notice
   distribution or a rebuilt candidate; these files do not fill native gaps 1–3.

The existing product source grant is GPL-2.0-or-later and permits a GPLv3 route;
this does not waive dependency source or notice duties. Preserve the gate in
`DEPENDENCY_AUDIT.md` until the specific missing native dependency closure is
supplied and checked against the distributed artifacts.

## Linux candidates and HTTPS packaging

### Cloud 0.3.0 candidate

[Workflow 43279436777](https://github.com/Zefth051/Shadowless/actions/runs/43279436777)
succeeded from source `c9558c35746507b2bbd52fc44ce641b7bd2a1a37` with
`SHADOWLESS_PUBLIC_UPDATES=true`. The resulting `linux-v0.3.0` release remains an
unpublished validation draft. AppImage size: 98,130,424 bytes; SHA-256:
`98cf8cf589d7819aed8afd5a799b10dcfa3a5064b6917e4c1ff534061cbe7896`.
The downloaded AppImage matches its checksum file and GitHub asset digest.
The exact build package inventory is attached as `BUILD_PACKAGES.txt` (SHA-256
`e840230de3d7e8bb199815340c874d40defcb7a62f49e4cf1b2b3b974ad94584`).

This image contains 219 ELF files but no `usr/plugins/tls` directory. A separate
probe restricted to the earlier equivalent bundle's plugin path returned
`supportsSsl=0`. Enabling the updater compile flag does not by itself make HTTPS
usable on a machine without host Qt plugins. Do not publish this candidate as
having independently working AppImage updates.

### Corrected 0.3.1 packaging

The packager now requires Qt's OpenSSL backend and copies OpenSSL libraries with
their versioned aliases before dependency deployment. It fails when those inputs
are absent. Seventeen packaging tests pass. A complete locally repackaged image,
restricted to its own plugin and library directories, loaded its own Qt,
OpenSSL and TLS backend and received HTTP 200 from GitHub with no TLS errors.
The old and corrected probes used the same application/network libraries; the
change was the missing backend and explicit runtime staging. Local evidence is
in ignored `.qa/tls-packaging-verification/`; it is not a full production updater
upgrade test. The 0.3.1 cloud candidate must be built with public updates enabled.

### Remaining Linux distribution material

The cloud bundle includes the maintained GSR source and engine build inputs,
project attribution and generic GNU license texts. It does not include complete
corresponding sources/build recipes and component copyright notices for all Qt,
FFmpeg, mpv, LayerShellQt and transitive libraries, or the embedded AppImage
runtime. `BUILD_PACKAGES.txt` records the build environment; it is not an exact
source closure or a replacement for those materials.

Do not map the cloud bundle from the developer's currently installed package
database: those packages have already changed, and the older local preview used
CachyOS-specific revisions. Match the actual cloud artifact and its inventory.
The unrelated Windows notice directory is not a substitute for Linux notices.

## Publication decision

Neither platform's current candidate is cleared by this preflight. The owner
requested publication only if releases were finished. Keep validation drafts
unpublished until the remaining exact native source/notice material is supplied,
or replace opaque third-party builds with builds whose complete inputs are
recorded and available. Missing cached source revisions cannot be inferred solely
from DLL strings or a builder's top-level commit.

The previous 0.2.4 Windows/Linux binaries were compiled with updates disabled.
They need one manual upgrade to an enabled build. Subsequent enabled builds can
check/download updates inside Shadowless; Linux applies the AppImage replacement
and asks the user to quit/reopen, while Windows invokes Setup or its Portable
replacement helper. This does not promise an unattended update or bypass user
confirmation. Full updates between actual published versions remain unverified.
