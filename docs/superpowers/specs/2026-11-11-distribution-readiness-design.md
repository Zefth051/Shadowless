# Distribution readiness

Approved direction: one initially private Shadowless repository, with `windows`
and `linux` branches. Preserve all existing uncommitted work. Do not push,
publish, change visibility, or bump versions during this preparation.

## Approach

Reuse the Windows Inno Setup/Portable pipeline and existing checksum-verifying
updater. Keep updates disabled in private builds; provide an explicit public
release build switch. Do not embed GitHub credentials or add a private update
service. Linux updates remain out of scope.

An authenticated private updater was considered but adds credentials and access
management that this release does not need. A separate repository per platform
would duplicate release administration and is not selected.

## Deliverables

- Animate the Linux notification lifetime bar, resetting it on replacement and
  stopping it when hidden; keep passive input behavior and existing timeout.
- Bound Linux library sorting/cache metadata to the newest 20 clips per collection
  while retaining the newest 20 globally and every collection filter.
- Group Windows distribution and diagnostic source files without changing their
  namespaces; update QA links and tooling paths together.
- Add repeatable tracked/current/history secret checks, ignore local secrets and
  generated output, and fail CI on recognized credentials.
- Package the native Linux executable and overlay plugin as an AppImage with
  explicit external GSR/host requirements, notices, checksums and a smoke test.
- Document private staging, platform-specific release tags, install/download
  links, public update activation, Captail attribution, and license/source duties.

## Validation and limits

Use regression tests before behavior changes, run Linux offline suites, and run
Windows portable-layout/update QA with the existing local SDK/PowerShell where
possible. Native Windows installation requires a Windows runner. AppImage
compatibility must be described by the tested base system, never assumed across
all distributions. Scan all reachable Git history before recommending a push.
The notices audit is technical provenance work, not legal certification.
