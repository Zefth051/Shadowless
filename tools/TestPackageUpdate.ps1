[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OldPackages,
    [Parameter(Mandatory)][string]$NewPackages,
    [string]$OldVersion = '9.8.6',
    [string]$NewVersion = '9.8.7'
)
$ErrorActionPreference = 'Stop'
if (-not $IsWindows) { throw 'Native package update QA requires Windows.' }
$root = Join-Path $env:RUNNER_TEMP ('shadowless-update-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $root | Out-Null
$OldPackages = (Resolve-Path $OldPackages).Path
$NewPackages = (Resolve-Path $NewPackages).Path
$qa = Join-Path $root 'qa'
dotnet publish "$PSScriptRoot/PackageUpdateQa" -c Release -o $qa
if ($LASTEXITCODE -ne 0) { throw 'Package update QA build failed.' }
function Run-Installer([string]$Path, [string]$Target, [string]$Log) {
    $process = Start-Process -FilePath $Path -PassThru -ArgumentList @(
        '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/CURRENTUSER',
        "/DIR=`"$Target`"", "/LOG=`"$Log`"")
    if (-not $process.WaitForExit(180000)) { $process.Kill($true); throw 'Installer timed out.' }
    if ($process.ExitCode -ne 0) { throw "Installer failed: $($process.ExitCode), see $Log" }
}
function Assert-Version([string]$Target) {
    $actual = (Get-Item "$Target/Shadowless.exe").VersionInfo.ProductVersion
    if ($actual -notlike "$NewVersion*") { throw "Unexpected updated launcher version: $actual" }
    $expectedRuntime = Join-Path $NewPackages "staging/Shadowless-$NewVersion/runtime/Shadowless.exe"
    if ((Get-FileHash "$Target/runtime/Shadowless.exe").Hash -ne (Get-FileHash $expectedRuntime).Hash) {
        throw 'Updated runtime does not match the built package.'
    }
}
# Use an isolated Windows runner: installer registration and real user settings are exercised.
$settings = Join-Path $env:APPDATA 'Shadowless/config.json'
if (Test-Path $settings) { throw 'Refusing to replace existing runner settings.' }
New-Item -ItemType Directory -Force (Split-Path $settings) | Out-Null
$sentinel = '{"Language":"en","BufferSeconds":173,"AccentColor":"violet","ReplayEnabled":false}'
Set-Content -LiteralPath $settings -Value $sentinel -NoNewline
$settingsHash = (Get-FileHash $settings).Hash
$installed = Join-Path $root 'installed'
Run-Installer (Join-Path $OldPackages "Shadowless-$OldVersion-Setup-win-x64.exe") $installed "$root/install-old.log"
Copy-Item "$qa/*" "$installed/runtime" -Recurse -Force
dotnet "$installed/runtime/PackageUpdateQa.dll" $NewPackages $NewVersion "$root/installer.json" Installer
if ($LASTEXITCODE -ne 0) { throw 'Installed PrepareAsync failed.' }
$prepared = Get-Content "$root/installer.json" -Raw | ConvertFrom-Json
Run-Installer $prepared.PackagePath $installed "$root/install-new.log"
Assert-Version $installed
if ((Get-FileHash $settings).Hash -ne $settingsHash) { throw 'Setup upgrade changed settings.' }
Write-Host 'PASS: native Setup old-to-new upgrade and settings preservation.'

Expand-Archive (Join-Path $OldPackages "Shadowless-$OldVersion-Portable-win-x64.zip") "$root/portable"
$portable = Join-Path $root "portable/Shadowless-$OldVersion"
Copy-Item "$qa/*" "$portable/runtime" -Recurse -Force
dotnet "$portable/runtime/PackageUpdateQa.dll" $NewPackages $NewVersion "$root/portable.json" Portable $portable
if ($LASTEXITCODE -ne 0) { throw 'Portable PrepareAsync/apply failed.' }
Assert-Version $portable
# A process-start marker proves the native launcher ran; GUI/GPU health is outside this test.
$launchPid = [int](Get-Content "$root/portable.json.relaunch")
Write-Host "PASS: updated native launcher started (PID $launchPid)."
Start-Sleep -Seconds 3
Get-CimInstance Win32_Process | Where-Object {
    $_.ExecutablePath -and $_.ExecutablePath.StartsWith($portable, [StringComparison]::OrdinalIgnoreCase)
} | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
$config = Get-Content $settings -Raw | ConvertFrom-Json
if ($config.Language -ne 'en' -or $config.BufferSeconds -ne 173 -or
    $config.AccentColor -ne 'violet' -or $config.ReplayEnabled -ne $false) {
    throw 'Portable update/relaunch changed configured settings.'
}
Write-Host 'PASS: Portable old-to-new replacement, real launcher process start and settings preservation.'
Write-Host 'LIMIT: injected package transport; no published GitHub releases, UI interaction, GPU capture or SmartScreen validation.'
"QA evidence: $root" | Tee-Object -FilePath (Join-Path $NewPackages 'update-qa-result.txt')
