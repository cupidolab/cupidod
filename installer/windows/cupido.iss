; =============================================================================
;  CUPIDO — instalador Windows (Inno Setup 6)
;  Compilar:  iscc installer\windows\cupido.iss /DSourceDir=..\..\build\Cupido_artefacts\Release
;  (o GitHub Actions já faz isso automaticamente)
; =============================================================================
#define AppName      "CUPIDO"
#define AppVersion   GetEnv("CUPIDO_VERSION") != "" ? GetEnv("CUPIDO_VERSION") : "1.0.0"
#define AppPublisher "Cupido Audio"
#ifndef SourceDir
  #define SourceDir "..\..\build\Cupido_artefacts\Release"
#endif

[Setup]
AppId={{6E3C2B7A-9D41-4C8E-A5F2-C0B1D0E3F7A1}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL=https://cupido.audio
DefaultDirName={autopf}\{#AppPublisher}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=..\..\dist
OutputBaseFilename=CUPIDO-{#AppVersion}-Windows-Instalador
SetupIconFile=cupido.ico
UninstallDisplayIcon={app}\CUPIDO.exe
WizardStyle=modern
WizardImageFile=wizard-large-100.bmp,wizard-large.bmp
WizardSmallImageFile=wizard-small-100.bmp,wizard-small.bmp
WizardImageStretch=yes
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
LicenseFile=..\..\LICENSE.txt
ShowLanguageDialog=auto
CloseApplications=yes

[Languages]
Name: "ptbr"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "en";   MessagesFile: "compiler:Default.isl"

[CustomMessages]
ptbr.CompVST3=Plugin VST3 (Ableton, FL Studio, Cubase, Studio One, Reaper, Bitwig…)
ptbr.CompAAX=Plugin AAX (Pro Tools)
ptbr.CompApp=Aplicativo independente (Standalone)
ptbr.CompPresets=Presets de exemplo na pasta Documentos
ptbr.WebView2Msg=O CUPIDO precisa do Microsoft Edge WebView2 (já vem no Windows 10/11). Instalar agora?
en.CompVST3=VST3 plugin
en.CompAAX=AAX plugin (Pro Tools)
en.CompApp=Standalone application
en.CompPresets=Example presets in Documents
en.WebView2Msg=CUPIDO needs Microsoft Edge WebView2. Install it now?

[Types]
Name: "full";   Description: "Completa"
Name: "custom"; Description: "Personalizada"; Flags: iscustom

[Components]
Name: "vst3"; Description: "{cm:CompVST3}"; Types: full custom; Flags: fixed
#if DirExists(SourceDir + "\AAX\CUPIDO.aaxplugin")
Name: "aax";  Description: "{cm:CompAAX}"; Types: full
#endif
Name: "app";  Description: "{cm:CompApp}"; Types: full

[Files]
Source: "{#SourceDir}\VST3\CUPIDO.vst3\*"; DestDir: "{commoncf64}\VST3\CUPIDO.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
#if DirExists(SourceDir + "\AAX\CUPIDO.aaxplugin")
Source: "{#SourceDir}\AAX\CUPIDO.aaxplugin\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins\CUPIDO.aaxplugin"; Components: aax; Flags: ignoreversion recursesubdirs createallsubdirs
#endif
Source: "{#SourceDir}\Standalone\CUPIDO.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "..\..\docs\COMO-USAR.pdf"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
#if FileExists("MicrosoftEdgeWebview2Setup.exe")
Source: "MicrosoftEdgeWebview2Setup.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall
#endif

[Icons]
Name: "{group}\CUPIDO"; Filename: "{app}\CUPIDO.exe"; Components: app
Name: "{autodesktop}\CUPIDO"; Filename: "{app}\CUPIDO.exe"; Components: app; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: app

[Run]
#if FileExists("MicrosoftEdgeWebview2Setup.exe")
Filename: "{tmp}\MicrosoftEdgeWebview2Setup.exe"; Parameters: "/silent /install"; StatusMsg: "Instalando WebView2…"; Check: NeedsWebView2
#endif
Filename: "{app}\CUPIDO.exe"; Description: "{cm:LaunchProgram,CUPIDO}"; Flags: nowait postinstall skipifsilent; Components: app

[Code]
// WebView2 instalado? (chave do runtime "Evergreen")
function NeedsWebView2: Boolean;
var v: String;
begin
  Result := not (RegQueryStringValue(HKLM, 'SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', v) and (v <> '') and (v <> '0.0.0.0'))
        and not (RegQueryStringValue(HKCU, 'Software\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}', 'pv', v) and (v <> '') and (v <> '0.0.0.0'));
end;
