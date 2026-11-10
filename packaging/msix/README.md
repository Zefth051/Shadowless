# Microsoft Store package

This package channel is separate from Shadowless's GitHub Installer and Portable
releases. Microsoft Store installs and updates this build. Shadowless's GitHub
self-updater is disabled at compile time and guarded again at runtime whenever
Windows reports package identity.

## Package identity

Build-only packages default to `Zefth051.Shadowless` and `CN=Shadowless`.
For Partner Center, supply the reserved identity with
`SHADOWLESS_STORE_PACKAGE_NAME` and `SHADOWLESS_STORE_PUBLISHER`.

## Build

From repository root:

```powershell
.\tools\BuildStorePackage.ps1 -Version 0.2.0
```

Output:

```text
artifacts\store\0.2.0\Shadowless-0.2.0.0-x64.msix
artifacts\store\0.2.0\Shadowless-0.2.0.0-x64.msixupload
artifacts\store\0.2.0\SHA256SUMS.txt
```

Upload `.msixupload` on Partner Center's **Packages** page. Do not publish this
artifact as a GitHub Release. Microsoft Store signs the accepted package for
distribution.

The package is unsigned. Sideloading requires a trusted test certificate. Store customers receive Microsoft's signed package after
certification.

Store packages use verified self-contained FFmpeg command-line tools. This
prevents packaged-process DLL resolution from mixing FFmpeg with libobs media
libraries that use the same file names. Package creation fails if duplicate
libraries return or either command-line tool cannot start.

## Versioning

MSIX requires four numeric components. Shadowless `0.2.0` becomes package version
`0.2.0.0`. Every Store submission must use a strictly higher package version.
Never reuse a version already submitted to Partner Center.

## Store-specific behavior

- Updates are delivered only by Microsoft Store.
- Footer shows `Microsoft Store` instead of an update action.
- Startup uses the package `windows.startupTask` extension.
- Installer and Portable builds continue using GitHub Releases and HKCU Run.

## Before submission

1. Build package from clean `main` commit.
2. Check `SHA256SUMS.txt` and inspect generated manifest identity.
3. Test install, first launch, tray, capture, replay save, clip preview and trim,
   microphone permission, startup toggle, uninstall, and Store update from the
   previous package version.
4. Complete Partner Center privacy, age rating, properties, screenshots, and
   certification notes.
