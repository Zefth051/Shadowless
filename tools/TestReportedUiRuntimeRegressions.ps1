[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))

function Read-Text([string]$relativePath) {
    [IO.File]::ReadAllText((Join-Path $repoRoot $relativePath))
}

function Require([string]$text, [string]$pattern, [string]$message) {
    if ($text -notmatch $pattern) {
        throw $message
    }
}

$settings = Read-Text "src\Shadowless\SettingsWindow.xaml.cs"
$app = Read-Text "src\Shadowless\App.xaml.cs"
$engine = Read-Text "src\Shadowless\ObsReplayEngine.cs"
$launcher = Read-Text "src\Shadowless\ExternalLinkLauncher.cs"
$player = Read-Text "src\Shadowless\MpvHost.cs"
$editor = Read-Text "src\Shadowless\ClipEditorWindow.xaml.cs"
$editorXaml = Read-Text "src\Shadowless\ClipEditorWindow.xaml"

Require $settings 'L\.Status\.RecordingDisabled' `
    "Recording mode needs its own inactive status title."
Require $settings 'L\.Status\.RecordingIdle' `
    "Recording mode needs its own inactive status detail."
Require $app 'L\.Notify\.RecordingOffGameTitle' `
    "Game-start warning must describe disabled Recording mode."

Require $engine '_recordingGameExecutable' `
    "Continuous recording must preserve detected game identity for save routing."
Require $engine 'IsContinuousRecording[\s\S]{0,220}IsAutomaticCapture' `
    "Continuous Desktop recording must keep automatic game detection enabled."

Require $launcher 'Windows\.System\.Launcher\.LaunchUriAsync' `
    "Packaged and portable builds must use Windows URI launcher."
Require $launcher 'ShellExecuteW' `
    "External links need native shell activation in MSIX builds."
Require $settings 'AboutPopupPanel\.IsAncestorOf' `
    "About popup buttons must receive clicks before outside-click dismissal."

Require $player 'NativeMouseWheel' `
    "Embedded native video host must forward mouse-wheel input."
Require $player 'await StopAsync\(' `
    "Replacing a loaded clip must drain old end-file event before loading next clip."
Require $editor 'PreviewPlayer\.NativeMouseWheel' `
    "Replay player must consume wheel events forwarded by native video host."

Require $editorXaml 'x:Name="SidebarColumn"' `
    "Fullscreen layout needs direct control over recent-clips column."
Require $editor 'SidebarColumn\.Width\s*=\s*new GridLength\(0\)' `
    "Fullscreen must remove recent-clips column from layout."
Require $editor 'Grid\.SetColumnSpan\(EditorWorkspace, 2\)' `
    "Fullscreen video must span complete window width."
Require $editor 'ResizeMode\s*=\s*ResizeMode\.NoResize' `
    "Fullscreen must remove WPF resize frame."
Require $editor 'SwpFrameChanged' `
    "Fullscreen must notify Windows after changing native frame style."

Require $app 'await SaveReplayGuardedAsync\(engine\);\s*PlayReplaySavedSound\(\);' `
    "Replay confirmation sound must play only after a successful save."
Require $app 'Replay saved sound failed:' `
    "Sound playback failure must be logged separately from clip saving."
$soundPath = Join-Path $repoRoot "src\Shadowless\Assets\ReplaySaved.wav"
$sound = [IO.File]::ReadAllBytes($soundPath)
if ([Text.Encoding]::ASCII.GetString($sound, 0, 4) -ne 'RIFF' -or
    [Text.Encoding]::ASCII.GetString($sound, 8, 4) -ne 'WAVE' -or
    [BitConverter]::ToInt16($sound, 20) -ne 1 -or
    [BitConverter]::ToInt32($sound, 40) -ne $sound.Length - 44) {
    throw "Replay saved sound must be a valid PCM WAV asset."
}

[xml]$settingsLayout = Read-Text "src\Shadowless\SettingsWindow.xaml"
$namespaces = [Xml.XmlNamespaceManager]::new($settingsLayout.NameTable)
$namespaces.AddNamespace('x', 'http://schemas.microsoft.com/winfx/2006/xaml')
$fpsOptions = $settingsLayout.SelectSingleNode('//*[@x:Name="FpsOptions"]', $namespaces)
foreach ($rate in @(30, 60, 120, 144, 240)) {
    $button = $fpsOptions.SelectSingleNode("*[@Tag='$rate']")
    $expectedRow = if ($rate -ge 120) { '2' } else { '' }
    if ($null -eq $button -or $button.GetAttribute('Grid.Row') -ne $expectedRow) {
        throw "Frame rate $rate must remain selectable in its standard or experimental row."
    }
}

Get-ChildItem (Join-Path $repoRoot "src\Shadowless\Languages") `
    -Filter "Strings.*.xaml" |
    ForEach-Object {
        $language = [IO.File]::ReadAllText($_.FullName)
        Require $language 'x:Key="L\.Video\.ExperimentalFrameRate"' `
            "$($_.Name) is missing the experimental frame rate warning."
        Require $language 'x:Key="L\.Status\.RecordingDisabled"' `
            "$($_.Name) is missing Recording inactive title."
        Require $language 'x:Key="L\.Status\.RecordingIdle"' `
            "$($_.Name) is missing Recording inactive detail."
        Require $language 'x:Key="L\.Notify\.RecordingOffGameTitle"' `
            "$($_.Name) is missing Recording game-start warning."
        Require $language 'x:Key="L\.Notify\.RecordingOffGameDetail"' `
            "$($_.Name) is missing Recording game-start warning detail."
    }

Write-Host "REPORTED_UI_RUNTIME_REGRESSIONS PASS"
