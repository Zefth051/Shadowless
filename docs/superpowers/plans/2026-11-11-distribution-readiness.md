# Distribution Readiness Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Prepare both platform branches for safe private staging and eventual public releases, and fix the Linux notification.

**Architecture:** Reuse Qt, existing release scripts and the Windows updater. Keep platform changes in their existing worktrees and preserve pending work.

**Tech Stack:** Qt 6/C++17, WPF/.NET 10, PowerShell, Python standard library, GitHub Actions, Inno Setup, AppImage tools.

**Spec:** `docs/superpowers/specs/2026-11-11-distribution-readiness-design.md`

## Global Constraints

- English repository text; no credentials in source, binaries or logs.
- No push, release publication, visibility change or version bump.
- Windows public updates are opt-in at build time; private builds remain blocked.
- Linux remains Hyprland/Wayland; no Linux self-update.
- Preserve existing uncommitted changes; leave this work uncommitted for review.

## Review Focus

- Repeated notifications restart the countdown; hidden windows stop repainting.
- Older game collections remain accessible when other collections have newer clips.
- Portable update targets the package root, never nests runtime/runtime.
- Secret scans include reachable history and filenames without printing secret values.
- AppImage does not point desktop launchers at an ephemeral mount or imply GSR is bundled.

### Task 1: Linux notification and bounded library scan

Files: `linux/ui/clip_notification.{h,cpp}`, `linux/ui/library.cpp`, `linux/tests/tst_ui.cpp`.

- [x] Add a rendered-pixel countdown/replacement regression, run it and observe failure.
- [x] Animate the bar using elapsed time and stop its timer on hide; run the regression.
- [x] Add a multiple-collection >20 clips ordering regression; bound scan results per collection and retain global order.
- [x] Run `./linux/build.sh`; record results and a synthetic sorting benchmark.

### Task 2: Windows organization and public update builds

Files: `src/Shadowless/Distribution/`, `src/Shadowless/Diagnostics/`, linked QA projects/scripts, `Shadowless.csproj`, `BuildRelease.ps1`.

- [x] Move only non-XAML distribution/diagnostic files and update every explicit path; run layout/diagnostic QA.
- [x] Add public-build update QA and observe the missing-switch failure.
- [x] Add `EnablePublicUpdates` default false, propagate it through release builds, preserve Store isolation.
- [x] Correct portable root resolution for the organized layout and test root/payload/trust-boundary cases.

### Task 3: Repository safety and provenance

Files on both branches: `.gitignore`, `tools/check_secrets.py`, `tools/tests/test_check_secrets.py`, `.github/workflows/security.yml`, notices and publication docs.

- [x] Test current files, staged files, history-only secrets, sensitive filenames and sanitized output before implementing scanner.
- [x] Implement standard-library scanner; run tests and full history scans.
- [x] Preserve Captail attribution and add a dependency/source compliance checklist for exact redistributed builds.

### Task 4: Linux AppImage and release UX

Files: `linux/packaging/`, `linux/tests/packaging/`, `.github/workflows/linux-release.yml`, README and release docs.

- [x] Test packaging contract and hostile/missing inputs before implementing the package script.
- [x] Reuse existing Linux build outputs, bundle runtime libraries with verified AppImage tools, add a dependency-checking launcher and package smoke check.
- [x] Add an opt-in Linux workflow with artifact upload; release publication remains a separate explicit action.
- [x] Document Windows installer and Linux AppImage download/install flows, private access and public transition gates.
- [x] Run relevant suites, inspect both diffs, and obtain one independent review of all changes.

Execution evidence and remaining release gates: `docs/DISTRIBUTION_READINESS.md`.
Native Windows execution and full dependency sources remain outstanding release gates.
