[CmdletBinding()]
param(
    [ValidateSet(
        "overview",
        "capture",
        "audio",
        "player",
        "ui",
        "distribution",
        "release")]
    [string]$Area = "overview",

    [ValidateRange(1, 100)]
    [int]$MaxStatusEntries = 20,

    [ValidateRange(20, 500)]
    [int]$MaxDocumentLines = 160,

    [switch]$NoDocumentText
)

$ErrorActionPreference = "Stop"
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

$areas = @{
    overview = @{
        Docs = @(
            "docs/agent/INDEX.md",
            "docs/agent/CURRENT.md",
            "docs/agent/ARCHITECTURE.md",
            "docs/agent/KNOWN_ISSUES.md"
        )
        Paths = @(
            "src/Shadowless/App.xaml.cs",
            "src/Shadowless/Config.cs",
            "src/Shadowless/ObsReplayEngine.cs",
            "src/Shadowless/SettingsWindow.xaml.cs",
            "src/Shadowless/ClipEditorWindow.xaml.cs"
        )
        Tests = @(
            ".\tools\TestAgentContext.ps1",
            "dotnet build .\Shadowless.sln -c Release --no-restore"
        )
    }
    capture = @{
        Docs = @(
            "docs/agent/CAPTURE.md",
            "docs/agent/decisions/0001-app-owns-capture-lifecycle.md"
        )
        Paths = @(
            "src/Shadowless/App.xaml.cs",
            "src/Shadowless/Config.cs",
            "src/Shadowless/ObsReplayEngine.cs",
            "src/Shadowless/ObsNative.cs",
            "src/Shadowless/Interop/CaptureInterop.cs",
            "src/Shadowless/ReplayPaths.cs",
            "native/ObsBridge/"
        )
        Tests = @(
            ".\tools\TestCapturePlayerRegression.ps1",
            ".\tools\TestIssue41Fixes.ps1",
            ".\tools\TestFullscreenOverlayHotkeys.ps1",
            "dotnet build .\Shadowless.sln -c Release --no-restore"
        )
    }
    audio = @{
        Docs = @(
            "docs/agent/AUDIO.md",
            "docs/agent/decisions/0002-advanced-audio-is-optional.md"
        )
        Paths = @(
            "src/Shadowless/AudioDevices.cs",
            "src/Shadowless/AudioRoutingCapabilities.cs",
            "src/Shadowless/ProcessAudioMonitor.cs",
            "src/Shadowless/ProcessAudioReconciler.cs",
            "src/Shadowless/ProcessAudioRoutingWindow.xaml",
            "src/Shadowless/ProcessAudioRoutingWindow.xaml.cs",
            "src/Shadowless/ProcessAudioSessionMonitor.cs",
            "src/Shadowless/ProcessIconProvider.cs",
            "src/Shadowless/Interop/ProcessTopology.cs",
            "native/ProcessAudio/"
        )
        Tests = @(
            ".\tools\TestIssue41Fixes.ps1",
            ".\tools\TestIssue41Followup.ps1",
            "dotnet build .\Shadowless.sln -c Release --no-restore"
        )
    }
    player = @{
        Docs = @("docs/agent/PLAYER.md")
        Paths = @(
            "src/Shadowless/ClipEditorWindow.xaml",
            "src/Shadowless/ClipEditorWindow.xaml.cs",
            "src/Shadowless/MpvHost.cs",
            "src/Shadowless/ReplayLibrary.cs",
            "src/Shadowless/FfmpegAdapter.cs"
        )
        Tests = @(
            ".\tools\TestCapturePlayerRegression.ps1",
            ".\tools\TestNextFeatureRegression.ps1",
            "dotnet build .\Shadowless.sln -c Release --no-restore"
        )
    }
    ui = @{
        Docs = @("docs/agent/UI.md")
        Paths = @(
            "src/Shadowless/SettingsWindow.xaml",
            "src/Shadowless/SettingsWindow.xaml.cs",
            "src/Shadowless/OverlayNotificationWindow.xaml",
            "src/Shadowless/OverlayNotificationWindow.xaml.cs",
            "src/Shadowless/ReplayStatusIndicatorWindow.xaml",
            "src/Shadowless/ReplayStatusIndicatorWindow.xaml.cs",
            "src/Shadowless/DisplayIdentifierWindow.xaml",
            "src/Shadowless/DisplayIdentifierWindow.xaml.cs",
            "src/Shadowless/Localization.cs",
            "src/Shadowless/Languages/",
            "src/Shadowless/Themes/",
            "src/Shadowless/ThemeManager.cs",
            "src/Shadowless/HotkeyManager.cs"
        )
        Tests = @(
            ".\tools\TestReportedUiRuntimeRegressions.ps1",
            ".\tools\TestStartupUiSurfaces.ps1",
            ".\tools\TestLocalizedLayout.ps1",
            ".\tools\TestNextFeatureRegression.ps1",
            "dotnet build .\Shadowless.sln -c Release --no-restore"
        )
    }
    distribution = @{
        Docs = @(
            "docs/agent/DISTRIBUTION.md",
            "docs/agent/decisions/0003-release-channels-are-independent.md"
        )
        Paths = @(
            "src/Shadowless/Distribution/AppDataPaths.cs",
            "src/Shadowless/Distribution/AppDistribution.cs",
            "src/Shadowless/Autostart.cs",
            "src/Shadowless/Diagnostics/DiagnosticLogExporter.cs",
            "src/Shadowless/Distribution/StorePackageLifecycle.cs",
            "src/Shadowless/Distribution/UpdateService.cs",
            "packaging/",
            "store-listing/",
            ".github/workflows/release.yml",
            ".github/workflows/store-release.yml"
        )
        Tests = @(
            ".\tools\TestStoreLifecycleIsolation.ps1",
            ".\tools\TestStoreFfmpegIsolation.ps1",
            ".\tools\TestStoreNativeDependencies.ps1",
            ".\tools\TestStoreIconAssets.ps1",
            ".\tools\TestStoreReleaseWorkflow.ps1"
        )
    }
    release = @{
        Docs = @(
            "docs/agent/WORKFLOWS.md",
            "docs/agent/decisions/0003-release-channels-are-independent.md"
        )
        Paths = @(
            "CHANGELOG.md",
            "README.md",
            "docs/RELEASING.md",
            "docs/RELEASE_NOTES.md",
            "docs/SCREENSHOTS.md",
            "docs/MICROSOFT-STORE-RELEASES.md",
            "packaging/",
            "store-listing/",
            ".github/workflows/release.yml",
            ".github/workflows/store-release.yml",
            "tools/BuildRelease.ps1",
            "tools/BuildStorePackage.ps1",
            "tools/CaptureReadmeScreenshots.ps1",
            "tools/New-StoreListingScreenshots.ps1",
            "tools/SyncStoreListing.ps1"
        )
        Tests = @(
            ".\tools\TestStoreReleaseWorkflow.ps1",
            ".\tools\TestStoreIconAssets.ps1",
            ".\tools\SyncStoreListing.ps1 -Version <version> -ValidateOnly"
        )
    }
}

