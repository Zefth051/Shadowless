#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\artifacts\release\0.1.0\staging\Shadowless-0.1.0"
#endif
#ifndef OutputDir
  #define OutputDir "..\artifacts\release\0.1.0"
#endif

#define MyAppName "Shadowless"
#define MyAppPublisher "FaulMit"
#define MyAppURL "https://github.com/Zefth051/Shadowless"
#define MyAppExeName "Shadowless.exe"

[Setup]
AppId={{1D598E51-7024-4A68-B5D0-483E3AD0C0FC}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription={#MyAppName} Installer
VersionInfoProductName={#MyAppName}
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
OutputDir={#OutputDir}
OutputBaseFilename=Shadowless-{#MyAppVersion}-Setup-win-x64
SetupIconFile=..\src\Shadowless\Assets\Shadowless.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
Uninstallable=yes
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
ChangesAssociations=no
ChangesEnvironment=no
AllowNoIcons=yes
UsedUserAreasWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startup"; Description: "Start Shadowless with Windows"; GroupDescription: "Windows integration:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Shadowless"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\Shadowless"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "Shadowless"; ValueData: """{app}\{#MyAppExeName}"" --background"; Flags: uninsdeletevalue; Tasks: startup

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\Shadowless"
Type: filesandordirs; Name: "{userappdata}\Shadowless"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch Shadowless"; Flags: nowait postinstall skipifsilent

[Code]
function StopShadowless(): Boolean;
var
  ResultCode: Integer;
  InstalledExe: String;
begin
  Result := True;
  InstalledExe := ExpandConstant('{app}\{#MyAppExeName}');
  if FileExists(InstalledExe) then
  begin
    if not Exec(InstalledExe, '--shutdown-existing', '', SW_HIDE,
      ewWaitUntilTerminated, ResultCode) then
      Result := False
    else if ResultCode <> 0 then
      Result := False;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not StopShadowless() then
    Result := 'Shadowless is still busy. Wait for replay saving to finish, then retry.';
end;

function InitializeUninstall(): Boolean;
begin
  Result := StopShadowless();
  if not Result then
    MsgBox(
      'Shadowless could not stop safely. Close any active save operation and retry.',
      mbError,
      MB_OK);
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    RegDeleteValue(HKCU,
      'Software\Microsoft\Windows\CurrentVersion\Run', 'Shadowless');
end;
