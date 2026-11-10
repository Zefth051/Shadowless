[CmdletBinding()]
param(
    [string]$LauncherPath = ""
)

$ErrorActionPreference = "Stop"
$testParent = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\artifacts\test"))
$testRoot = Join-Path $testParent ("Shadowless-layout-" + [Guid]::NewGuid())
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
try {
    foreach ($name in @("Shadowless.exe", "obs.dll", "libmpv-2.dll", "ShadowlessObsBridge.dll",
        "OBS_RUNTIME_VERSION", "ExportDiagnostics.cmd", "README.md", "LICENSE",
        "THIRD_PARTY_NOTICES.md", "LOCAL_DIAGNOSTICS.md")) {
        Set-Content -LiteralPath (Join-Path $testRoot $name) -Value $name
    }
    foreach ($directory in @("data\libobs", "obs-plugins\64bit", "ffmpeg")) {
        New-Item -ItemType Directory -Path (Join-Path $testRoot $directory) | Out-Null
    }
    $fakeLauncher = Join-Path $testRoot "ExportDiagnostics.cmd"
    Set-Content -LiteralPath $fakeLauncher -Value "launcher"
    Set-Content -LiteralPath (Join-Path $testRoot "data\libobs\default.effect") -Value "effect"
    & (Join-Path $PSScriptRoot "SetPortableLayout.ps1") -PublishDirectory $testRoot -LauncherPath $fakeLauncher
    $actualRoot = @(Get-ChildItem -LiteralPath $testRoot | Select-Object -ExpandProperty Name | Sort-Object)
    if (($actualRoot -join ",") -ne "docs,ExportDiagnostics.cmd,runtime,Shadowless.exe") {
        throw "Unexpected Portable root: $actualRoot"
    }
    foreach ($required in @("runtime\Shadowless.exe", "runtime\obs.dll", "runtime\libmpv-2.dll",
        "runtime\ShadowlessObsBridge.dll", "runtime\OBS_RUNTIME_VERSION",
        "runtime\data\libobs\default.effect", "runtime\obs-plugins\64bit",
        "runtime\ffmpeg", "docs\LICENSE", "docs\README.md",
        "docs\THIRD_PARTY_NOTICES.md", "docs\LOCAL_DIAGNOSTICS.md")) {
        if (-not (Test-Path -LiteralPath (Join-Path $testRoot $required))) {
            throw "Missing organized file: $required"
        }
    }
    $rejected = $false
    try {
        & (Join-Path $PSScriptRoot "SetPortableLayout.ps1") -PublishDirectory $testRoot -LauncherPath $fakeLauncher
    } catch {
        $rejected = $true
    }
    if (-not $rejected) { throw "Repeated organization must be rejected." }
    if ($LauncherPath) {
        $launcher = [IO.Path]::GetFullPath($LauncherPath)
        $probeRoot = Join-Path $testRoot "launcher probe with spaces"
        $probeRuntime = Join-Path $probeRoot "runtime"
        dotnet build (Join-Path $PSScriptRoot "PortableRuntimeQa") -c Release -o $probeRuntime
        if ($LASTEXITCODE -ne 0) { throw "Launcher probe build failed." }
        Copy-Item -LiteralPath (Join-Path $probeRuntime "PortableRuntimeQa.exe") -Destination (Join-Path $probeRuntime "Shadowless.exe")
        Copy-Item -LiteralPath $launcher -Destination (Join-Path $probeRoot "Shadowless.exe")
        $resultPath = Join-Path $probeRoot "result.json"
        $process = Start-Process -FilePath (Join-Path $probeRoot "Shadowless.exe") `
            -ArgumentList ('--launcher-probe "text with spaces and ' + [char]0x00F1 + '" "" "' + $resultPath + '"') `
            -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(15000)) {
            $process.Kill($true)
            throw "Portable launcher did not finish."
        }
        $process.Refresh()
        if ($process.ExitCode -ne 17) { throw "Launcher did not forward exit code 17." }
        $result = Get-Content -LiteralPath $resultPath -Raw | ConvertFrom-Json
        if ($result.Arguments.Count -ne 4 -or
            $result.Arguments[1] -cne ('text with spaces and ' + [char]0x00F1) -or
            $result.Arguments[2] -cne "" -or
            $result.WorkingDirectory -ine $probeRuntime -or
            $result.ProcessPath -ine (Join-Path $probeRuntime "Shadowless.exe")) {
            throw "Launcher changed arguments, process path or working directory."
        }
        Write-Host "PASS: launcher forwards Unicode, spaces, empty arguments, working directory and exit code."
    }
    Write-Host "PASS: Portable layout preserves runtime resources and licenses with a clean root."
} finally {
    $resolved = [IO.Path]::GetFullPath($testRoot)
    if ($resolved.StartsWith($testParent + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($resolved).StartsWith("Shadowless-layout-")) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
