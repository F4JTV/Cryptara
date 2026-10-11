; ============================================================================
;  CRYPTARA - Inno Setup script (bilingual English / French)
;
;  Saved as UTF-8 with a byte-order mark: without it Inno Setup may read the
;  file in the system code page, and the French accents come out garbled.
;  CRLF line endings, as every Windows file of the project.
;
;  Built by build_all.bat, which stages everything in dist\ first:
;      ISCC.exe /DAppVersion=x.y.z CRYPTARA.iss
;
;  The version is passed by build_all.bat (/DAppVersion=x.y.z), which reads
;  it from CMakeLists.txt. The value below only serves when this file is
;  compiled by hand, and is then wrong: go through build_all.bat.
;
;  Upgrading the Python version: same AppId as its installer (which left it
;  to the default, the application name), same folder, administrator
;  install like it.  The PyInstaller files it left (_internal) are removed;
;  the settings in %APPDATA%\CRYPTARA are kept and read as they are.
; ============================================================================

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#define AppName "CRYPTARA"
#define AppPublisher "CRYPTARA Community"
#define AppExeName "CRYPTARA.exe"

[Setup]
AppId={#AppName}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription=CRYPTARA - Secure VARA chat client - setup
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName} {#AppVersion}
OutputDir=output
OutputBaseFilename={#AppName}-{#AppVersion}-setup
SetupIconFile=..\assets\cryptara.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; 64-bit only.
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; Program Files, as the Python version's installer did.
PrivilegesRequired=admin
LicenseFile=dist\LICENSE.txt
CloseApplications=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "french";  MessagesFile: "compiler:Languages\French.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[InstallDelete]
; What the PyInstaller build of the Python version left behind.
Type: filesandordirs; Name: "{app}\_internal"

[Files]
; Everything windeployqt and build_all.bat gathered: the program and the Qt
; runtime with its QML modules.
Source: "dist\*"; DestDir: "{app}"; Excludes: "vc_redist.x64.exe"; Flags: ignoreversion recursesubdirs createallsubdirs
; The Visual C++ runtime, unpacked to a temporary folder, run, then deleted.
#ifexist "dist\vc_redist.x64.exe"
Source: "dist\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
#endif

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
; A runtime already present, of the same or a newer version, makes it return
; at once with a non-zero code, which Inno Setup ignores.
#ifexist "dist\vc_redist.x64.exe"
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/install /quiet /norestart"; \
    StatusMsg: "{cm:InstallingRuntime}"; Flags: waituntilterminated
#endif
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[CustomMessages]
english.InstallingRuntime=Installing the Microsoft Visual C++ runtime...
french.InstallingRuntime=Installation du runtime Microsoft Visual C++...
