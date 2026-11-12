# Publishing Shadowless on GitHub

See [current packages and outstanding validation](RELEASE_STATUS.md). Source is
public; platform packages remain unpublished validation drafts. Public source
access does not grant access to draft assets.
Real in-app upgrades on Linux and Windows remain untested.

Use one GitHub repository named `Shadowless` with the description
**Shadowless - An app for clipping**. Use `windows` as the default branch and
`linux` for the native Qt/Hyprland edition. The default Windows branch contains dispatchable workflows for both platforms;
select the matching branch when building.

From the Windows Git worktree, after authenticating with GitHub CLI:

```sh
gh repo create Zefth051/Shadowless --private --description "Shadowless - An app for clipping"
git remote add shadowless https://github.com/Zefth051/Shadowless.git
git push -u shadowless windows linux
gh repo edit Zefth051/Shadowless --default-branch windows
git ls-remote --heads shadowless
```

Keep the repository private until the release gates below pass. If the
repository already exists, add its URL as the `shadowless` remote and start at
the push command. The final command should list both platform branches.

Release assets are shared by all branches in a GitHub repository. Select
`windows` when running the Windows release workflow. Give Linux releases
platform-specific tags such as `linux-v0.2.4` and attach Linux assets there.

## Required local review

Run in both platform checkouts before the first push:

```sh
python3 -m unittest discover -s tools/tests -p test_check_secrets.py
python3 tools/check_secrets.py --history
git status --short
git diff --check
git diff --cached --stat
```

The scanner inspects tracked/current non-ignored files, the index and all reachable
branches/tags/stashes. It reports paths/rules without secret values. This guards
common credentials; it cannot prove all private information is absent. Review
screenshots, logs, Git configuration and release archives separately. `.gitignore`
does not remove history. Revoke real leaked credentials, then clean history before
pushing; never blindly rewrite the shared repository.

Commit reviewed source separately on each branch: pending Linux files are required
for the native application and a push omits uncommitted work. Push only reviewed
branches, avoiding `--mirror`, `--all` and unreviewed tags. Never push the parent
workspace, `archives/`, runtime downloads, local SDKs or recordings. Inspect/reuse
existing remotes and preserve upstream. The commands above are instructions,
not authorization for an agent to publish.

CI's `Repository safety` workflow repeats the scan. Store credentials in GitHub
Secrets; ordinary actions use the temporary scoped `GITHUB_TOKEN`. Do not put
personal tokens into the app, source or remote URL.

## Private test packages

Windows: dispatch **Build release** on `windows`, keeping
`enable_public_updates=false` and `publish=false`. The workflow creates installer,
Portable ZIP and checksums, tests installation/uninstallation on Windows, and
creates a **draft** release. Provenance attestation runs only when public.

Linux: dispatch **Build Linux AppImage preview** on `linux`. It builds in an Arch
container, runs offline tests and uploads AppImage/checksums/package inventory
as workflow artifacts and creates a draft `linux-vX.Y.Z` release. It does not
publish the draft; complete the dependency audit before sharing its binaries. Its workflow also exists
on the default Windows branch to permit dispatch with `--ref linux`.
See `linux/packaging/README.md` on the Linux branch for local packaging.

Private releases/artifacts require authorized GitHub access. Testers download
through GitHub; the application never receives a private repository token.
Complete the [source/license audit](DEPENDENCY_AUDIT.md) before sharing binaries
with recipients, including private testers.

## Public transition

Review current files, index, history and release contents for secrets/personal data;
complete exact dependency sources/notices; verify Windows upgrades and the AppImage
on a clean supported host; replace old screenshots. Changing visibility remains
a separate explicit action in GitHub Settings.

For a public Windows build, set `enable_public_updates=true`; the workflow rejects
this while the repository is private. Local builds use
`BuildRelease.ps1 -EnablePublicUpdates` or MSBuild `-p:EnablePublicUpdates=true`.
Private builds already installed need one manual upgrade to that public-enabled
build. Desktop updates then use the in-app flow, repository-scoped URLs and
SHA-256 verification. Store updates remain Store-managed. Linux public builds use the tray updater;
AppImage replacement takes effect on the next launch. Build with
`SHADOWLESS_PUBLIC_UPDATES=true SHADOWLESS_VERSION=X.Y.Z ./linux/build.sh`.
Private/development Linux builds continue to update manually.

| Platform | Tag | Download |
| --- | --- | --- |
| Windows | `vX.Y.Z` | `Shadowless-X.Y.Z-Setup-win-x64.exe`, optional Portable ZIP |
| Linux | `linux-vX.Y.Z` | `Shadowless-X.Y.Z-linux-x86_64.AppImage`, initially Hyprland |

Releases are repository-wide. Never mark Linux as the repository's latest Windows
release. Windows parsing ignores `linux-v` tags. Keep platform checksum files in
their respective releases. For reviewed Linux artifacts, manually create a draft
release targeting the exact Linux commit and attach AppImage, checksums, inventory
and required sources/notices. Review before publishing; never overwrite assets.

README download links and [installation instructions](INSTALL.md) let users
install directly from GitHub. Pages is optional and disabled while private.
