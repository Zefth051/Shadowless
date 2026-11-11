[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

function Read-RepoText([string]$relativePath) {
    return [IO.File]::ReadAllText((Join-Path $repoRoot $relativePath))
}

function Require([string]$text, [string]$pattern, [string]$message) {
    if ($text -notmatch $pattern) {
        throw $message
    }
}

function Reject([string]$text, [string]$pattern, [string]$message) {
    if ($text -match $pattern) {
        throw $message
    }
}

$globalJson = Read-RepoText "global.json"
$project = Read-RepoText "src\Shadowless\Shadowless.csproj"
$lockFile = Read-RepoText "src\Shadowless\packages.lock.json"
$runtimeAcquisition = Read-RepoText "tools\AcquireObsRuntime.ps1"
$sdkAcquisition = Read-RepoText "tools\AcquireObsPluginSdk.ps1"
$mpvAcquisition = Read-RepoText "tools\AcquireMpvRuntime.ps1"
$ffmpegAcquisition = Read-RepoText "tools\AcquireFfmpegRuntime.ps1"
$engine = Read-RepoText "src\Shadowless\ObsReplayEngine.cs"
$releaseWorkflow = Read-RepoText ".github\workflows\release.yml"
$releaseScript = Read-RepoText "tools\BuildRelease.ps1"

Require $globalJson '"version"\s*:\s*"10\.0\.401"' `
    ".NET SDK must stay on the validated 10.0.401 feature band."
Require $project '<TargetFramework>net10\.0-windows10\.0\.22621\.0</TargetFramework>' `
    "Shadowless must target .NET 10 WPF."
Require $project 'H\.NotifyIcon\.Wpf" Version="2\.4\.1"' `
    "Shadowless must use H.NotifyIcon.Wpf 2.4.1."
Reject $project 'PackageReference Include="System\.Drawing\.Common"' `
    "System.Drawing.Common is supplied by .NET 10 WindowsDesktop; an explicit reference causes NU1510."
Reject $project 'Endpne\.LibMPV\.Windows' `
    "Shadowless must use the pinned mpv runtime instead of the stale NuGet binary."
Require $project '<MpvRuntimeRoot>[\s\S]*?runtime\\mpv</MpvRuntimeRoot>' `
    "Shadowless must copy libmpv from the pinned runtime directory."
Require $lockFile '"net10\.0-windows10\.0\.22621"' `
    "NuGet lock file must target .NET 10."
Require $lockFile '"H\.NotifyIcon\.Wpf"[\s\S]*?"resolved"\s*:\s*"2\.4\.1"' `
    "NuGet lock file must resolve H.NotifyIcon.Wpf 2.4.1."

foreach ($text in @($runtimeAcquisition, $sdkAcquisition)) {
    Require $text '\$version\s*=\s*"32\.2\.2"' `
        "OBS runtime and SDK acquisition must stay aligned on 32.2.2."
}
Require $runtimeAcquisition `
    '4d6e40e3ab155f56b30de517380566a206d74b63cdf5ad49aa596924768f97e1' `
    "OBS 32.2.2 Windows archive hash is not pinned."
Require $engine 'RequiredObsVersion\s*=\s*"32\.2\.2"' `
    "Runtime OBS version guard must require 32.2.2."
Require $mpvAcquisition '\$version\s*=\s*"git-bcde6b93cc"' `
    "mpv runtime must stay on the SHA-256-pinned 2026-11-11 build."
Require $mpvAcquisition `
    '273c079b11ea5575822f972756f4c9a71149a5c10130353abeb8586738938452' `
    "mpv runtime archive hash is not pinned."
Require $ffmpegAcquisition '\$version\s*=\s*"n9\.0\.2-49-gc179411bbb"' `
    "FFmpeg runtime must stay on the SHA-256-pinned 9.0.2 build."
Require $ffmpegAcquisition `
    '4ac2a4a732ee66707ab2a4d7fc135d085e1da691f365cdb2f1b48f8f3816d4d8' `
    "FFmpeg shared runtime archive hash is not pinned."

Require $releaseWorkflow 'is-7_1_0/innosetup-7\.1\.0-x64\.exe' `
    "Release workflow must install Inno Setup 7.1.0."
Require $releaseWorkflow `
    '0362a383ed217d4c4239b5933866dd96d3eb2102737da92f80f6057a4b40df2f' `
    "Inno Setup 7.1.0 installer hash is not pinned."
Require $releaseScript '\[Version\]"7\.1\.0"' `
    "Local release builds must reject older Inno Setup compilers."

Write-Host "DEPENDENCY_VERSIONS_TEST PASS"
