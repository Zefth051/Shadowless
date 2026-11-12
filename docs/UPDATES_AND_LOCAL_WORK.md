# Updates and local development

## Current private preview

The repository remains private. The existing `v0.2.4` Windows and
`linux-v0.2.4` Linux packages are validation drafts, with in-app updates disabled.
An installed private build does not start updating merely because the repository
later becomes public. No GitHub token is embedded in either application.

Give a tester repository access and a reviewed package through an authorized
GitHub download. Draft releases need sufficient repository permissions; read-only
source access alone is not enough. Until reviewed packages are released, do not
promise a normal public download or private in-app updates.

A private Windows tester upgrades by running the newer Setup over the existing
installation. Keep the same install directory and retain settings/recordings.
A Linux tester replaces the old AppImage manually after quitting. Current
validation and remaining restrictions are recorded in [release status](RELEASE_STATUS.md).

## After the public transition

Repository visibility and release publication are separate actions requiring the
owner's approval. A public repository with only draft releases still has no
downloadable update for the application.

1. Build a new candidate with public updates enabled: Windows release workflow
   `enable_public_updates=true`; Linux build
   `SHADOWLESS_PUBLIC_UPDATES=true SHADOWLESS_VERSION=X.Y.Z ./linux/build.sh`.
2. Review the candidate, exact dependencies/sources/notices, checksum file and
   platform screenshots. Publish the matching platform release only after approval.
3. Existing private-build users manually install this public-enabled build once.
4. Later published releases with a greater version become eligible updates.

Windows checks when its dashboard/settings window first loads. The UI offers the new version;
the user starts the update, which downloads and checks SHA-256 before invoking
Setup for installed builds or the copy/relaunch helper for Portable builds.
This is not unattended installation. The Windows branch now only offers published
final releases, including on `0.x` builds. Prereleases, drafts and Linux tags are excluded.

Linux public-enabled builds check shortly after startup; the tray also provides
**Check for updates**. The user confirms the download. Shadowless checks the
GitHub SHA-256 digest and replaces the writable regular AppImage atomically.
Quit and reopen after recording to run the new version. Read-only paths and
symlinks require manual updates. Windows Store installations use Store updates.

[Windows native package QA](WINDOWS_UPDATE_VALIDATION.md) passed. This is not
proof of a full production network update:
that still requires two eligible published releases and the real application UI.
See [the validation status](RELEASE_STATUS.md) before claiming otherwise.

## Local work → GitHub → installed apps

`Edit locally → test → commit → push the matching branch → build a candidate
→ review → publish a new platform release → installed app discovers it`.

- `windows` owns the WPF/libobs edition; `linux` owns Qt/GSR.
- Local edits and commits do not reach GitHub until pushed.
- Pushing source does not change an installed copy or create an application update.
- A candidate must be built from the intended committed source; record its commit
  and artifact hashes. Later documentation changes do not alter an existing binary.
- Windows release tags use `vX.Y.Z`; Linux uses `linux-vX.Y.Z`. Versions must
  increase. Keep Linux releases out of the repository's Windows latest channel.
- Never replace released binaries under an existing version. Build a new version.
- Keep SDK downloads, private config, credentials, logs and recordings ignored.
  Check staged changes and run the repository scanner before pushing.

The [GitHub guide](GITHUB.md) describes candidate workflows; the
[release guide](RELEASING.md) covers review/publication gates.
