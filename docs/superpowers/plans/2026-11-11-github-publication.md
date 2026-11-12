# GitHub Publication Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Publish reviewed platform sources and prepare releases with in-app updates.
**Architecture:** Reuse Windows Setup/update code; Qt Network discovers Linux releases and QSaveFile replaces verified AppImages atomically after confirmation.
**Tech Stack:** GitHub CLI/Actions, C++17/Qt 6, .NET 10/Inno Setup, Python.
**Spec:** `docs/superpowers/specs/2026-11-11-github-publication-design.md`

## Global Constraints

- Preserve pending source work, GPL attribution and upstream history.
- English repository text; disclose AI development without claiming upstream was AI-created.
- No tokens in applications; private builds disable updates.
- Verify SHA-256 before atomically replacing an AppImage, apply next launch.
- Do not publish unreviewed bundled binaries as license-cleared.

## Review Focus

- Ignore Windows tags, drafts, older versions and unrelated asset URLs.
- Reject malformed/missing digests and oversized responses/downloads.
- Preserve the old executable on checksum failure or unwritable destination.
- Never restart/interfere with an ongoing recording to apply an update.
- Keep credentials, archives, runtime downloads and recordings out of Git pushes.

### Task 1: Linux updater

**Files:** `linux/app/update_service.{h,cpp}`, app/controller/build project files,
`linux/tests/updates/`, `linux/packaging/build_appimage.py`.

- [x] Add Qt tests for release selection, URL trust, digest validation and atomic replacement; run before implementation.
- [x] Implement release discovery/download with bounded asynchronous networking and QSaveFile installation.
- [x] Add tray update action, startup checks for public builds, version/build configuration and packaging verification.
- [x] Run `./linux/build.sh` and Python packaging tests.

### Task 2: Publication documentation and review

**Files:** platform READMEs, `docs/FEATURES.md`, `docs/AI_DEVELOPMENT.md`, notices/audit and release workflows.

- [x] Document platform features, AI development and exact remaining redistribution requirements.
- [x] Run secret scanner tests/history scans, Windows update QA, workflow parsing and diff checks.
- [x] Commit reviewed files separately on each branch, create repository, push only both platform refs and verify remote SHAs.
- [x] Run release builds where feasible; report any actual build/license blocker.
