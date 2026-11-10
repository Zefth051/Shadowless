[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PublishDirectory,

    [Parameter(Mandatory)]
    [string]$LauncherPath
)

$ErrorActionPreference = "Stop"
$root = [IO.Path]::GetFullPath($PublishDirectory)
if (-not (Test-Path -LiteralPath $LauncherPath -PathType Leaf)) {
    throw "Portable launcher not found: $LauncherPath"
}
foreach ($required in @("Shadowless.exe", "obs.dll", "libmpv-2.dll")) {
    if (-not (Test-Path -LiteralPath (Join-Path $root $required) -PathType Leaf)) {
        throw "Published file not found: $required"
    }
}
foreach ($directory in @("runtime", "docs")) {
    if (Test-Path -LiteralPath (Join-Path $root $directory)) {
        throw "Portable layout destination already exists: $directory"
    }
}

$items = @(Get-ChildItem -LiteralPath $root)
$runtimeRoot = New-Item -ItemType Directory -Path (Join-Path $root "runtime")
$docsRoot = New-Item -ItemType Directory -Path (Join-Path $root "docs")
$rootPrefix = $root.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
foreach ($item in $items) {
    if ($item.Name -eq "ExportDiagnostics.cmd") {
        continue
    }
    $destination = if (-not $item.PSIsContainer -and
        ($item.Extension -in @(".md", ".txt") -or $item.Name -eq "LICENSE")) {
        $docsRoot.FullName
    } else {
        $runtimeRoot.FullName
    }
    if (-not $item.FullName.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase) -or
        -not $destination.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Portable layout paths must stay inside the publish directory."
    }
    Move-Item -LiteralPath $item.FullName -Destination $destination
}
Copy-Item -LiteralPath $LauncherPath -Destination (Join-Path $root "Shadowless.exe")
