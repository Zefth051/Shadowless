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
findings. Eight scanner regressions pass on each branch. No matching secret was
found to remove. Reviewed primary README screenshots use demo clip/app names.
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
