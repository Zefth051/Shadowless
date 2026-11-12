# Third-party notices

## Captail / original project

Shadowless is derived from [Captail by FaulMit](https://github.com/FaulMit/captail)
under GNU GPL-2.0-or-later. Original authors retain their copyrights; Shadowless's
renaming and modifications do not imply upstream endorsement. Preserve the
original history, attribution, license and corresponding modified source.

Before distributing, complete [the dependency/source audit](docs/DEPENDENCY_AUDIT.md)
for the exact runtime builds and include all required license/copyright texts.
Project links alone do not satisfy every redistribution requirement.

Shadowless dynamically uses and may redistribute selected components from OBS Studio 32.2.2.

## OBS Studio / libobs

- Project: https://github.com/obsproject/obs-studio
- Source release: https://github.com/obsproject/obs-studio/tree/32.2.2
- License: GNU General Public License, version 2 or later
- Local license text: `LICENSE`

`tools/AcquireObsRuntime.ps1` prepares the runtime. Distributions contain only components required for libobs, Replay Buffer, Windows capture, WASAPI audio, and supported hardware encoders.

Shadowless's `native/ProcessAudio` module narrowly adapts the process-loopback
activation, audio format, silent-buffer, timestamp, and packet-delivery portions
of OBS Studio 32.2.2 `plugins/win-wasapi/win-wasapi.cpp`. Window matching,
device capture, OBS UI properties, and other unrelated `win-wasapi`
functionality are not copied. `tools/AcquireObsPluginSdk.ps1` downloads the
pinned OBS 32.2.2 source archive with SHA-256 verification and extracts the
public libobs headers used to compile the module.

## FFmpeg

- Project: https://ffmpeg.org/
- Windows build: https://github.com/BtbN/FFmpeg-Builds
- Build: `n9.0.2-49-gc179411bbb`, LGPL shared/static variants
- License: the selected Windows archive includes LGPL-3.0; optional components retain their own licenses. Its original license text is preserved as `LICENSE.FFmpeg.txt`.

`tools/AcquireFfmpegRuntime.ps1` downloads a pinned, SHA-256-verified build. Shadowless uses it for clip metadata, thumbnails, and non-destructive trimming.

## mpv / libmpv

- Project: https://mpv.io/
- Source revision: `git-bcde6b93cc`
- Native Windows build: https://github.com/shinchiro/mpv-winbuild-cmake/releases/tag/20261111
- License: this pinned build uses a GPLv3-enabled dependency configuration. Its exact upstream build recipe enables FFmpeg `--enable-gpl --enable-version3` and links GPL components; the availability of LGPL builds of mpv does not describe this binary.

The release links upstream workflow `42927840447`, whose build recipes are at
`shinchiro/mpv-winbuild-cmake` commit `bda10a2816ce9908d419b3f4b564227330457e76`.
Exact source revisions and notices for its complete static dependency closure
remain unverified. See [the Windows 0.3.0 preflight](docs/RELEASE_PREFLIGHT.md).

Shadowless dynamically loads the replaceable `libmpv-2.dll` for embedded editor playback, hardware decoding, seeking, and local mixing of selected audio tracks. Shadowless does not bundle or launch the standalone `mpv.exe` player.

## NuGet dependencies

- NAudio — MIT License: https://github.com/naudio/NAudio
- H.NotifyIcon — MIT License: https://github.com/HavenDV/H.NotifyIcon
- System.Drawing.Common — MIT License: https://github.com/dotnet/runtime

Licenses and copyright notices from these projects remain applicable to their respective components.
Verified notice texts and their source/hash manifest are in `licenses/third-party/windows/`.
These files are included by future packaging runs; the existing 0.3.0 draft
does not contain this newly collected set.

## Discord Quest Completer game catalog

- Project: https://github.com/markterence/discord-quest-completer
- Source revision: `53191138e647304dc7dce6a6ace77ae19f8309c3`
- Source file: `src/assets/gamelist.json`
- License: MIT License
- Copyright: Mark Terence Tiglao, 2025

Shadowless embeds a reduced Windows executable-to-game-name index generated from
this catalog. `tools/UpdateGameCatalog.ps1` filters launcher and non-Windows
entries and records the exact source revision in the generated asset.

MIT License

Copyright (c) 2025 Mark Terence Tiglao

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## License texts and development disclosure

Relevant GNU license texts are included in `licenses/`. Exact component notices
and corresponding sources remain subject to [the dependency audit](docs/DEPENDENCY_AUDIT.md).
Shadowless is developed with AI assistance, including OpenAI Codex; this does not
change original authorship, attribution or third-party licensing.
