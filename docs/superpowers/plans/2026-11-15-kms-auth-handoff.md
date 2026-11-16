# KMS Authentication Handoff Implementation Plan

> **For agentic workers:** Use executing-plans; preserve the existing dirty Linux worktree.

**Goal:** Make required system authentication accessible without manually dismissing Shadowless.
**Architecture:** The maintained recorder emits explicit start/end markers only around its Polkit path. GsrCapture converts complete log lines into an authentication state signal. Controller temporarily releases its overlay and restores previous visibility when authentication finishes, including failure and cancellation. No password handling, new privileged policy, or unconditional screen-picker migration.
**Tech Stack:** C, C++17, Qt Test, Meson/qmake, AppImage.

The owner explicitly requested hiding/restoring the overlay when authentication is necessary. No additional design approval is needed. Preserve background startup, explicit hide commands and shutdown. Do not change release versions or publish.

- [x] Add failing recorder stream/state and controller visibility regression tests.
- [x] Implement explicit KMS markers, bounded complete-line parsing, and overlay handoff; update source provenance/hash.
- [x] Rebuild recorder/app and run relevant suites; package and verify a new private test artifact.
- [x] Mark completed Linux roadmap work separately from shared Windows parity and add an Unreleased changelog.
- [x] Check GitHub access read-only; leave publication and final version for owner approval.


## Verification

- Watched the missing auth signal/controller integration regressions fail before implementing the handoff.
- Built recorder 6.1.7 and its matching helper from the modified, hash-verified source in `.qa/kms-auth-build`; copied them into the local app build for packaging. Used isolated Meson 1.9.1/Ninja 1.13.0 and existing local Vulkan headers; no system package installation.
- App and all qmake targets compile. UI: 58 passed, 2 skipped. Recorder: 24 passed, 7 live hardware/fixture skips. Packaging: 19 passed. Overlays: 12, player: 8, playback: 9, preview: 8, shortcuts: 10 passed. Desktop entry check passes.
- Final bundled runtime repeats UI 58 passed / 2 skipped and updater 7 passed, including actual public HTTPS download, SHA-256 and atomic replacement of a temporary AppImage. The installed application was not replaced.
- Private AppImage `.qa/qol-auth-20261115/Shadowless-0.3.3-linux-x86_64.AppImage` checksum and bundled version check pass. The bundled recorder contains both authentication markers. Version stays 0.3.3 until a release version is approved.
- Fresh-context review found no concrete bugs in the scoped handoff, parser, visibility and ownership changes.
- GitHub account is already authenticated with repository/workflow access. No push, tag or release performed.
- Real Polkit interaction on Hyprland/Plasma and the public 0.3.3-to-next-release upgrade remain owner validation. No credentials or persistent privilege policies changed.
