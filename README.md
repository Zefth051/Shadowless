# Shadowless - An app for clipping

**Developed with AI assistance, including OpenAI Codex.** This is a fork of
Captail by FaulMit; original attribution and licensing are retained.
See [AI disclosure](docs/AI_DEVELOPMENT.md) and the
[Windows / Linux feature comparison](docs/FEATURES.md).

[Windows installer / releases](https://github.com/Zefth051/Shadowless/releases) ·
[Linux AppImage releases](https://github.com/Zefth051/Shadowless/releases?q=linux-v) ·
[Installation guide](docs/INSTALL.md)

**Capture engines:** Windows uses OBS components (`libobs`), included in the
installer and Portable package; OBS Studio does not need to be installed.
Linux uses GPU Screen Recorder (GSR) as an external capture engine. Install
`gpu-screen-recorder` separately before running Shadowless on Linux; the AppImage
does not bundle or install it. Shadowless starts and manages GSR for you.

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
[full comparison](docs/FEATURES.md). Existing private drafts update manually;
new Windows Release builds enable public-release checks by default. Automated
updater tests do not establish a successful upgrade between two released versions.
[Windows native package upgrades passed](docs/WINDOWS_UPDATE_VALIDATION.md);
the complete published-release/UI path remains pending.
See [updates and local work](docs/UPDATES_AND_LOCAL_WORK.md) for the release process.

[Linux edition](../../tree/linux) · [Publishing both branches](docs/GITHUB.md)

<p align="center">
  <img src="docs/banner.svg" alt="Shadowless — instant replay that stays on, with the Shadowless moon logo" width="920">
</p>

<p align="center">
  <a href="https://github.com/Zefth051/Shadowless/actions/workflows/ci.yml"><img src="https://github.com/Zefth051/Shadowless/actions/workflows/ci.yml/badge.svg" alt="Build"></a>
  <a href="https://github.com/Zefth051/Shadowless/releases"><img src="https://img.shields.io/github/v/release/Zefth051/Shadowless?include_prereleases&sort=semver&color=b69af7" alt="Latest release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-GPL--2.0--or--later-b69af7" alt="GPL-2.0-or-later"></a>
  <img src="https://img.shields.io/badge/platform-Windows%2010%2F11-252b2e" alt="Windows 10/11">
</p>
<p align="center">
  <strong>Save what just happened.</strong><br>
  Lightweight, open-source instant replay for Windows and Linux — built to stay recording.
</p>

<p align="center">
  <a href="https://github.com/Zefth051/Shadowless/releases"><img src="docs/readme-download.svg" alt="Download Shadowless for Windows" width="286"></a>
  <a href="https://github.com/Zefth051/Shadowless/releases/tag/untagged-e5258d391b46e29e7bd6"><img src="docs/readme-download-linux.svg" alt="Download Shadowless for Linux" width="286"></a>
</p>

<p align="center">
  <a href="https://github.com/Zefth051/Shadowless/issues/new/choose">Report a problem</a>
</p>

Shadowless is a fork of [Captail by FaulMit](https://github.com/FaulMit/captail) and a focused alternative to NVIDIA ShadowPlay Instant Replay. It can keep the latest seconds or minutes in a rolling buffer or write a continuous recording until you stop it. No scenes, streaming setup, account, cloud upload, analytics, or telemetry.

> [!WARNING]
> Shadowless `v0.2.4` is a private validation candidate. Native Windows capture and hardware compatibility need validation for this fork; inherited upstream test reports are not candidate verification.

## What's new in Shadowless 0.2.4?

- Play and trim clips in one window, with the complete timeline, audio tracks, and save actions always available.
- Export clips with a chosen format, resolution, video codec, bitrate, and audio codec.
- Browse recent clips more smoothly and see which clip is currently playing.
- Use window and shortcut icons that match your chosen accent color.
- Benefit from updated media runtimes and improved desktop-capture preparation on supported systems.

## Is Shadowless for me?

Shadowless is for Windows players who want instant replay or focused continuous recording without running a full streaming suite or wondering whether capture silently stopped.

It is a good fit if you want:

- one hotkey to save recent gameplay;
- an optional continuous recording mode without replay-buffer limits;
- desktop capture that automatically switches to a fullscreen game;
- a game-only mode that never records the desktop;
- real high-frame-rate capture for slow motion;
- system/game and microphone audio, mixed or separated;
- automatic recovery after recoverable capture, game, or driver failures;
- local files that stay on your PC.

Shadowless is not a streaming application, scene compositor, DRM bypass, or cloud clip service.

## What does it look like?

Windows screenshots remain pending; inherited Captail screenshots are excluded.
The image below shows the **Linux Qt/Hyprland edition**, captured from its actual
`linux-v0.2.4` AppImage with capture off and synthetic demo media.
See [all six Linux captures and provenance](docs/screenshots/linux/README.md)
and [Windows capture requirements](docs/SCREENSHOTS.md).

![Linux edition: Shadowless dashboard with capture off and one demo clip](docs/screenshots/linux/dashboard.png)

## How do I install it?

Shadowless builds produce the following Windows packages. Published builds are listed under GitHub Releases.

| Package | Choose it when | Installation |
| --- | --- | --- |
| `Shadowless-x.y.z-Setup-win-x64.exe` | You want the normal Windows experience | Run Setup. It installs Shadowless for your Windows account and adds an uninstaller to Windows Settings. |
| `Shadowless-x.y.z-Portable-win-x64.zip` | You want a movable, self-contained folder | Extract the entire ZIP, then run `Shadowless.exe` inside it. Do not run it from the archive. |
| `SHA256SUMS.txt` | You want to verify the download | Compare the package SHA-256 with the published value before running it. |

Every Windows package includes .NET, libobs, FFmpeg, and the embedded preview player. You do not need to install OBS Studio or extra runtimes.

> [!NOTE]
> Release binaries are not Authenticode-signed yet. Windows SmartScreen may show “Unknown publisher.” Verify `SHA256SUMS.txt` and GitHub build provenance if you want to confirm the download.

> [!IMPORTANT]
> New Windows Release builds enable GitHub update checks and checksum verification
> by default, including candidates built before the repository becomes public.
> A real in-app upgrade remains untested on Windows and Linux. Existing private
> drafts and development builds update manually; Store builds use Microsoft Store updates.

## How do I save my first replay?

1. Launch Shadowless. It stays available from the system tray.
2. Keep **Instant Replay** selected on the main dashboard.
3. Open Settings and choose **Desktop** or **Game Capture**.
4. Choose buffer length, codec, resolution, FPS, and audio sources.
5. Play normally.
6. Press `Ctrl+Shift+F10`, or click **Save**, when something worth keeping happens.

Default hotkeys:

| Action | Default |
| --- | --- |
| Save recent footage | `Ctrl+Shift+F10` |
| Enable or disable Instant Replay | `Ctrl+Shift+F9` |

Both hotkeys are configurable. Double-click the tray icon to reopen Shadowless.

The optional replay status indicator keeps recording state visible over games and desktop apps. It is enabled by default, can sit in any screen corner, ignores mouse input, and briefly confirms a successful save. Disable it or change its position in Replay settings.

Enable **Warn when a game starts** if you want one quiet reminder when Shadowless detects a game while Instant Replay is disabled. The reminder never enables recording automatically.

On first launch, Shadowless follows your Windows language when it supports it; otherwise it starts in English. Use the compact language control in the title bar to switch between English, Russian, Ukrainian, Simplified Chinese, Spanish, Brazilian Portuguese, German, French, Japanese, Korean, and Polish.

If similarly sized monitors are hard to distinguish, open Video settings and select **Identify displays** beside the monitor list. Shadowless briefly places a large number on each matching screen.

## What does each capture mode record?

| Mode | When no game is detected | When a fullscreen game appears | Audio source |
| --- | --- | --- | --- |
| **Desktop** | Records the selected monitor | Uses direct Game Capture when usable; otherwise keeps desktop video active while recognizing supported fullscreen games | Selected Windows output device |
| **Game Capture** | Keeps a lightweight game detector ready while the replay buffer sleeps | Wakes the replay buffer and captures the detected game directly | Detected game audio |

Use **Desktop** when you want continuous coverage; use **Game Capture** when privacy and low idle resource use outside games matter more. In Game Capture mode, Shadowless wakes the replay buffer only after a plausible game appears. When a fullscreen game cannot be hooked, Desktop mode favors uninterrupted video over pretending direct Game Capture is working.

## What happens when capture breaks?

Shadowless supervises the recording pipeline instead of assuming that a successful start means it will remain healthy forever.

| Situation | Expected behavior |
| --- | --- |
| Game closes or crashes | Desktop mode returns to desktop capture. Game Capture mode waits for another game. |
| You switch between game and desktop | Desktop mode follows the active capture source without stopping the buffer. |
| A capture source temporarily disappears | Shadowless keeps the pipeline supervised and retries recovery. |
| Graphics driver restarts | Video may pause or go black temporarily; the watchdog attempts a controlled restart with retry delay. |
| Recovery cannot complete | Shadowless reports the failure and retries instead of silently claiming that replay is active. |
| DRM-protected video appears | Protected regions may be black. Shadowless does not bypass DRM and is designed to keep the buffer running. |

Recovery reduces lost footage; it cannot guarantee frames that Windows, a game, an anti-cheat system, or a failed driver never delivered.

## Where is the rolling buffer stored?

The live replay buffer stores compressed video and audio packets in **RAM**. Only saved replays are written to your selected folder.

- **Buffer length** controls how much recent time Shadowless tries to keep.
- **Buffer limit** caps compressed replay data in RAM. If it is reached first, available replay time becomes shorter.
- **Bitrate, resolution, FPS, and codec** affect both memory use and saved file size.
- The dashboard shows free space on the drive containing your replay folder.

After a successful save, Shadowless starts a new replay segment. Saving again five minutes later produces roughly five minutes of new footage instead of duplicating the previous clip.

## Which codec should I choose?

Shadowless detects the current GPU and driver, then hides unavailable hardware codecs.

| Codec | Best for | Trade-off |
| --- | --- | --- |
| **AV1** | Best compression on supported modern GPUs | Newest format; some older players and editors may not support it. Hardware support commonly includes GeForce RTX 40/50, Radeon RX 7000, and Intel Arc. |
| **HEVC / H.265** | Good compression with broader modern-GPU support | Compatibility is better than AV1 but still weaker than H.264 in older software. |
| **H.264 / AVC** | Easiest playback, editing, and sharing | Larger files or lower quality at the same bitrate. |

Unsure? Start with **H.264** for compatibility. Use **AV1** when your GPU and editor support it and file efficiency matters.

## Does 240 FPS mean 240 real frames?

Shadowless requests real frames from the capture source; it does not generate duplicates to make a file report a higher FPS.

- **Game Capture:** can record distinct high-rate frames when the game, GPU, encoder, and capture path can sustain them.
- **Desktop capture:** limited by how often Windows and the monitor present new desktop frames.
- **Slow motion:** 120–240 FPS is useful only when the source actually produces that many distinct frames.

Higher FPS raises GPU load, RAM use, and file size. Shadowless supports 30, 60, 120, 144, and 240 FPS.

## How does audio work?

You can record system or detected-game audio, a microphone, or both.

- Change system and microphone volume independently.
- Add up to `+20 dB` microphone boost when 100% is still too quiet.
- Use **One mixed track** for maximum player compatibility.
- Use **Separate tracks** to keep system/game audio and microphone independent for editing.
- Use **Per-app routing** to select individual running applications and assign each one to an available recording track. Selected apps stay at the top, followed by apps currently playing audio and then other running processes.
- Choose AAC in fragmented MP4 or Opus in MKV.

The selected audio codec and container determine how many tracks are available. Shadowless shows real application icons and live level meters in the routing window, and exposes routed sources as quick toggles on the main screen.

Some media players play only one track from a multi-track file. Shadowless's clip editor previews every available track together and lets you keep or remove each track from the trimmed result.

## Can I manage and trim replays inside Shadowless?

Yes. The main window lists supported files from the selected replay folder in a scrollable library.

- Open a replay in File Explorer.
- Move it to the Recycle Bin after confirmation.
- Play and edit it in the same window.
- Seek, pause, use fullscreen, and change playback speed from `0.25×` to `2×` with the keyboard.
- Scrub through a responsive preview.
- Select one trim range with draggable handles.
- Keep or remove each audio track.
- Save a new copy or overwrite the original after confirmation.
- See estimated output size, original size, resolution, source FPS, and codec.

Trimming uses stream copy when possible, avoiding a full video re-encode.

## Can Shadowless organize clips by game?

Enable **Organize games into folders** in Storage. Replays saved while a game is detected go into a folder named after that game's executable. Replays saved while Desktop remains active stay in the main replay folder.

## Does Shadowless send or upload my recordings?

No. Shadowless has no account, cloud upload, analytics, or telemetry. Replays, thumbnails, settings, and logs stay on your PC.

Public-update-enabled GitHub builds contact the GitHub Releases API to check whether a newer version exists. They download an update package only after you click the update control. The Microsoft Store build uses Store-managed updates instead.

## How is Shadowless different from ShadowPlay and OBS Replay Buffer?

| | Shadowless | ShadowPlay | OBS Replay Buffer |
| --- | --- | --- | --- |
| Primary job | Instant replay | NVIDIA capture suite | Recording/streaming production |
| GPU support goal | NVIDIA, AMD, Intel hardware encoders | NVIDIA | Broad through OBS |
| Setup | Choose settings and leave it in tray | GeForce/NVIDIA App | Configure scenes, sources, and output |
| Automatic desktop/game switching | Yes, in Desktop mode | Platform-managed | Requires scene/source setup |
| Capture watchdog and recovery | Built around automatic supervision | Internal behavior | Not Shadowless's focused workflow |
| Built-in replay library and trim editor | Yes | Varies by NVIDIA software version | No focused clip library |
| Open source | Yes | No | Yes |

Shadowless uses libobs for capture and encoding, but it is not an OBS Studio frontend. It exposes no streaming, scenes, transitions, or plugin management because those features would work against its small, single-purpose UI.

## Will it work on my PC?

Requirements:

- Windows 10 version 2004 or newer, or Windows 11;
- x64 processor and operating system;
- a GPU and driver exposing a supported hardware H.264, HEVC, or AV1 encoder.

Shadowless hardware testing has only been performed on the `linux` branch with
an NVIDIA GeForce GTX 1650. This does not validate the Windows edition.
RTX test reports belong to upstream Captail on Windows, not Shadowless.

Current Shadowless hardware status:

| Hardware | Status |
| --- | --- |
| NVIDIA GeForce GTX 1650 / Linux | Tested on the `linux` branch with Hyprland / Wayland and GSR |
| Windows / all GPUs | Shadowless capture and hardware validation pending |
| Other Linux GPUs | Not tested by this project |

Codec options are based on detected encoder support. Shadowless falls back to another available codec if a previously selected encoder disappears.

## What are the current limitations?

- This is an early preview; hardware-specific bugs are expected.
- Release binaries are not Authenticode-signed.
- Some games or anti-cheat configurations may block Game Capture.
- DRM-protected regions cannot be recorded and may appear black.
- Desktop capture cannot create unique frames beyond the desktop presentation rate.
- Older NVIDIA, AMD, and Intel hardware needs more real-world testing.

## What information should I include in a bug report?

First check [existing issues](https://github.com/Zefth051/Shadowless/issues), then click **Report bug** in Shadowless or open the [bug report form](https://github.com/Zefth051/Shadowless/issues/new/choose).

The in-app button prefills Shadowless version, package channel, Windows build, GPU and driver when available, recording configuration, and a short recent diagnostic excerpt. Shadowless removes personal paths, network addresses, identifiers, window titles, device names, secrets, and uncontrolled third-party output before opening GitHub. Review the form before submitting it; the complete local log and recorded files are never attached automatically.

Include:

- Shadowless version and Setup/Portable package type;
- Windows version;
- GPU model and graphics-driver version;
- capture mode, codec, resolution, FPS, and audio configuration;
- exact reproduction steps and expected/actual behavior;
- additional log context only when the sanitized excerpt does not cover the problem.

Review logs before attaching them and remove personal paths or other sensitive information. Report security vulnerabilities privately through the process in [SECURITY.md](SECURITY.md).

Compatibility reports for older NVIDIA, AMD, and Intel GPUs are useful even when everything works.

Have an improvement rather than a bug? Open **About → Feature** to use Shadowless's focused feature-request template.

## How do I build or contribute?

Development requires Windows 10/11 x64, .NET 10 SDK, CMake 3.20+, and Visual Studio 2022 Build Tools with **Desktop development with C++**.

```powershell
git clone --branch windows https://github.com/Zefth051/Shadowless.git
cd Shadowless
dotnet restore .\src\Shadowless\Shadowless.csproj --locked-mode
dotnet build .\Shadowless.sln -c Debug --no-restore
```

The first build acquires the pinned OBS and FFmpeg runtimes when they are missing.

Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes. Release maintainers should use [docs/RELEASING.md](docs/RELEASING.md) and the [README screenshot plan](docs/SCREENSHOTS.md); published binaries are built and verified by GitHub Actions.

## License and attribution

Shadowless is licensed under [GNU GPL-2.0-or-later](LICENSE).

Privacy details are documented in [PRIVACY.md](PRIVACY.md).

Shadowless uses libobs and selected OBS Studio runtime components. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Shadowless is not affiliated with or endorsed by NVIDIA or OBS Project. NVIDIA and ShadowPlay are trademarks of NVIDIA Corporation.
