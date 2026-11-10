[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Require([string]$path, [string]$pattern, [string]$message) {
    if ((Get-Content -LiteralPath (Join-Path $root $path) -Raw) -notmatch $pattern) {
        throw $message
    }
}

Require 'src/Shadowless/Config.cs' 'GpuPreference\s*\{\s*get;\s*set;\s*\}\s*=\s*"high-performance"' 'Default capture GPU must prefer discrete graphics.'
Require 'src/Shadowless/Config.cs' 'GpuPreference = source.GpuPreference;' 'Copying settings must preserve the GPU preference.'
Require 'src/Shadowless/Config.cs' 'string.Equals\(GpuPreference, other.GpuPreference, StringComparison.Ordinal\)' 'Changing GPU must restart the capture pipeline.'
Require 'src/Shadowless/ObsReplayEngine.cs' 'GpuPreference.AdapterIndex\(_config.GpuPreference\)' 'The capture device must use the configured GPU preference.'
Require 'native/ObsBridge/ShadowlessObsBridge.cpp' 'get_preferred_adapter_index\(DXGI_GPU_PREFERENCE_MINIMUM_POWER\)' 'Integrated GPU selection must use the minimum-power DXGI preference.'
Require 'native/ObsBridge/ShadowlessObsBridge.cpp' 'get_preferred_adapter_index\(DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE\)' 'Default GPU selection must keep the high-performance DXGI preference.'
Require 'src/Shadowless/GpuPreference.cs' 'preference == "power-saving"[\s\S]{0,100}shadowless_get_power_saving_adapter_index\(' 'Managed GPU selection must reach the integrated GPU export.'
Require 'src/Shadowless/SettingsWindow.xaml.cs' 'SelectByTag\(GpuPreferenceBox, _config.GpuPreference\)' 'Settings must restore the selected GPU.'
Require 'src/Shadowless/SettingsWindow.xaml.cs' 'string.Equals\(left.GpuPreference, right.GpuPreference, StringComparison.Ordinal\)' 'GPU edits must participate in unsaved-settings detection.'
$settings = Get-Content (Join-Path $root 'src/Shadowless/SettingsWindow.xaml.cs') -Raw
if ([regex]::Matches($settings, 'candidate.GpuPreference = GetSelectedTag\(GpuPreferenceBox, "high-performance"\);').Count -ne 2) {
    throw 'Both staged settings and applying settings must read the selected GPU.'
}
Write-Host 'GPU_SELECTION PASS'
