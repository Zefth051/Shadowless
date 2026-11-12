# Private preview validation plan

**Goal:** provide real Linux screenshots and reviewable Windows update evidence,
then update both GitHub branches while keeping the repository private.

**Scope:** current Linux Qt/Hyprland app and AppImage, Windows production updater
and installer, existing release/status documentation. No embedded credentials,
visibility changes, silent-update redesign or replacement of existing assets.

- [x] Capture the real Linux app using isolated settings and safe demo media;
  review each PNG, record source/build hashes and states, and add it to README.
- [x] Verify the existing Linux draft AppImage checksum and startup; retain its
  private-preview limitations and direct authorized reviewers to the draft.
- [x] Exercise Windows package preparation and native Setup/Portable upgrades
  through CI, including checksum failure and state preservation. Clearly separate
  controlled transport/package tests from real public GitHub update validation.
- [x] Update release status and explain private/public update behavior and the
  local-source → branch → workflow → reviewed release process.
- [x] Review changes, scan for sensitive information, commit intended files,
  push both platform branches and verify remote checks and private visibility.

The friend can download private previews through an authorized GitHub account;
the current application does not authenticate to private repositories. Existing
updates require confirmation. Public release/visibility remains the user's final
decision after review.

Evidence: Windows native run `43160914401` and ordinary Windows build
`43161028134` passed. The full published-release/UI update remains pending;
see `docs/WINDOWS_UPDATE_VALIDATION.md` for the exact limits.
