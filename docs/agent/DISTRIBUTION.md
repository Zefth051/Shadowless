# Distribution area

## Channels

Portable ZIP and Setup share ordinary desktop behavior. This fork disables
official release checks, downloads, and self-update installation in `UpdateService`.
The version button is informational; updates must be installed manually.
Microsoft Store uses MSIX identity, package-managed state, Store updates, and a
separate Partner Center release workflow.

## State and lifetime

- `AppDistribution` detects package channel.
- `AppDataPaths` selects roaming/local paths for ordinary builds and
  `ApplicationData` paths for Store builds.
- `StorePackageLifecycle` responds to package update/removal lifetime events.
- `Autostart` uses the channel-appropriate mechanism.
- `UpdateService.SelfUpdateEnabled` is false in this fork, including forced checks.
- `DiagnosticLogExporter` sanitizes local diagnostic context before a bug form
  is opened; logs and recordings are never attached automatically.
- Portable builds include `ExportDiagnostics.cmd` for a sanitized local ZIP.
  `Log` retains five files of approximately 2 MiB; `CrashDiagnostics` records
  managed failures. See `docs/LOCAL_DIAGNOSTICS.md` for coverage and privacy limits.

## Packaging

- `BuildRelease.ps1` keeps `Shadowless.exe` and `ExportDiagnostics.cmd` at the
  Portable root, groups dependencies (including OBS) in `runtime/`, and moves
  documentation and licenses to `docs/` using `SetPortableLayout.ps1`.
  The root executable is `native/PortableLauncher`; it forwards arguments and
  exit codes to `runtime/Shadowless.exe`. OBS helpers stay beside the real process.
  Test deployments use this payload; development/Store retain their flat layout.
- `tools/BuildRelease.ps1`: Portable/Setup artifacts.
- `tools/BuildStorePackage.ps1`: upload-ready MSIX archive.
- `packaging/msix/AppxManifest.xml.template`: Store identity and capabilities.
- `store-listing/`: localized Store text and images.
- `.github/workflows/release.yml`: GitHub release.
- `.github/workflows/store-release.yml`: Store package/listing submission.

## Invariants

- Never mix Store package files into a GitHub Release.
- Never hard-code a Store Package Family Name in product logic or docs.
- Store internal identity has four fields and ends in `.0`.
- Package-root native dependency collisions fail validation.
- External publishing requires explicit user authorization.

## Validation

Run `./tools/TestPortableLayout.ps1` for the Portable folder contract. Add
`-LauncherPath <built-launcher-exe>` to also probe argument forwarding, working
directory, and exit codes through `tools/PortableRuntimeQa` without opening UI.

Run `dotnet run --project tools/ForkUpdateQa -c Release` to verify fork update
checks and installation entry points stay blocked.

Run `TestStoreLifecycleIsolation.ps1`, `TestStoreFfmpegIsolation.ps1`,
`TestStoreNativeDependencies.ps1`, `TestStoreIconAssets.ps1`, and
`TestStoreReleaseWorkflow.ps1`, then follow the channel-specific release doc.
