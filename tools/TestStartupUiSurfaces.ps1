[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

function Read-Text([string]$relativePath) {
    return [IO.File]::ReadAllText((Join-Path $repoRoot $relativePath))
}

function Assert-Contains(
    [string]$text,
    [string]$pattern,
    [string]$message
) {
    if ($text -notmatch $pattern) {
        throw $message
    }
}

$app = Read-Text "src\Shadowless\App.xaml.cs"
$indicator = Read-Text "src\Shadowless\ReplayStatusIndicatorWindow.xaml.cs"
$settings = Read-Text "src\Shadowless\SettingsWindow.xaml"
$settingsCode = Read-Text "src\Shadowless\SettingsWindow.xaml.cs"
$hotkeys = Read-Text "src\Shadowless\HotkeyManager.cs"

Assert-Contains $app `
    '_tray\.ForceCreate\(enablesEfficiencyMode:\s*false\)' `
    "Background recording must not enable H.NotifyIcon Efficiency Mode."
Assert-Contains $app `
    'SetCurrentProcessExplicitAppUserModelID' `
    "Portable builds need a stable shell identity after sign-in."
Assert-Contains $indicator `
    'ContentRendered\s*\+=' `
    "Recording indicator capture protection must wait for its first frame."
Assert-Contains $indicator `
    'if\s*\(!_firstFrameRendered\)\s*return;' `
    "Recording indicator must not apply capture affinity before rendering."
Assert-Contains $settings `
    'Icon="\{DynamicResource ApplicationIcon\}"' `
    "Main window must use the accent-aware application icon resource."
Assert-Contains $settings 'Topmost="True"' `
    "Control overlay must stay above desktop windows."
Assert-Contains $settings 'ShowInTaskbar="False"' `
    "Control overlay must not create a taskbar window."
Assert-Contains $settings 'StretchDirection="DownOnly"' `
    "Control overlay must fit smaller screens and higher display scales."
Assert-Contains $settingsCode 'MonitorFromWindow\(GetForegroundWindow\(\), 2\)' `
    "Control overlay must open on the foreground monitor."
Assert-Contains $settingsCode 'AddHook\(OverlayWindowProc\)' `
    "Control overlay must observe application activation, including while an owned editor is active."
Assert-Contains $settingsCode 'message == WmActivateApp && wParam == 0' `
    "Control overlay must dismiss when switching to another application."
Assert-Contains $settingsCode 'processId != 0 && processId != Environment.ProcessId' `
    "Activation transitions within Shadowless must not dismiss the overlay."
Assert-Contains $settingsCode 'editor.CloseOwnerOnExit = true;\s*editor.Close\(\);[\s\S]{0,100}else[\s\S]{0,50}Close\(\);' `
    "Alt+Tab must close the editor through its normal save path, or close the dashboard directly."
if ($settingsCode -match '_overlayTopmostTimer') {
    throw "Control overlay must not force itself above the application selected by Alt+Tab."
}
$appOverlays = Read-Text "src\Shadowless\App.Overlays.cs"
$captureEditor = Read-Text "src\Shadowless\CaptureOverlayEditorWindow.xaml.cs"
Assert-Contains $appOverlays 'if \(editor.CloseOwnerOnExit\)\s*_settingsWindow.Close\(\);\s*else\s*_settingsWindow.Activate\(\);' `
    "Closing the editor after Alt+Tab must close its owner without reactivating Shadowless."
Assert-Contains $captureEditor 'else\s*\{\s*CloseOwnerOnExit = false;\s*IsEnabled = true;' `
    "A failed save must cancel dismissal and keep the editor available."
if ($settingsCode -match 'SetWindowDisplayAffinity') {
    throw "The interactive control overlay must remain visible in desktop screen sharing and clips."
}
Assert-Contains $settingsCode 'else if \(e.Key == Key.Escape\)[\s\S]{0,80}Close\(\)' `
    "Escape must close the control dashboard."
Assert-Contains $app 'OverlayRequested \+= ToggleControlOverlay' `
    "Overlay shortcut must toggle the control surface."
Assert-Contains $hotkeys 'binding.Modifiers \| 0x4000' `
    "Overlay shortcut must suppress keyboard autorepeat."
Assert-Contains $hotkeys 'OverlayHotkeyAvailable[\s\S]{0,160}UnregisterHotKey' `
    "Overlay shortcut must release its registration when rebinding or disposing."
Assert-Contains $hotkeys 'private void Restore[\s\S]*?RegisterOverlayBinding\(\)' `
    "Failed capture-shortcut changes must restore the overlay shortcut."

Write-Host "STARTUP_UI_SURFACES_TEST PASS"
