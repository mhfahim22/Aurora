; Aurora Language Installer for Windows
; Uses Inno Setup — compile with ISCC.exe
; Download: https://jrsoftware.org/isdl.php

#define MyAppName "Aurora"
#define MyAppVersion "0.2.0"
#define MyAppPublisher "Aurora Language"
#define MyAppURL "https://github.com/mhfahim22/Aurora"
#define MyAppExeName "aurorac.exe"

[Setup]
AppId={{A8B4C5D6-E7F8-9012-3456-7890ABCDEF12}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={localappdata}\Aurora
DefaultGroupName=Aurora
DisableProgramGroupPage=yes
DisableDirPage=no
DisableWelcomePage=no
OutputDir=.
OutputBaseFilename=Aurora-{#MyAppVersion}-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64compatible
ChangesEnvironment=yes
;SetupIconFile=..\aurora.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupAppTitle=Install Aurora {#MyAppVersion}
SetupWindowTitle=Install Aurora {#MyAppVersion}

[CustomMessages]
AppDescription=Aurora Programming Language — Polyglot with LLVM-native compilation

[Dirs]
; Ensure libc subdirectory exists even if empty during compile

[Files]
; ── Core compiler & tools ──
Source: "..\build\Release\aurorac.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\voss.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\aurora_lsp.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\aurora_bindgen.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\aurora_cppwrap.exe"; DestDir: "{app}"; Flags: ignoreversion

; ── Runtime DLLs (required for library imports) ──
Source: "..\build\Release\glfw3.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\cpp_interop_test.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\mysql_native.dll"; DestDir: "{app}"; Flags: ignoreversion

; ── Static libraries (for AOT compilation) ──
Source: "..\build\Release\aurora_runtime.lib"; DestDir: "{app}\lib"; Flags: ignoreversion

; ── Standard library (.auf files) ──
Source: "..\libc\*.auf"; DestDir: "{app}\libc"; Flags: ignoreversion recursesubdirs

; ── Headers and includes ──
Source: "..\aurora\include\runtime\*.hpp"; DestDir: "{app}\include\runtime"; Flags: ignoreversion
Source: "..\aurora\include\runtime\*.h"; DestDir: "{app}\include\runtime"; Flags: ignoreversion
Source: "..\aurora\include\runtime\interop\*.hpp"; DestDir: "{app}\include\runtime\interop"; Flags: ignoreversion
Source: "..\aurora\include\runtime\ui\*.h"; DestDir: "{app}\include\runtime\ui"; Flags: ignoreversion
Source: "..\aurora\include\compiler\*.hpp"; DestDir: "{app}\include\compiler"; Flags: ignoreversion

; ── Documentation ──
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\aurora\docs\*.md"; DestDir: "{app}\docs"; Flags: ignoreversion
Source: "..\SECURITY.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\CONTRIBUTING.md"; DestDir: "{app}"; Flags: ignoreversion

; ── Examples ──
Source: "..\examples\*"; DestDir: "{app}\examples"; Flags: ignoreversion recursesubdirs

; ── deps (stb_image.h) ──
Source: "..\deps\stb_image.h"; DestDir: "{app}\deps"; Flags: ignoreversion

[Icons]
Name: "{group}\Aurora REPL"; Filename: "{app}\aurorac.exe"; Parameters: "--repl"; WorkingDir: "{app}"
Name: "{group}\Aurora Documentation"; Filename: "{app}\docs\language.md"
Name: "{group}\Aurora Examples"; Filename: "{app}\examples"
Name: "{group}\Uninstall Aurora"; Filename: "{uninstallexe}"
Name: "{commondesktop}\Aurora REPL"; Filename: "{app}\aurorac.exe"; Parameters: "--repl"; WorkingDir: "{app}"

[Run]
; Optionally run after install
Filename: "{app}\aurorac.exe"; Parameters: "--repl"; Description: "Launch Aurora REPL"; Flags: postinstall nowait skipifsilent unchecked

[Registry]
; Add Aurora to PATH (HKCU so it doesn't require admin)
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Check: NeedsAddPath(ExpandConstant('{app}'))
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "AURORA_PATH"; ValueData: "{app}\libc"; Flags: deletevalue
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "AURORA_LIB"; ValueData: "{app}\libc"; Flags: deletevalue

[Code]
const
  WM_WININICHANGE = $001A;

function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKCU, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  Result := Pos(LowerCase(Param), LowerCase(OrigPath)) = 0;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    SendBroadcastMessage(WM_WININICHANGE, 0, 'Environment');
  end;
end;
