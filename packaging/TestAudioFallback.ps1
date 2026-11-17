<#
Shadowless 0.3.4 test helper (Windows 10 audio fallback).
Usage (PowerShell, Shadowless must be closed):
  .\TestAudioFallback.ps1 -Mode Advanced   # saves Advanced audio routing, the setup that broke 0.3.3
  .\TestAudioFallback.ps1 -Mode Separate   # saves Separate audio tracks
  .\TestAudioFallback.ps1 -Restore         # puts your original settings back
It only edits two audio options in your own config.json, after making a backup copy
(config.json.rc-backup). Nothing is uploaded and nothing else is changed.
#>
param(
    [ValidateSet('Advanced', 'Separate')][string]$Mode,
    [switch]$Restore
)
$ErrorActionPreference = 'Stop'
$config = Join-Path $env:APPDATA 'Shadowless\config.json'
$backup = "$config.rc-backup"
if (Get-Process -Name Shadowless -ErrorAction SilentlyContinue) {
    Write-Host 'Please close Shadowless first (tray icon > Exit), then run this again.' -ForegroundColor Yellow
    exit 1
}
if ($Restore) {
    if (-not (Test-Path $backup)) { Write-Host 'No backup found; nothing to restore.'; exit 0 }
    Copy-Item $backup $config -Force
    Remove-Item $backup
    Write-Host 'Your original settings were restored.' -ForegroundColor Green
    exit 0
}
if (-not $Mode) { Write-Host 'Choose -Mode Advanced or -Mode Separate (or -Restore).'; exit 1 }
if (-not (Test-Path $config)) { Write-Host 'Start Shadowless once first so it creates its settings, close it, then try again.'; exit 1 }
if (-not (Test-Path $backup)) { Copy-Item $config $backup }
$json = Get-Content $config -Raw | ConvertFrom-Json
if ($Mode -eq 'Advanced') {
    $json.AudioRoutingMode = 'advanced'
    $json.SeparateAudioTracks = $false
} else {
    $json.AudioRoutingMode = 'simple'
    $json.SeparateAudioTracks = $true
}
$json.CaptureSystemAudio = $true
$json | ConvertTo-Json -Depth 20 | Set-Content $config -Encoding UTF8
Write-Host "Saved '$Mode' audio. Now start Shadowless 0.3.4, turn on Instant Replay and press the save hotkey." -ForegroundColor Green
