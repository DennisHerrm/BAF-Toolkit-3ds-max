; ============================================================
;  BAF Toolkit for 3ds Max - Inno Setup script
;
;  Normally built by ERSTELLE_RELEASE.bat, which stages the
;  package, fetches the VC++ runtime and passes the version:
;
;    ISCC.exe /DAppVersion=1.8.3 /DPackageDir=..\dist\stage\BafToolkit BafToolkit.iss
;
;  Installs the Autodesk application package for ALL users into
;    C:\ProgramData\Autodesk\ApplicationPlugins\BafToolkit
;  - the location Autodesk documents for machine-wide plugins.
;  3ds Max 2016-2027 finds it there by itself. Writing there
;  needs administrator rights, so the installer asks for them.
;
;  Besides the package it only installs the Microsoft Visual C++
;  runtime, and only when the one on the machine is too old for
;  the plugin (built with MSVC 14.50 / Visual Studio 2026).
;
;  Code signing: pass /DSignToolName=<name> and configure a sign
;  tool of that name in the Inno Setup IDE (Tools -> Configure
;  Sign Tools), or with /S on the command line. Without it the
;  installer is built unsigned.
; ============================================================

#ifndef AppVersion
  #error Pass the version: ISCC /DAppVersion=x.y.z
#endif
#ifndef PackageDir
  #define PackageDir "..\dist\stage\BafToolkit"
#endif
#ifndef VCRedist
  #define VCRedist "..\dist\redist\vc_redist.x64.exe"
#endif

; Lowest VC++ runtime the plugin runs on: the toolset it was
; built with. MSVC 14.50 = Visual Studio 2026.
#define VCMinMajor 14
#define VCMinMinor 50

#define AppName      "BAF Toolkit for 3ds Max"
#define AppPublisher "DennisH"
#define AppURL       "https://github.com/DennisHerrm/BAF-Toolkit-3ds-max"

