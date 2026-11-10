[CmdletBinding()]
param(
    [ValidateSet("unit", "replay", "recording", "ui")]
    [string]$Mode = "unit",
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$qaParent = Join-Path $repoRoot ".qa"
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $qaParent "capture-overlays" }
$qaRoot = [IO.Path]::GetFullPath($OutputDirectory)
if (-not $qaRoot.StartsWith($qaParent + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Overlay QA output must stay under $qaParent"
}
New-Item -ItemType Directory -Force -Path $qaRoot | Out-Null

dotnet build (Join-Path $PSScriptRoot "CaptureOverlayQa\CaptureOverlayQa.csproj") `
    -c Release -p:NuGetAudit=false --nologo
if ($LASTEXITCODE -ne 0) { throw "Overlay QA build failed." }
$framework = "net10.0-windows10.0.22621.0\win-x64"
$appBin = Join-Path $repoRoot "src\Shadowless\bin\Release\$framework"
$qaBin = Join-Path $PSScriptRoot "CaptureOverlayQa\bin\Release\$framework"
# Use a native apphost beside OBS so its helper executables and DLLs resolve.
Copy-Item -Path (Join-Path $qaBin "CaptureOverlayQa.*") -Destination $appBin -Force
$qaExe = Join-Path $appBin "CaptureOverlayQa.exe"

if ($Mode -ne "unit") {
    $ffmpeg = Join-Path $appBin "ffmpeg\ffmpeg.exe"
    & $ffmpeg -hide_banner -loglevel error -f lavfi -i "color=c=red:s=320x180" `
        -frames:v 1 -update 1 -y (Join-Path $qaRoot "overlay.png")
    if ($LASTEXITCODE -ne 0) { throw "Image fixture generation failed." }
    & $ffmpeg -hide_banner -loglevel error -f lavfi -i "testsrc2=s=320x180:r=30" `
        -t 2 -y (Join-Path $qaRoot "overlay.gif")
    if ($LASTEXITCODE -ne 0) { throw "GIF fixture generation failed." }
    & $ffmpeg -hide_banner -loglevel error -f lavfi -i "testsrc2=s=320x180:r=30" `
        -f lavfi -i "sine=frequency=1000" -t 2 -c:v mpeg4 -pix_fmt yuv420p `
        -c:a aac -y (Join-Path $qaRoot "overlay.mp4")
    if ($LASTEXITCODE -ne 0) { throw "Video fixture generation failed." }
}

& $qaExe "--$Mode" $qaRoot
if ($LASTEXITCODE -ne 0) { throw "Overlay QA $Mode failed." }
if ($Mode -in @("unit", "ui")) { return }

$clips = if ($Mode -eq "replay") {
    @("baseline", "image", "gif", "video", "disabled") | ForEach-Object {
        Get-Item -LiteralPath (Join-Path $qaRoot "$_.mp4")
    }
} else {
    Get-ChildItem -LiteralPath $qaRoot -Filter "Recording_*.mp4" |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
}
foreach ($clip in $clips) {
    & $ffmpeg -v error -xerror -i $clip.FullName -f null -
    if ($LASTEXITCODE -ne 0) { throw "Full decode failed: $($clip.Name)" }
    & $ffmpeg -v error -xerror -ss 1 -i $clip.FullName -frames:v 1 -f null -
    if ($LASTEXITCODE -ne 0) { throw "Seek decode failed: $($clip.Name)" }
}
Write-Host "CAPTURE_OVERLAY_CLIP_DECODE PASS ($Mode)"
