# Releasing Shadowless

See [current release status](RELEASE_STATUS.md) before building or publishing.
Real in-app upgrades on Windows Setup, Windows Portable and Linux AppImage
remain untested. Complete and record those checks before claiming verified
update support; automated updater QA alone is insufficient.

Shadowless releases are built entirely by GitHub Actions on a pinned Windows runner.

Each release contains:

- Self-contained Portable ZIP
- Self-contained Windows installer with uninstaller
- SHA-256 checksum file
- GitHub build-provenance attestations

## Linux download buttons

When a new Linux candidate becomes available, update the Linux download links in
`README.md` and `site/index.html` on both platform branches to its release page.
Use the URL returned by `gh release view linux-vX.Y.Z --json url --jq .url`:
draft releases have a temporary URL and require repository access. Refresh the
links after publication. Keep Linux releases out of the Windows latest channel.

## Create a release

Before running the workflow:

1. Review changes since the previous tag.
2. Move completed entries from `[Unreleased]` into a dated version section in `CHANGELOG.md`.
3. Follow [RELEASE_NOTES.md](RELEASE_NOTES.md) for categories, wording, and required upgrade notices.
4. Capture the complete README baseline from the exact release-candidate build by following [SCREENSHOTS.md](SCREENSHOTS.md). Replace every required screenshot even when its screen did not visibly change.
5. Verify the screenshot build can actually record and reopen the README showcase profile: hardware AV1, 4K, 240 FPS, system audio, and microphone.
6. Add a real screenshot for each significant new user-facing workflow or screen. Do not use mockups, concepts, generated UI, or screenshots from an older build.
7. Preview the generated GitHub Release description locally:

```powershell
.\tools\New-ReleaseNotes.ps1 `
  -Version 0.2.0 `
  -PreviousTag v0.1.9 `
  -OutputPath "$env:TEMP\shadowless-release-notes.md"
```

Commit and merge the changelog before dispatching the release. The workflow stops if it cannot find a non-empty section matching the requested version.

Then start the release from GitHub:

From GitHub:

1. Open **Actions**.
2. Select **Build release**.
3. Click **Run workflow**.
4. Enter a semantic version without `v`, for example `0.2.0`.
5. Keep **Pre-release** enabled while Shadowless is in preview.

From GitHub CLI:

```powershell
gh workflow run release.yml `
  --repo Zefth051/Shadowless `
  --ref main `
  -f version=0.2.0 `
  -f prerelease=true
```

The workflow validates the version, builds and verifies both packages, creates tag `v<version>`, and publishes the GitHub Release using the matching changelog section.

After publishing, verify:

- release title, version, and prerelease status;
- Installer, Portable ZIP, and `SHA256SUMS.txt` assets;
- generated release notes and full-changelog link;
- README screenshots match the published build, contain no private data, and render at a readable size;
- GitHub build-provenance attestations.

## Local package build

Install Inno Setup 7.1.0 or newer, then run:

```powershell
.\tools\AcquireObsRuntime.ps1
.\tools\BuildRelease.ps1 `
  -Version 0.2.0 `
  -InnoSetupCompiler "C:\Program Files\Inno Setup 7\ISCC.exe"
```

Output is written to `artifacts\release\0.2.0`.

## Version rules

- Patch: compatible bug fix (`0.1.0` → `0.1.1`)
- Minor: compatible feature (`0.1.x` → `0.2.0`)
- Major: breaking change (`0.x` → `1.0.0`)

Never replace published binaries under an existing tag. Publish a new version.

## Microsoft Store release

Microsoft Store is a separate package channel. It must not be attached to a
GitHub Release because Store installs, signs, and updates the package.

Build an upload-ready package locally:

```powershell
.\tools\BuildStorePackage.ps1 -Version 0.2.0
```

Store packaging uses self-contained FFmpeg tools and rejects package-root DLL
collisions before creating the Partner Center upload archive.

Upload `artifacts\store\0.2.0\Shadowless-0.2.0.0-x64.msixupload` in Partner
Center. Full identity, validation, update-channel, and submission instructions
are in [`packaging/msix/README.md`](../packaging/msix/README.md).
