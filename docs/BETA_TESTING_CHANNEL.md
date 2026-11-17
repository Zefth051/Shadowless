# Beta testing update channel (future requirement, not implemented)

Status: **planned for a future release. Nothing in this document is implemented yet.**
Follows `docs/PRODUCT_PHILOSOPHY.md`: normal users stay on stable releases without doing anything.

## Current behavior (must not change until this feature ships)

- Releases published as **Stable** (not draft, not prerelease) are the update channel. Experimental builds are
  published as prereleases or drafts and must never reach normal users automatically.
- Windows (`src/Shadowless/Distribution/UpdateService.cs`): considers only non-draft, non-prerelease releases whose tag is
  `vX.Y.Z`; Linux tags (`linux-vX.Y.Z`) never match.
- Linux (`linux/app/update_service.cpp`): considers only `linux-vX.Y.Z` tags and the matching AppImage with a valid
  digest. Prereleases are ignored on every version (fixed in 0.3.8 development; earlier builds accepted them on `0.x`).
  Prereleases must be considered only when Beta testing is enabled.

## Requirement

Settings → Updates gets an option **Beta testing**, **off by default**, on both Windows and Linux.

| Beta testing | Considered releases |
| --- | --- |
| Off | Stable releases only. Prereleases and drafts are ignored. |
| On | Stable releases and compatible prereleases. |

Rules:

1. Pick the newest candidate by **version comparison** (semantic/numeric, never by publish date or by string).
2. **Never downgrade automatically.** Only a candidate strictly newer than the installed version is offered.
3. Turning Beta testing off at any time keeps the installed version and all user data; it only stops offering
   prereleases. No forced downgrade, no data migration. A user on a beta build moves to the next stable that is newer
   than the installed version.
4. Each platform keeps its own tags, assets and channel: Windows `vX.Y.Z` (Setup/Portable + `SHA256SUMS.txt`), Linux
   `linux-vX.Y.Z` (AppImage + digest). A Linux install never receives a Windows build and vice versa.
5. The preference is stored in the existing settings, defaults to off, and is never asked about on first run.
6. Prerelease identifiers need a defined ordering relative to the same stable number (for example `0.4.0-beta.1` <
   `0.4.0`); decide the tag format before implementing and keep the existing release tag patterns valid.
7. The setting does not change the Microsoft Store channel, which stays managed by the Store.

## Acceptance tests (when implemented)

- Off: a newer prerelease is ignored; a newer stable is offered.
- On: the newest of stable and prerelease wins by version; an equal or older candidate is never offered.
- Toggling off after installing a prerelease: no downgrade prompt, data intact.
- Cross-platform: Linux ignores Windows tags and Windows ignores Linux tags in both modes.
- Default after upgrading from a build without the option: off.
