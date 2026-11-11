[CmdletBinding()]
param(
    [string]$Destination = "",

    [ValidateSet("Shared", "Static")]
    [string]$Flavor = "Shared"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
$version = "n9.0.2-49-gc179411bbb"
$releaseTag = "autobuild-2026-11-11-12-39"
$isStatic = $Flavor -eq "Static"
$archiveName = if ($isStatic) {
    "ffmpeg-$version-win64-lgpl-9.0.zip"
}
else {
    "ffmpeg-$version-win64-lgpl-shared-9.0.zip"
}
$expectedArchiveSha256 = if ($isStatic) {
    "9ccd65428fb0e60a0d574b76145df41f402b754edd2d06c2e24fa506fc1c2b02"
}
else {
    "4ac2a4a732ee66707ab2a4d7fc135d085e1da691f365cdb2f1b48f8f3816d4d8"
}
$url =
    "https://github.com/BtbN/FFmpeg-Builds/releases/download/" +
    "$releaseTag/$archiveName"
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$allowedRuntimeRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot "runtime"))

function Get-Sha256Hex([string]$Path) {
    $algorithm = [Security.Cryptography.SHA256]::Create()
    $stream = $null
    try {
        $stream = [IO.File]::OpenRead($Path)
        return ([BitConverter]::ToString(
            $algorithm.ComputeHash($stream))).Replace("-", "")
    }
    finally {
        if ($null -ne $stream) {
            $stream.Dispose()
        }
        $algorithm.Dispose()
    }
}

if (-not $Destination) {
    $runtimeName = if ($isStatic) { "ffmpeg-static" } else { "ffmpeg" }
    $Destination = Join-Path $allowedRuntimeRoot $runtimeName
}

$Destination = [IO.Path]::GetFullPath($Destination)
$allowedPrefix = $allowedRuntimeRoot.TrimEnd(
    [IO.Path]::DirectorySeparatorChar,
    [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if (-not $Destination.StartsWith(
        $allowedPrefix,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "FFmpeg runtime destination must stay under $allowedRuntimeRoot"
}

$archive = Join-Path $env:TEMP $archiveName
$extract = Join-Path $env:TEMP "Shadowless-FFmpeg-$PID-$([Guid]::NewGuid().ToString('N'))"
try {
    if (Test-Path -LiteralPath $archive) {
        $existingHash = Get-Sha256Hex $archive
        if (-not $existingHash.Equals(
                $expectedArchiveSha256,
                [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $archive -Force
        }
    }
    if (-not (Test-Path -LiteralPath $archive)) {
        Write-Host "Downloading FFmpeg $version runtime..."
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $archive
    }
    $actualHash = Get-Sha256Hex $archive
    if (-not $actualHash.Equals(
            $expectedArchiveSha256,
            [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $archive -Force
        throw "FFmpeg archive SHA-256 mismatch. Expected $expectedArchiveSha256; found $actualHash."
    }

    Expand-Archive -LiteralPath $archive -DestinationPath $extract
    $ffmpeg = Get-ChildItem -LiteralPath $extract -Filter ffmpeg.exe -Recurse |
        Select-Object -First 1
    if (-not $ffmpeg) {
        throw "ffmpeg.exe not found in FFmpeg archive."
    }
    $binRoot = $ffmpeg.Directory.FullName
    if (-not (Test-Path -LiteralPath (Join-Path $binRoot "ffprobe.exe"))) {
        throw "ffprobe.exe not found in FFmpeg archive."
    }
    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -LiteralPath (Join-Path $binRoot "ffmpeg.exe") -Destination $Destination
    Copy-Item -LiteralPath (Join-Path $binRoot "ffprobe.exe") -Destination $Destination
    Copy-Item -LiteralPath (Join-Path $ffmpeg.Directory.Parent.FullName "LICENSE.txt") `
        -Destination (Join-Path $Destination "LICENSE.FFmpeg.txt")
    Get-ChildItem -LiteralPath $binRoot -File -Filter *.dll |
        Copy-Item -Destination $Destination

    Set-Content -LiteralPath (Join-Path $Destination "VERSION") `
        -Value $version -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $Destination "FLAVOR") `
        -Value $Flavor.ToLowerInvariant() -Encoding ascii
    Set-Content -LiteralPath (Join-Path $Destination "SOURCE_URL") `
        -Value $url -Encoding UTF8
    Set-Content -LiteralPath (Join-Path $Destination "SOURCE_SHA256") `
        -Value $expectedArchiveSha256 -Encoding ascii
}
finally {
    if (Test-Path -LiteralPath $extract) {
        $tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd(
            [IO.Path]::DirectorySeparatorChar,
            [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
        $resolvedExtract = [IO.Path]::GetFullPath($extract)
        if ($resolvedExtract.StartsWith(
                $tempRoot,
                [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $resolvedExtract -Recurse -Force
        }
    }
}

Write-Host "FFmpeg runtime $version ($Flavor) ready: $Destination"