[Setup]
; AppId identifies the installation for upgrades and uninstall.
; It must NEVER change between versions.
AppId={{3B6E2F1D-9A47-4C85-B1E0-7D52C8A9F364}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
AppCopyright=Copyright (c) 2026 {#AppPublisher}

; Autodesk fixes the location - there is nothing to choose.
DefaultDirName={commonappdata}\Autodesk\ApplicationPlugins\BafToolkit
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
; The per-user cleanup below touches {userappdata} on purpose.
UsedUserAreasWarning=no

PrivilegesRequired=admin

; Keep the uninstaller out of the package root so the folder
; stays exactly what Autodesk expects.
UninstallFilesDir={app}\Uninstall
UninstallDisplayName={#AppName}
UninstallDisplayIcon={uninstallexe}

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; The current VC++ runtime supports Windows 10 and newer only.
MinVersion=10.0

LicenseFile=..\LICENSE
WizardStyle=modern
SetupLogging=yes
; Never offer to close 3ds Max for the user - unsaved work would
; be lost. The [Code] section asks them to close it instead.
CloseApplications=no
RestartApplications=no

OutputDir=..\dist
OutputBaseFilename=BafToolkit-{#AppVersion}-Setup
Compression=lzma2/max
SolidCompression=yes

; Shown under Properties -> Details of the setup file.
VersionInfoVersion={#AppVersion}
VersionInfoProductVersion={#AppVersion}
VersionInfoProductName={#AppName}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} Setup
VersionInfoCopyright=Copyright (c) 2026 {#AppPublisher}

#ifdef SignToolName
SignTool={#SignToolName}
SignedUninstaller=yes
#endif

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"

[Messages]
en.FinishedLabelNoIcons=Setup has finished installing [name] for all users.%n%nStart 3ds Max, then:%n    File > Import > Big Ape Actor (*.baf, *.lab)%n    File > Export > Big Ape Actor (*.baf)%n%nBoth windows are also listed under Customize > Customize User Interface > Category "DH Tools".
de.FinishedLabelNoIcons=[name] wurde fuer alle Benutzer installiert.%n%n3ds Max starten, dann:%n    Datei > Importieren > Big Ape Actor (*.baf, *.lab)%n    Datei > Exportieren > Big Ape Actor (*.baf)%n%nBeide Fenster stehen ausserdem unter Customize > Customize User Interface > Kategorie "DH Tools".

[CustomMessages]
en.MaxRunning=3ds Max is running.%n%nThe plugin file is locked while 3ds Max is open. Please save your work, close every 3ds Max window and click Retry.
de.MaxRunning=3ds Max laeuft noch.%n%nSolange 3ds Max offen ist, ist die Plugin-Datei gesperrt. Bitte die Arbeit speichern, alle 3ds-Max-Fenster schliessen und auf Wiederholen klicken.
en.InstallingRuntime=Installing the Microsoft Visual C++ runtime...
de.InstallingRuntime=Microsoft Visual C++ Laufzeit wird installiert...
en.RuntimeFailed=The Microsoft Visual C++ runtime could not be installed (code %1).%n%nThe plugin needs it. Install it from https://aka.ms/vc14/vc_redist.x64.exe and start 3ds Max afterwards.
de.RuntimeFailed=Die Microsoft Visual C++ Laufzeit konnte nicht installiert werden (Code %1).%n%nDas Plugin braucht sie. Bitte von https://aka.ms/vc14/vc_redist.x64.exe installieren und danach 3ds Max starten.

[InstallDelete]
; Files of an older version, so no stale plugin build stays behind.
Type: filesandordirs; Name: "{app}\Contents"
; Up to 1.3.1 the package was called BafImport. Max would load it
; next to BafToolkit, and both register the same interface ID -
; Max then aborts at startup. Remove it in both locations.
Type: filesandordirs; Name: "{userappdata}\Autodesk\ApplicationPlugins\BafImport"
Type: filesandordirs; Name: "{commonappdata}\Autodesk\ApplicationPlugins\BafImport"
; A per-user copy (INSTALLIERE.bat or an older ZIP install) would
; be a second package next to this one. Remove it as well.
Type: filesandordirs; Name: "{userappdata}\Autodesk\ApplicationPlugins\BafToolkit"

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#VCRedist}"; DestDir: "{tmp}"; Flags: deleteafterinstall; Check: NeedsVCRuntime; AfterInstall: InstallVCRuntime

[UninstallDelete]
Type: dirifempty; Name: "{app}"

[Code]
var
  RuntimeNeedsRestart: Boolean;

{ Is a process with this executable name running?
  WMI answers for processes of every user and session. If the
  query itself fails, assume "not running" - the file lock check
  below still catches a 3ds Max that holds the plugin. }
function IsProcessRunning(const ExeName: String): Boolean;
var
  Locator, Service, Found: Variant;
begin
  Result := False;
  try
    Locator := CreateOleObject('WbemScripting.SWbemLocator');
    Service := Locator.ConnectServer('.', 'root\CIMV2');
    Found := Service.ExecQuery(Format('SELECT ProcessId FROM Win32_Process WHERE Name = "%s"', [ExeName]));
    Result := Found.Count > 0;
  except
  end;
end;

{ Can the file be opened for writing? A DLU loaded by a running
  3ds Max cannot. A missing file is not locked. }
function IsFileLocked(const FileName: String): Boolean;
var
  Stream: TFileStream;
begin
  Result := False;
  if not FileExists(FileName) then
    Exit;
  try
    Stream := TFileStream.Create(FileName, fmOpenReadWrite or fmShareExclusive);
    Stream.Free;
  except
    Result := True;
  end;
end;

{ Any installed plugin file - this package, a per-user copy or
  the old BafImport package - still held by 3ds Max? }
function AnyPluginLocked(): Boolean;
var
  Year: Integer;
  Name: String;
begin
  Result := False;
  for Year := 2016 to 2027 do
  begin
    Name := '\Contents\' + IntToStr(Year) + '\';
    if IsFileLocked(ExpandConstant('{commonappdata}\Autodesk\ApplicationPlugins\BafToolkit') + Name + 'BafToolkit.dlu') or
       IsFileLocked(ExpandConstant('{userappdata}\Autodesk\ApplicationPlugins\BafToolkit') + Name + 'BafToolkit.dlu') or
       IsFileLocked(ExpandConstant('{userappdata}\Autodesk\ApplicationPlugins\BafImport') + Name + 'BafImport.dlu') or
       IsFileLocked(ExpandConstant('{commonappdata}\Autodesk\ApplicationPlugins\BafImport') + Name + 'BafImport.dlu') then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

{ Ask until 3ds Max is closed or the user gives up. }
function WaitForMaxClosed(): Boolean;
begin
  Result := True;
  while IsProcessRunning('3dsmax.exe') or AnyPluginLocked() do
  begin
    if SuppressibleMsgBox(CustomMessage('MaxRunning'), mbError, MB_RETRYCANCEL, IDCANCEL) = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
  end;
end;

{ Is the installed VC++ runtime older than the plugin's toolset?
  Microsoft documents this key as the way to detect the v14
  runtime; the x64 view is read explicitly. }
function NeedsVCRuntime(): Boolean;
var
  Installed, Major, Minor: Cardinal;
  Key: String;
begin
  Key := 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64';
  Result := True;
  if RegQueryDWordValue(HKLM64, Key, 'Installed', Installed) and (Installed = 1) and
     RegQueryDWordValue(HKLM64, Key, 'Major', Major) and
     RegQueryDWordValue(HKLM64, Key, 'Minor', Minor) then
    Result := (Major < {#VCMinMajor}) or ((Major = {#VCMinMajor}) and (Minor < {#VCMinMinor}));
end;

procedure InstallVCRuntime();
var
  ResultCode: Integer;
begin
  WizardForm.StatusLabel.Caption := CustomMessage('InstallingRuntime');
  if not Exec(ExpandConstant('{tmp}\vc_redist.x64.exe'), '/install /quiet /norestart', '',
              SW_SHOW, ewWaitUntilTerminated, ResultCode) then
    ResultCode := -1;
  { 0 = done, 1638 = a newer version is already there,
    3010 = done, a restart finishes it. }
  if ResultCode = 3010 then
    RuntimeNeedsRestart := True
  else if (ResultCode <> 0) and (ResultCode <> 1638) then
    SuppressibleMsgBox(FmtMessage(CustomMessage('RuntimeFailed'), [IntToStr(ResultCode)]), mbError, MB_OK, IDOK);
end;

function NeedRestart(): Boolean;
begin
  Result := RuntimeNeedsRestart;
end;

function InitializeSetup(): Boolean;
begin
  Result := WaitForMaxClosed();
end;

function InitializeUninstall(): Boolean;
begin
  Result := WaitForMaxClosed();
end;
