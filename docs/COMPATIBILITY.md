# Compatibility and support policy

Single source of truth for which systems Shadowless targets, their real minimum
requirements, and how support is claimed. The rule behind it is in
[PRODUCT_PHILOSOPHY.md](PRODUCT_PHILOSOPHY.md#compatibility-and-reliability).
Other documents link here instead of restating requirements.

A priority system is a target we design and test for. It is **not** a promise
that every GPU, driver or desktop configuration works. A claim needs evidence
(see [Evidence levels](#evidence-levels)).

## Priority systems (current)

| System | Notes |
| --- | --- |
| Windows 10 | x64, version 2004 (build 19041) or newer, including 22H2 (build 19045). |
| Windows 11 | x64. |
| Arch Linux | x86_64, native Wayland: Hyprland and KDE Plasma 6. |
| CachyOS | Same as Arch Linux. |

## Minimum requirements (what the code and installers actually require)

### Windows

- Windows 10 version 2004 (build 19041) or Windows 11, x64. The installer refuses
  older builds (`MinVersion=10.0.19041`).
- A GPU with Direct3D 11 and a supported hardware encoder (NVENC, AMF or Quick Sync).
  Bundled OBS (`libobs` 32.2.2) is used; OBS Studio is not needed. .NET is included.
- **Per-application audio routing (Advanced audio, separate tracks):** requires
  Windows build 20348 or newer (Windows 11 or Windows Server 2022) and the bundled
  `shadowless-process-audio` plugin. On Windows 10 and when the plugin is missing,
  Shadowless captures mixed audio instead (see ADR 0002 and ADR 0006). This fallback
  ships in Windows 0.3.4; Windows 0.3.3 does not have it and cannot start capture on
  Windows 10 with those settings saved.
- Visual C++ runtime (x64): the native OBS components import `VCRUNTIME140`,
  `VCRUNTIME140_1` and `MSVCP140`, and the Portable/Setup do not bundle them. A PC
  without the Visual C++ Redistributable is expected to fail to load OBS. Not yet
  reproduced on a clean machine; tracked as an independent improvement
  (`tools/check_runtime_dependencies.py`).

### Linux (Arch Linux and CachyOS)

- x86_64, a native Wayland session with Hyprland or KDE Plasma 6 (layer-shell).
  Plasma is implemented but its live capture, tray and shortcut approval are not
  validated on a real session. GNOME, X11 and other compositors are not targets.
- PipeWire with `pipewire-pulse`, `pactl`, FFmpeg/`ffprobe`, working GPU drivers
  (VA-API/NVENC through the bundled GSR engine). The AppImage needs FUSE, or
  `--appimage-extract-and-run`. Check with `--check-dependencies`.
- Core capture (Instant Replay, Recording) does not depend on optional permissions.
- **Optional**: Polkit authentication agent only when direct KMS capture or
  Peripheral overlay input access asks for authorization. Declining or lacking it
  never disables capture.
- Glibc baseline: measured on the 0.3.8 development AppImage (built on CachyOS): the app needs GLIBC 2.34 but the
  bundled libraries need **GLIBC 2.44**, so it runs only on systems at least that new (current Arch/CachyOS). Older
  distributions need an AppImage built on an older baseline (`docs/LINUX_APPIMAGE_VALIDATION.md`).

## Evidence levels

| Level | Meaning | Allowed wording |
| --- | --- | --- |
| E0 | Designed or implemented, nothing run | "implemented", "planned" |
| E1 | Automated tests or CI (unit, offline, runner) | "tests pass" (say what the runner is) |
| E2 | Checked by a maintainer on a real system of that kind | "verified on <system>" |
| E3 | Repeated real-system reports plus acceptance checklist complete | "supported" |

"Supported" for a system needs E3 for install, capture, audio, permissions,
updater and desktop behavior. CI is E1 only: GitHub's `windows-2022` is build
20348, so it cannot exercise Windows 10 behavior; Windows 10 cases must be
simulated in tests (for example build 19045) **and** checked on a real
Windows 10 PC before they are called verified.

## Current status

| System | Status |
| --- | --- |
| Windows 11 | Targeted; real-system verification pending per release status. |
| Windows 10 | Targeted. A Windows 10 22H2 (build 19045, RTX 2060) PC confirmed the 0.3.3 capture-start regression from its log; the 0.3.4 fix is E1 (CI) until re-tested on that PC. |
| Arch Linux / CachyOS | Targeted. Hardware testing so far: GTX 1650 on Hyprland. Plasma not validated. |
| Fedora, Ubuntu, Debian, others | Not supported. Future objectives, see below. |

## Expansion plan for more Linux distributions (future, subject to validation)

No distribution below is officially supported until its stage is complete and
recorded here. Order is a proposal, not a schedule; stages never delay fixes
for the priority systems.

| Stage | Targets | Why this order |
| --- | --- | --- |
| 0 | Arch Linux, CachyOS | Current priority; baseline for every check. |
| 1 | Fedora (Workstation/KDE) | Rolling-ish, Wayland and PipeWire by default, close to Arch. |
| 2 | Ubuntu (current LTS) | Largest user base; older glibc and FUSE/packaging differences. |
| 3 | Debian (stable) | Older libraries; stricter packaging baseline. |
| 4 | Other relevant distributions (for example openSUSE) | By demand, after the above. |

Known risks to verify, not assumptions: glibc baseline of the AppImage, FUSE
availability, codec/VA-API packaging (Fedora), default desktop (Ubuntu and
Debian default to GNOME, which Shadowless does not target, so Plasma or Hyprland
must be installed), sandboxing/AppArmor/SELinux, Polkit agent presence,
portal backends.

### Acceptance criteria for each distribution

A distribution moves to "supported" only when all of these are recorded with
evidence (E2 at minimum, E3 to declare support):

1. **Install:** the single AppImage runs with the distribution's default tools;
   `--check-dependencies` reports every missing package with the exact package name.
2. **Dependencies:** required packages are available from the distribution's own
   repositories or bundled; no manual builds.
3. **Capture:** Instant Replay and Recording produce playable clips on Hyprland
   and/or KDE Plasma 6 Wayland, including the KMS authorization path.
4. **Audio:** PipeWire capture of system audio and microphone; separate-track
   mode where offered; fallback when unavailable.
5. **Permissions:** optional authorization flows work or degrade silently
   without a Polkit agent; no administrator rights are required for core use.
6. **Updater:** discovery, SHA-256 check and in-place AppImage replacement work
   from a previous release.
7. **Desktop behavior:** layer-shell overlay, tray, shortcuts and autostart.
8. **Docs and checks:** requirements for that distribution are written in this
   file and `docs/INSTALL.md`, and a repeatable check (script or CI job) exists.