function Invoke-Git([string[]]$Arguments) {
    $output = @(& git -C $repoRoot @Arguments 2>$null)
    if ($LASTEXITCODE -ne 0) {
        throw "Git command failed: git $($Arguments -join ' ')"
    }
    return $output
}

function Normalize-RepoPath([string]$Path) {
    return $Path.Trim().Trim('"').Replace('\', '/')
}

function Get-StatusPath([string]$Line) {
    if ($Line.Length -le 3) {
        return ""
    }
    $path = $Line.Substring(3).Trim()
    if ($path.Contains(" -> ")) {
        $path = ($path -split " -> ", 2)[1]
    }
    return Normalize-RepoPath $path
}

function Test-RelevantPath([string]$Path, [object[]]$Prefixes) {
    foreach ($prefixValue in $Prefixes) {
        $prefix = Normalize-RepoPath ([string]$prefixValue)
        if ($prefix.EndsWith("/")) {
            if ($Path.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
                return $true
            }
        }
        elseif ($Path.Equals($prefix, [StringComparison]::OrdinalIgnoreCase)) {
            return $true
        }
    }
    return $false
}

$selection = $areas[$Area]
foreach ($relativePath in @($selection.Docs) + @($selection.Paths)) {
    $candidate = Join-Path $repoRoot ([string]$relativePath).TrimEnd('/', '\')
    if (-not (Test-Path -LiteralPath $candidate)) {
        throw "Context route points to a missing path: $relativePath"
    }
}
[xml]$project = Get-Content -LiteralPath (
    Join-Path $repoRoot "src/Shadowless/Shadowless.csproj")
$version = [string](
    $project.Project.PropertyGroup.Version | Select-Object -First 1)
$branch = (
    Invoke-Git -Arguments @("branch", "--show-current") |
        Select-Object -First 1)
$head = (
    Invoke-Git -Arguments @("rev-parse", "--short", "HEAD") |
        Select-Object -First 1)
$status = @(
    Invoke-Git -Arguments @("status", "--short", "--untracked-files=all"))
$relevantPrefixes = @($selection.Paths) + @($selection.Docs) + @(
    "AGENTS.md",
    "docs/agent/INDEX.md",
    "tools/GetAgentContext.ps1",
    "tools/TestAgentContext.ps1"
)
$relevantStatus = @(
    $status | Where-Object {
        $Area -eq "overview" -or
        (Test-RelevantPath (Get-StatusPath $_) $relevantPrefixes)
    }
)

Write-Output "# Shadowless task context: $Area"
Write-Output ""
Write-Output "## Live state"
Write-Output ""
Write-Output "- Repository: ``$repoRoot``"
Write-Output "- Branch: ``$branch``"
Write-Output "- HEAD: ``$head``"
Write-Output "- Project version: ``$version``"
Write-Output "- Working tree entries: $($status.Count) total; $($relevantStatus.Count) relevant"

if ($relevantStatus.Count -gt 0) {
    Write-Output ""
    Write-Output "Relevant status (capped at $MaxStatusEntries):"
    Write-Output ""
    Write-Output '```text'
    $relevantStatus | Select-Object -First $MaxStatusEntries | Write-Output
    if ($relevantStatus.Count -gt $MaxStatusEntries) {
        Write-Output "... $($relevantStatus.Count - $MaxStatusEntries) more relevant entries"
    }
    Write-Output '```'
}

Write-Output ""
Write-Output "## Read first"
Write-Output ""
Write-Output "- ``AGENTS.md``"
foreach ($doc in $selection.Docs) {
    Write-Output "- ``$doc``"
}

if (-not $NoDocumentText) {
    Write-Output ""
    Write-Output "## Focused documentation"
    $remainingLines = $MaxDocumentLines
    foreach ($doc in $selection.Docs) {
        if ($remainingLines -le 0) {
            break
        }
        $fullPath = Join-Path $repoRoot $doc
        if (-not (Test-Path -LiteralPath $fullPath)) {
            throw "Context document is missing: $doc"
        }
        $lines = @(Get-Content -LiteralPath $fullPath)
        $take = [Math]::Min($remainingLines, $lines.Count)
        Write-Output ""
        Write-Output "### $doc"
        Write-Output ""
        $lines | Select-Object -First $take | Write-Output
        if ($take -lt $lines.Count) {
            Write-Output ""
            Write-Output "[truncated: $($lines.Count - $take) lines]"
        }
        $remainingLines -= $take
    }
}

Write-Output ""
Write-Output "## Relevant paths"
Write-Output ""
foreach ($path in $selection.Paths) {
    Write-Output "- ``$path``"
}

Write-Output ""
Write-Output "## Validation"
Write-Output ""
Write-Output '```powershell'
$selection.Tests | Write-Output
Write-Output '```'

Write-Output ""
Write-Output "## New task seed"
Write-Output ""
Write-Output "Work in this repository. Area: $Area. Read AGENTS.md and only the"
Write-Output "documents listed above. Preserve unrelated dirty-worktree changes. Goal: <task>."
Write-Output "Run listed validation and report completed, partial, and unverified results."
