; ============================================================================
; Script Inno Setup pour CRYPTARA (build PyInstaller ONEDIR)
; ============================================================================
;
; Prérequis avant compilation :
;   - dist\CRYPTARA\ produit par : pyinstaller cryptara.spec
;   - icon.ico dans le même dossier que ce .iss
;
; Compilation :
;   - Ouvrir ce fichier dans Inno Setup puis "Build"
;   - Ou en ligne de commande :
;       "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" cryptara.iss
;
; Résultat : Output\CRYPTARA_Setup_1.3.6.exe
; ============================================================================

#define MyAppName "CRYPTARA"
#define MyAppVersion "1.3.6"
#define MyAppPublisher "SDR++ Community"
#define MyAppExeName "CRYPTARA.exe"

[Setup]
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputBaseFilename={#MyAppName}_Setup_{#MyAppVersion}
Compression=lzma2
SolidCompression=yes
; Application 64 bits uniquement
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=icon.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
WizardStyle=modern
; L'installation dans Program Files requiert les droits admin (normal)
PrivilegesRequired=admin

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Copie tout le dossier onedir (CRYPTARA.exe + _internal\...)
Source: "dist\CRYPTARA\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
