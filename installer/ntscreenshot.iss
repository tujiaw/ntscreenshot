#ifndef PackageDir
  #error PackageDir is required
#endif
#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef OutputDir
  #error OutputDir is required
#endif

[Setup]
; Keep this ID unchanged so upgrades replace the existing installation.
AppId={{51CD680B-9FF1-4AC2-ABA9-68E6A4B6BAF2}
AppName=ntscreenshot
AppVersion={#AppVersion}
AppVerName=ntscreenshot {#AppVersion}
AppPublisher=tujiaw
AppPublisherURL=https://github.com/tujiaw/ntscreenshot
AppSupportURL=https://github.com/tujiaw/ntscreenshot/issues
AppUpdatesURL=https://github.com/tujiaw/ntscreenshot/releases
DefaultDirName={localappdata}\Programs\ntscreenshot
DefaultGroupName=ntscreenshot
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
UsePreviousAppDir=yes
UsePreviousTasks=yes
UninstallDisplayName=ntscreenshot
UninstallDisplayIcon={app}\ntscreenshot.exe
SetupIconFile=..\src\resource\images\ntscreenshot.ico
LicenseFile={#PackageDir}\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=ntscreenshot-{#AppVersion}-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
ChangesAssociations=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "chinesesimplified"; MessagesFile: "Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#PackageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\ntscreenshot"; Filename: "{app}\ntscreenshot.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\ntscreenshot"; Filename: "{app}\ntscreenshot.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\ntscreenshot.exe"; Description: "{cm:LaunchProgram,ntscreenshot}"; Flags: nowait postinstall skipifsilent

; User settings, ClipboardLite/history.db, credentials and search indexes are
; stored outside {app}. Never add recursive user-data deletion on uninstall.
