# Distribution readiness — 2026-11-11

This records the earlier local preparation. See [the current publication review](PUBLICATION_REVIEW.md)
for subsequent GitHub publication, Linux update work and current validation.

Prepared locally; no commits, pushes, GitHub publication or visibility changes
were performed. Existing pending platform/icon work was preserved.

## Implemented

- Linux clip notification uses a linear lifetime animation, resets on replacement
  and stops on hide. The previously static bar has a rendered-pixel regression.
- Linux library retains the newest 20 clip records per collection, then merges
  those candidates for the global newest 20. Every game filter remains available.
- Windows non-XAML distribution and diagnostics sources are grouped into folders;
  namespaces, SDK compilation and explicit QA/tool links are preserved.
- Windows updates default off. Explicit public builds enable them; Store remains
  isolated. URLs are repository-scoped, assets remain SHA-256 verified, API pages
  are followed across Linux releases, and organized packages update from their
  root. The helper retries temporary launcher replacement failures for 15 seconds.
- Both branches have credential exclusions and a tested current/index/history
  scanner, including sensitive historical names after unchanged renames and
  redaction of credentials in displayed filenames. CI repeats it.
- Windows release workflow defaults to a draft with updates disabled, blocks
  public-update mode while private and skips public attestation while private.
  Pages is disabled while the repository is private.
- Linux AppImage builder verifies downloaded tools/runtime hashes, bundles selected
  Qt plugins and dependencies, preserves the overlay path, isolates host-tool
  library paths, checks dependencies and smoke-tests the extracted application.
  Desktop identity points to the permanent AppImage, not a temporary mount.
- README installation links, private/public release instructions, Captail credit
  and a dependency/source audit are available on both branches.

## Evidence

Linux `./linux/build.sh`: 92 passing Qt test results, zero failures, seven
live/optional skips; separate desktop-identity check passes. Python: eight secret
scanner tests and six packaging tests pass. Both platform history scans report
zero findings for the recognized patterns. Workflow YAML and `git diff --check`
pass. A fresh independent review found four Important issues; each was reproduced
with a failing regression, fixed, and verified green. No Minor findings remained.

Synthetic library measurement (20,000 clips, 10 collections, one local sample):
original scan 82 ms / 59,328 KiB peak RSS, bounded scan 79 ms / 53,560 KiB.
This supports lower metadata memory use; it does not establish a significant
speedup, reduced recorder GPU use or improved game FPS.

Windows Portable layout, Store lifecycle, diagnostic persistence/export,
private/public update policy, archive traversal, pagination and transient-copy
recovery tests pass. The copy test simulates a sharing violation and performs real
file copying; on Windows it also holds a real file-sharing lock. Actual native
Windows execution was not available here. Managed C#/WPF builds pass with zero
warnings/errors when Windows targeting is enabled and native tasks are suppressed
using an ignored validation-only targets file. Both public and private compile
flags were reflected from production assemblies. This does not produce a tested
installer or validate native OBS components.

Local artifact: `artifacts/release/linux-0.2.4-verified/Shadowless-0.2.4-linux-x86_64.AppImage`
(about 102 MiB), with `SHA256SUMS.txt`. Extracted offscreen `--help`, actual runtime
`--appimage-extract-and-run --check-dependencies`, and checksum verification pass.
It is a local preview, not cleared for sharing/public distribution.

## Remaining release gates

1. Complete exact third-party license texts, build configurations, corresponding
   source and asset provenance in [DEPENDENCY_AUDIT.md](DEPENDENCY_AUDIT.md).
   Local Linux FFmpeg is GPL-3.0-only, unlike the selected Windows LGPL build.
2. Generate and exercise installer/Portable upgrades and uninstall on native
   Windows; include real launcher locking, interrupted copy and recovery checks.
   A failed copy reports a log/error; automatic rollback is not implemented.
3. Test AppImage on a clean supported Hyprland host, including actual GSR capture,
   driver and overlay ABI compatibility. Its glibc baseline is from this current
   CachyOS/Arch host; broad distribution compatibility is not established.
4. Review/commit intended pending files on each branch, create/push the private
   repository only when explicitly authorized, and test actual Actions/access.
   Publication and public visibility remain separate explicit actions.

See [GITHUB.md](GITHUB.md) for the sequence and [INSTALL.md](INSTALL.md) for users.
