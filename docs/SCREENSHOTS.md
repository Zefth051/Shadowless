# Shadowless README screenshot plan

## Current provenance and status

The inherited PNG files show Captail or earlier upstream builds. Their
`shadowless-*` filenames do not establish that they depict the current fork.
They are historical references and must not appear as current Shadowless product
images in README, the website, Store listings or release notes. Fresh Windows
screenshots remain pending. Six real Linux candidate captures are available in
[the Linux gallery](screenshots/linux/README.md), with exact build provenance.
Capture each edition separately and record the platform, source commit, displayed
version and capture date.

The Windows checklist below is a capture procedure, not evidence that it has
been completed for Shadowless. Linux needs real captures of its Qt dashboard,
capture/audio settings, clip library, editor and Hyprland overlays using
supported settings; Windows screenshots cannot stand in for them.

## Capture purpose

README screenshots answer one user question: what will Shadowless look like after I install this release?

Every release must ship a fresh screenshot set captured from the exact release-candidate build. A screenshot from an older version is stale even when the screen appears unchanged.

## Required set for every release

| README asset | Required state | What it must show |
| --- | --- | --- |
| `docs/shadowless-main.png` | Instant Replay active | Complete main window showing `AV1 · 4K · 240 FPS`, enabled system and microphone audio, current version, Save button, and several real recent-replay rows. |
| `docs/shadowless-feature-recording.png` | Continuous Recording active | Complete main window with Recording selected, active recording status, `AV1 · 4K · 240 FPS`, audio sources, Stop recording action, current version, and real recent clips. |
| `docs/shadowless-settings-video.png` | Video settings | Hardware AV1 selected at `3840 × 2160` and `240 FPS`, plus source, bitrate, quality profile, display, and resolution controls. |
| `docs/shadowless-settings-audio.png` | Audio and replay settings | Enabled system audio and microphone, separate tracks, volume, microphone boost, buffer, storage, and hotkey controls where they fit naturally. Use a second real settings screenshot when one readable frame cannot contain them. |
| `docs/shadowless-audio-routing.png` | Application audio routing open | Selected applications first, real application icons, live level meters, microphone assignment, track selectors, and additional running processes. |
| `docs/shadowless-player.png` | Real replay loaded in the combined player and editor | Working AV1 4K 240 FPS playback, video timeline, trim range, available audio tracks, output settings, save actions, fullscreen control, and matching media details. |

Overwrite these stable files on every release instead of adding versioned duplicates.

## README showcase profile

Public README screenshots must present Shadowless's high-end recording configuration:

- hardware AV1 encoder;
- `3840 × 2160` recording resolution;
- `240 FPS` capture;
- high-quality encoder profile and a sensible tested bitrate;
- system audio and microphone enabled;
- separate audio tracks where the screen exposes track configuration;
- an active replay buffer and a successfully saved replay produced with the same profile.

Capture these screenshots on hardware that exposes and successfully records this profile. Do not edit labels, enable unavailable controls, or use a lower-quality clip whose metadata merely claims AV1 4K 240 FPS. If the release candidate cannot produce and open a real replay with these settings, resolve or document that product problem before refreshing README.

## Significant feature screenshots

Add a separate screenshot when a release introduces or substantially redesigns any of these and the required set does not already show it:

- a top-level workflow;
- a new window, panel, editor mode, or settings group;
- a visible capture, recovery, save, update, or error state;
- a feature that is difficult to understand from one short paragraph;
- a visual change important enough to lead the release notes.

Use `docs/shadowless-feature-<short-name>.png`. Keep it in README while the feature remains important to first-time users. Remove obsolete feature screenshots rather than turning README into release history.

Small copy changes, internal performance work, dependency updates, and invisible bug fixes do not need their own feature screenshot. They still require the fresh six-image release baseline.

## Capture standard

- Capture the release candidate built from the same commit that will receive the tag.
- Use Shadowless's English interface for the public README.
- Use real application windows and real functioning controls. Never substitute Figma, generated UI, concept art, or a reconstructed composition.
- Use safe demo data. Remove personal names, full user paths, device identifiers, notifications, account details, and unrelated applications.
- Use neutral replay filenames and game footage that can be published legally.
- Hide the mouse cursor unless its position is essential to explain an interaction.
- Keep Windows scaling and application size consistent across releases. Capture at native resolution; do not upscale.
- Prefer PNG for application UI. Use JPEG only when a large photographic video frame makes PNG unreasonably heavy.
- Crop consistently around the full Shadowless window. Do not leave random desktop borders or another application behind it.
- Verify text is readable at the width used in README and write specific alt text describing the visible workflow.

## Release workflow

1. Build the release candidate and confirm the displayed version.
2. Prepare safe replay files and settings that demonstrate actual behavior.
   When native publishable 4K media is unavailable, create the deterministic editor fixture from a real 240 FPS Shadowless replay:

   ```powershell
   .\tools\New-ReadmeShowcaseReplay.ps1 `
     -Source 'D:\Shadowless-Screenshot-Media\source-av1-240.mkv'
   ```

   This preserves every source frame and available audio track while spatially scaling video to 3840×2160 and encoding AV1. It is suitable for demonstrating editor UI and metadata, but it is not evidence of native 4K capture quality or performance.
3. Run the capture tool from the repository root:

   ```powershell
   .\tools\CaptureReadmeScreenshots.ps1 `
     -ShadowlessExe .\artifacts\release\Shadowless-0.2.0\Shadowless.exe `
     -ExpectedVersion 0.2.0 `
     -ReplayFile 'D:\Shadowless-Screenshot-Media\showcase-av1-4k-240.mkv'
   ```

   The tool backs up the user config, applies the English AV1/4K/240 showcase profile, opens each screen through Windows UI Automation, captures cursor-free PNG files, verifies player media with bundled `ffprobe`, and restores the config even after a failure. Use `-SkipPlayer` only while preparing the required showcase replay, or `-PlayerOnly` to replace only `shadowless-player.png`. `-AllowNonShowcaseReplay` exists for local harness testing and must not be used for release screenshots.
4. Inspect all six PNG files at full size. Automation removes repetitive interaction; it does not replace visual review.
5. Capture each significant new feature or redesigned workflow.
6. Replace stable README assets and update image references or alt text when needed.
7. Open README locally or on the release PR and check image order, sizing, clipping, and dark-theme contrast.
8. Confirm `git diff` contains no accidental private information or cursor overlays.
9. Merge screenshots in the same release PR as the version and changelog.

## Current refresh queue

- Capture the current Shadowless Windows candidate on native Windows.
- Linux candidate `linux-v0.2.4`: six real captures completed on Hyprland; see
  [the gallery and provenance](screenshots/linux/README.md). Refresh for each new candidate.
- Review branding, safe demo data and platform-specific feature claims.
- Replace inherited images only after visual review and record capture provenance.

Earlier upstream showcase and hardware-validation notes do not establish that
these Shadowless candidates were captured or tested. Do not use concept graphics
as product screenshots.
