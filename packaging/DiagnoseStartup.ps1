<#
Shadowless startup diagnostic. Run from the Shadowless folder (Portable) or the
install folder (Setup): right-click > Run with PowerShell.
Writes Shadowless-startup-diagnostic.txt next to this script. The report contains
only Windows build, DLL names with Win32 error codes, and log lines that mention
startup failures, with user paths/names/IPs removed. Nothing is uploaded.
#>
$ErrorActionPreference = 'Continue'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$runtime = if (Test-Path (Join-Path $root 'runtime\obs.dll')) { Join-Path $root 'runtime' } else { $root }
$out = New-Object System.Collections.Generic.List[string]
function Add-Line($text) { $out.Add($text) | Out-Null }

Add-Type -Namespace Native -Name Loader -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("kernel32.dll", SetLastError=true, CharSet=System.Runtime.InteropServices.CharSet.Unicode)]
public static extern System.IntPtr LoadLibraryExW(string name, System.IntPtr file, uint flags);
[System.Runtime.InteropServices.DllImport("kernel32.dll")]
public static extern bool FreeLibrary(System.IntPtr module);
[System.Runtime.InteropServices.DllImport("kernel32.dll", CharSet=System.Runtime.InteropServices.CharSet.Unicode)]
public static extern bool SetDllDirectoryW(string path);
'@

$os = [Environment]::OSVersion.Version
Add-Line "Windows build: $($os.Major).$($os.Minor).$($os.Build)  64-bit OS: $([Environment]::Is64BitOperatingSystem)"
Add-Line "Shadowless runtime folder found: $(Test-Path (Join-Path $runtime 'obs.dll'))"

Add-Line ''
Add-Line '== System components required by the capture engine =='
foreach ($name in 'VCRUNTIME140.dll','VCRUNTIME140_1.dll','MSVCP140.dll','D3DCOMPILER_47.dll','d3d11.dll','dxgi.dll') {
    $handle = [Native.Loader]::LoadLibraryExW($name, [IntPtr]::Zero, 0)
    if ($handle -eq [IntPtr]::Zero) {
        Add-Line ("MISSING {0} (Win32 error {1})" -f $name, [Runtime.InteropServices.Marshal]::GetLastWin32Error())
    } else {
        Add-Line "ok      $name"
        [void][Native.Loader]::FreeLibrary($handle)
    }
}

Add-Line ''
Add-Line '== Loading the capture engine DLLs =='
[void][Native.Loader]::SetDllDirectoryW($runtime)
foreach ($name in 'obs.dll','libobs-d3d11.dll','ShadowlessObsBridge.dll') {
    $path = Join-Path $runtime $name
    if (-not (Test-Path $path)) { Add-Line "NOT FOUND $name"; continue }
    # LOAD_WITH_ALTERED_SEARCH_PATH (0x8) resolves dependencies from the DLL folder.
    $handle = [Native.Loader]::LoadLibraryExW($path, [IntPtr]::Zero, 0x8)
    if ($handle -eq [IntPtr]::Zero) {
        Add-Line ("FAILED  {0} (Win32 error {1}; 126 = a dependency DLL is missing)" -f $name, [Runtime.InteropServices.Marshal]::GetLastWin32Error())
    } else {
        Add-Line "ok      $name"
        [void][Native.Loader]::FreeLibrary($handle)
    }
}

Add-Line ''
Add-Line '== Recent startup failures from log.txt (sanitized) =='
$log = Join-Path $env:LOCALAPPDATA 'Shadowless\log.txt'
if (Test-Path $log) {
    $lines = Get-Content $log -Tail 400 | Where-Object { $_ -match 'startup failed|Watchdog|OBS pipeline|GPU capability|Exception|Unable to load|VideoFailed|initialize' } | Select-Object -Last 25
    foreach ($line in $lines) {
        $clean = $line -replace '[A-Za-z]:\\[^''"\r\n<>|:]*', '<path>' `
                       -replace '\b\d{1,3}(\.\d{1,3}){3}\b', '<ip>' `
                       -replace [regex]::Escape($env:USERNAME), '<user>' `
                       -replace [regex]::Escape($env:COMPUTERNAME), '<pc>'
        if ($clean.Length -gt 400) { $clean = $clean.Substring(0, 400) }
        Add-Line $clean
    }
    if (-not $lines) { Add-Line '(no matching lines)' }
} else {
    Add-Line '(log.txt not found - Shadowless has not run yet for this user)'
}

$report = Join-Path $root 'Shadowless-startup-diagnostic.txt'
$out | Set-Content -Path $report -Encoding UTF8
Write-Host "Report written to: $report"
Write-Host 'Review it, then send it to the developer.'
$out | ForEach-Object { Write-Host $_ }
