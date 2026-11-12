# Publication review — 2026-11-11

## Scope and repository

The user authorized uploading the Windows and Linux branches to GitHub and
preparing platform releases. `Zefth051/Shadowless` was created privately for
validation; only `windows` and `linux` are intended for upload. Existing platform
work and upstream history are preserved. Archives, SDKs, runtime downloads,
recordings and local diagnostic output are excluded.

## Sensitive data review

The current non-ignored source files, index and all reachable history were scanned
for recognizable credentials and sensitive filenames on both branches, with zero
findings. Eight scanner regressions pass on each branch. No matching credential was
found. Manual review subsequently found internal Windows test notes with local
paths and recording filenames. They were removed from the six new Windows commits;
a backup remains only in ignored local storage. Upstream history was unchanged.
Reviewed primary README screenshots use demo clip/app names.
Upstream author attribution is retained. Pattern scanning cannot establish the
absence of every possible private datum.

## Licenses and attribution

The original GPL-2.0-or-later license and Captail/FaulMit attribution are retained.
Full GPLv3, LGPLv3 and LGPLv2.1 license texts are now included and packaged.
The pinned Windows libmpv build is not certified LGPL solely from DLL strings;
the notice now explicitly states that verification is pending.

Binary distribution is not yet cleared: exact transitive notices and corresponding
sources/build inputs must be collected for bundled OBS, FFmpeg, libmpv and Linux
Qt libraries, and icon/sound/screenshot provenance still needs supporting records.
See `DEPENDENCY_AUDIT.md`. A Shadowless source ZIP alone is insufficient. Build
workflows create validation drafts, not an assertion of redistribution compliance.

## Product and release behavior

`FEATURES.md` compares platform behavior and installation requirements.
`AI_DEVELOPMENT.md`, both READMEs, release notes and About text disclose AI-assisted
development while retaining upstream authorship. Public Windows builds reuse the
checksum-verifying updater. Public Linux builds discover only matching Linux
releases, require a GitHub SHA-256 digest, and replace writable regular AppImages
atomically after confirmation. Users reopen after recording to apply the update.
Private/development builds do not embed tokens and update manually.

## Validation

Linux build/offline tests: 104 passing QtTest results, zero failures, nine optional
skips, plus desktop identity check. Six packaging and eight scanner tests pass.
New updater tests were first observed failing and pass after implementation.
An independent review found the updater test target omitted from clean builds;
that target is now included in the generated top-level Makefile.

Windows public/private updater QA passes, including pagination, repository trust,
Store isolation and transient launcher-copy retries. Managed WPF compilation with
Windows targeting and native targets suppressed passes with zero warnings/errors;
this does not validate native OBS or an installed Windows application. Workflow
YAML and project/XAML XML parse. Native Windows installer execution and clean-host
Hyprland capture remain required. Actual network update/install across two releases
has not yet been exercised.

## GitHub execution follow-up

Both platform branches were pushed and `windows` selected as the default. GitHub's
repository safety and agent-context checks passed. Initial release attempts exposed
container Git ownership trust and missing Git executable modes; both are fixed.
The old mpv/FFmpeg release downloads had expired upstream. Windows now pins mpv
`bcde6b93cc` (2026-11-11) and FFmpeg `n9.0.2-49-gc179411bbb`; archives were downloaded
and independently SHA-256 verified against their upstream asset digests. The
Windows dependency-pin validator passes. The upstream FFmpeg license was previously
dropped by acquisition; a reproduced failure now passes after preserving that text,
and the Windows release workflow checks it in the final Portable package.

Local AppImage `linux-0.2.4-publication` was rebuilt with update code, AI disclosure
and GNU license texts; its published checksum file verifies and extracted startup
reports `shadowless 0.2.4`. Cloud release builds are tracked in GitHub Actions;
private drafts remain subject to the binary distribution audit above.

## Windows release evidence

GitHub run `43146952853` completed successfully: native packages built, installer
installation and uninstallation passed, Portable layout and embedded FFmpeg license
were checked, and asset SHA-256 values were verified. Draft `v0.2.4` contains Setup,
Portable and checksums, targeting sanitized commit `8d4670694cfcc997a4dfa9c4cb65e3d85e533eab`.
No GPU/game capture validation on a physical Windows host is implied.

Linux container validation required explicit Qt WebP plugins, a writable isolated
Trash/data directory and a timed null audio sink for headless playback. These
changes apply to validation environments, not the user's audio configuration.

The full Windows analyzer gate found two pre-existing issues: an unchecked Win32
thread/process lookup and a native-window ownership warning in the overlay preview.
The lookup now handles failure; the preview documents its inherited HwndHost
finalizer, matching the existing MpvHost policy. The same analyzer command passes
locally with zero errors and 1,306 non-gating existing warnings; native target
suppression was used only for this Linux-hosted managed check.

## Linux release evidence

GitHub run `43148322139` completed successfully with 103 passing QtTest results,
zero failures and ten optional/live/graphics skips, plus the desktop identity
check and six packaging tests. It generated and smoke-tested the AppImage and
created draft `linux-v0.2.4` at `d5ed159591c41c8ddd98ca5e82858642fb918c9b`.
The cloud artifact records its Arch build packages; broad distribution/driver
compatibility is not established.

Both releases remain private validation drafts. Their checksum files were fetched
and compared with GitHub's asset digests: Windows Setup/Portable and Linux
AppImage all match. No published binary redistribution clearance is asserted.
The maintained branches may advance past the exact candidate commit recorded
in each draft. Private builds use manual updates; public-enabled update behavior
is documented in `FEATURES.md` and `INSTALL.md`.

The original platform worktrees are retained. Private diagnostic notes, audio
snapshots, build logs, downloaded runtimes and original-note backups stay ignored
and are not part of the GitHub source branches or release packages.
