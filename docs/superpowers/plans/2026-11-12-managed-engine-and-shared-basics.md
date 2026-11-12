# Managed engine and shared basics implementation plan

> **For agentic workers:** Use executing-plans to implement and verify each task.

**Goal:** Bundle a Shadowless-managed GSR engine, complete shared startup/folder actions, and upload a Windows installer with final-release updates.

**Architecture:** Preserve WPF/libobs and Qt/GSR subprocess boundaries. Vendor the approved GSR source and matching helper; keep Shadowless as the only interface. Public Windows updates stay anonymous and final-only.

**Tech Stack:** .NET 10/WPF, Qt 6/C++, GSR C/Meson, Python AppImage packaging, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-11-12-managed-gsr-design.md` and `docs/ROADMAP.md`.

## Global constraints

- GSR 6.1.7, commit `1d4946a16139b446e6308f741e9ba99f2ed5b3a7`; preserve upstream notices and source.
- No GSR UI, root Qt process, silent permanent capabilities, or host-recorder fallback.
- Startup registration and replay-on-launch are independent, opt-in settings.
- Preserve existing dirty work and the remote README-only commit.
- User authorized branch updates and installer upload. Keep the repository private and the release a draft.

## Review focus

- Startup uses persistent AppImage paths, not temporary mounts.
- Polkit helper remains accessible with correct private dependencies and no host fallback.
- A conflicting host recorder cannot override the managed engine.
- Source provenance and licenses match the distributed engine.
- Existing settings, recordings and unrelated files survive upgrades.

## Task 1: Windows shared basics

- [x] Inspect `Autostart`, config, startup and clip actions; reuse existing functionality.
- [x] Write a failing regression for independent replay-on-launch settings; implement default-off startup policy and localized checkbox.
- [x] Verify containing-folder action between playback and deletion; run startup/config/UI regressions and Windows build.

## Task 2: Linux shared basics

- [x] Add failing Qt tests for independent startup settings, persistent launch command and containing-folder action.
- [x] Implement XDG autostart registration, separate replay-on-launch policy and library folder action with existing Qt conventions.
- [x] Run desktop, settings and UI tests; build the native application.

## Task 3: Managed GSR

- [x] Import the pinned source into `linux/third_party/gpu-screen-recorder/`; record provenance and source manifest.
- [x] Write failing tests for absent/conflicting host GSR, helper pairing, launch failures, dependency isolation and source verification.
- [x] Build the recorder/helper and overlay with the pinned headers; select managed binaries for source and AppImage launches.
- [x] Package engine/helper dependencies and notices, handling Polkit mount/environment restrictions; update workflow and guides.
- [x] Build real engine/AppImage and run packaging/offline suites; exercise live capture where the session permits it.

## Task 4: Review and distribute

- [x] Update identical shared roadmaps, striking only tasks complete on both platforms and recording remaining verification limits.
- [x] Review diffs and source/license inputs; run updater QA, context checks, secret scans and appropriate native suites.
- [x] Commit completed Windows changes, reconcile the remote README commit and push; build version 0.3.0 via pinned Windows Actions with public updates enabled, prerelease=false, publish=false.
- [ ] Verify hosted installer/uninstaller/package checks; download release assets and validate SHA-256, record candidate evidence.
- [ ] Commit/push verified Linux work and synchronized documentation. Do not claim live GPU/public-update coverage without evidence.

## Decisions

- Execute without a further approval round: the user explicitly approved the design and requested implementation/upload.
- Version 0.3.0 is a compatible feature increment under the repository version rules.
- Windows screenshots/hardware showcase need an actual Windows GPU session. The upload remains a validation draft; disclose missing evidence instead of reusing old screenshots as new.

## Verification and review ledger

- Windows startup/recovery tests: RED → GREEN, 36 config cases pass. Independent
  review found effective startup state was not persisted for recovery; fixed and
  verified by disk roundtrip. Native Windows Build workflow 43275192665 passed.
- Managed GSR forced-stop review found an orphaned recorder after shell SIGKILL.
  Added parent-death ownership to recorder/helper, tested with a compiled stubborn
  recorder; Qt owns and removes staged helper files. Packaging14 and Qt108 pass.
- Linux startup review found failed-save cached settings were inconsistent with
  external rollback. Added a failing write fixture and restored previous values;
  UI51 and desktop command-execution tests pass.
- Ruling: Keep source defaults aligned at0.3.0; previously built validation
  AppImage already used explicit SHADOWLESS_VERSION=0.3.0.
- Ruling: Test generated autostart commands in isolated config directories; do
  not modify the developer's real session config or log them out. Real login
  behavior remains an explicit validation limit.
- Ruling: Do not authenticate Polkit interactively on the user's behalf. Actual
  bundled helper loader/capability checks pass; KMS recording remains unverified.

- Source verification: all190 vendored files are tracked and staged bytes match
  the pinned SHA-256 manifest. Upstream whitespace is preserved deliberately;
  diff checks pass outside the imported upstream tree.
- Local AppImage0.3.0 SHA-256: `4cc5ce9fc7625bc4caeb3154f84a7245253b31f8c353c3981916198948a00339`.
  This is local validation evidence, not an uploaded Linux release.
