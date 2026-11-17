# Linux 0.3.8 development: AppImage validation (2026-11-17)

Source: `dev/linux-0.3.8` at `eba8105`. Built on CachyOS x86_64 (glibc 2.44, Qt 6.12.0) in a private build dir
(`/tmp`), without touching the maintainer's running session. Nothing was installed, no capture was started and no
normal application instance was launched. Verification level: E1/E2 for the packaging only; no desktop behavior.

## Results

| Check | Result |
| --- | --- |
| `linux/build.sh` (engine, app, all offline suites) | Pass. 17 suites, 0 failures (skips: 2 UI, 1 updates, 7 player/playback environment-dependent). Includes input access 21, helper 10, evdev 48, recorder 22, camera, shortcuts. |
| `python3 -m unittest discover -s linux/tests/packaging` | 34 pass (after the long-`TMPDIR` fixture fix below). |
| AppImage build (`build_appimage.py`) | Success, 103 MB, SHA-256 recorded. Built as 0.3.7 because the app's default version is 0.3.7; a real 0.3.8 needs `SHADOWLESS_VERSION=0.3.8` at build time. |
| `--check-dependencies` | Exit 0 on this host. |
| `--version`, `--help` (offscreen) | Pass. There is no offscreen normal-UI mode, so UI start is not covered here. |
| Payload | `usr/libexec/shadowless/shadowless-input-setup` (0755), `usr/engine/{gpu-screen-recorder,.bin,gsr-kms-server,helper-libs}`, `usr/plugins`, provenance and licenses. |
| Input helper in the image | Runs, no arguments -> exit 2 with a JSON error. |
| Staged-helper flow with the **real bundled helper** (same script as `InputAccess`, run unprivileged against a temp root, no pkexec) | Correct hash: install and remove succeed and the rule file is generated from the template. Tampered stream (truncated helper): rejected by `sha256sum -c`, exit 1. Staging directory removed in every case. |
| Polkit policy | Not shipped in the AppImage (by design: it targets a root-owned `/usr/libexec` path, used by distribution packages). The AppImage uses the generic pkexec prompt via the staged script. Real pkexec dialog not exercised (it would interrupt the desktop session). |

## glibc baseline (important for compatibility)

- Top-level app, helper and engine binaries need GLIBC 2.34.
- The **bundled libraries need GLIBC 2.44** (for example `libavutil.so.61`, `libavfilter.so.12`, `libglslang.so.16`,
  `libluajit-5.1.so.2`), because they were bundled from the CachyOS build host. A system older than glibc 2.44 cannot run
  this AppImage. That is acceptable for current Arch/CachyOS, and it means Fedora, Ubuntu and Debian are **not** covered
  until the AppImage is built on an older baseline (see `docs/COMPATIBILITY.md`, stages 1-3).

## Fixes made

- `linux/tests/packaging/test_dynamic_audio.py`: the spool fixture copied an absolute `TMPDIR` into GSR's 64-byte field
  and aborted under a long path. It now runs inside its temp directory (production already uses a short template).

## Not verified

Real pkexec authorization dialog (Hyprland `hyprpolkitagent`, Plasma agent), live capture and KMS authorization from the
packaged image, tray/overlay on a real session, upgrading an installed 0.3.7 AppImage in place, FUSE-less hosts.
