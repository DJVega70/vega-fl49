; Vega Productions DAW / VEGA FL49 - Windows installer (Inno Setup 6)
; Built by GitHub Actions: iscc /DAppVersion=0.4.0.<build> /DSrcDir=<dist folder> [/DFL49=1] installer\VegaDAW.iss
; /DFL49=1 builds the VEGA FL49 edition's installer: its own AppId and folder, so both products can be installed side by side.

#ifndef AppVersion
  #define AppVersion "0.4.0"
#endif
#ifndef SrcDir
  #define SrcDir "..\dist"
#endif

#ifdef FL49
  #define Product "VEGA FL49"
  #define ProductId "{{9C3F7A21-4D8E-4B6A-8E1F-2A7C5D9B3E41}"
  #define Folder "VEGA FL49"
  #define SetupName "VEGA-FL49-Setup"
#else
  #define Product "Vega Productions DAW"
  #define ProductId "{{6B1E2C4A-5F7D-4E8B-9A3C-1D2E3F4A5B6C}"
  #define Folder "DAW"
  #define SetupName "VegaProductions-DAW-Setup"
#endif
#define ExeName Product + ".exe"

[Setup]
AppId={#ProductId}
AppName={#Product}
AppVersion={#AppVersion}
AppVerName={#Product} {#AppVersion}
AppPublisher=Vega Productions
DefaultDirName={autopf}\Vega Productions\{#Folder}
DefaultGroupName=Vega Productions
DisableProgramGroupPage=yes
LicenseFile={#SrcDir}\LICENSE
OutputDir=..\installer-out
OutputBaseFilename={#SetupName}
SetupIconFile=..\Resources\VegaDAW.ico
UninstallDisplayIcon={app}\{#ExeName}
UninstallDisplayName={#Product}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
ChangesAssociations=yes
CloseApplications=yes
PrivilegesRequiredOverridesAllowed=dialog

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Shortcuts:"
#ifdef FL49
Name: "assoc"; Description: "Open .vegaproj projects with {#Product}"; GroupDescription: "Files:"
#else
Name: "assoc"; Description: "Open .vegaproj projects with {#Product}"; GroupDescription: "Files:"
#endif

[Files]
Source: "{#SrcDir}\{#ExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SrcDir}\VegaStemHelper.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SrcDir}\VegaStemHelperAVX2.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SrcDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SrcDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SrcDir}\TRACKTION_LICENSE.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#Product}"; Filename: "{app}\{#ExeName}"
Name: "{group}\Uninstall {#Product}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#Product}"; Filename: "{app}\{#ExeName}"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\Classes\.vegaproj"; ValueType: string; ValueName: ""; ValueData: "VegaProductions.Project"; Flags: uninsdeletevalue; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\VegaProductions.Project"; ValueType: string; ValueName: ""; ValueData: "Vega Productions project"; Flags: uninsdeletekey; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\VegaProductions.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#ExeName},0"; Tasks: assoc
Root: HKA; Subkey: "Software\Classes\VegaProductions.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#ExeName}"" ""%1"""; Tasks: assoc

[Run]
Filename: "{app}\{#ExeName}"; Description: "Start {#Product}"; Flags: nowait postinstall skipifsilent
