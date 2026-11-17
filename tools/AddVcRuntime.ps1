<#
Copies the Visual C++ runtime DLLs that the bundled OBS components import into the runtime
directory (app-local deployment). A Windows PC without the Visual C++ Redistributable can then
load OBS without a separate install and without administrator rights.

Source: the x64 CRT redistributable folder of the Visual Studio installation on the build machine
(VC\Redist\MSVC\<version>\x64\Microsoft.VC*.CRT). Microsoft permits app-local deployment of these
DLLs (see redist.txt in that Visual Studio installation).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RuntimeDirectory
)

$ErrorActionPreference = "Stop"
$required = @("vcruntime140.dll", "vcruntime140_1.dll", "msvcp140.dll")

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe not found; Visual Studio is required to locate the Visual C++ runtime."
}
$installation = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
if (-not $installation) {
    throw "No Visual Studio installation with the C++ x64 tools was found."
}

$redistRoot = Join-Path $installation "VC\Redist\MSVC"
$crtDirectory = Get-ChildItem -LiteralPath $redistRoot -Directory |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+' } |
    Sort-Object { [version]($_.Name -replace '^(\d+\.\d+\.\d+).*', '$1') } -Descending |
    ForEach-Object { Get-ChildItem -LiteralPath (Join-Path $_.FullName "x64") -Directory -Filter "Microsoft.VC*.CRT" -ErrorAction SilentlyContinue } |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "vcruntime140.dll") } |
    Select-Object -First 1
if (-not $crtDirectory) {
    throw "x64 Visual C++ CRT redistributable folder not found under $redistRoot."
}

New-Item -ItemType Directory -Force -Path $RuntimeDirectory | Out-Null
foreach ($name in $required) {
    $source = Join-Path $crtDirectory.FullName $name
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Required Visual C++ runtime file not found: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $RuntimeDirectory $name) -Force
}
$version = (Get-Item -LiteralPath (Join-Path $RuntimeDirectory "vcruntime140.dll")).VersionInfo.ProductVersion
Write-Host "Bundled Visual C++ runtime $version from $($crtDirectory.Name)."
