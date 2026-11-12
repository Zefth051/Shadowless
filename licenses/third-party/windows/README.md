# Verified Windows notice texts

Unmodified upstream copyright/license texts collected for the Windows 0.3.0
preflight. `SOURCES.json` records their exact source URLs and SHA-256 hashes.
The .NET version 10.0.15 is identified in the actual draft executable's embedded
runtime configuration. NAudio 2.3.0 and H.NotifyIcon 2.4.1 source commits are
recorded by the exact package nuspec metadata. System.Drawing.Common and
Microsoft.Win32.SystemEvents notice files come from their 10.0.0 NuGet packages.

`BuildRelease.ps1` already copies this directory recursively into new packages.
These additions do not modify the existing v0.3.0 draft binaries. This collection
is incomplete for the native OBS, FFmpeg and libmpv dependency closures; see
`docs/RELEASE_PREFLIGHT.md`. It does not clear the distribution gate.
