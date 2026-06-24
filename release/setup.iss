; Aurora Language Installer for Windows
; Uses Inno Setup — compile with ISCC.exe
; Download: https://jrsoftware.org/isdl.php

#define MyAppName "Aurora"
#define MyAppVersion "0.3.0"
#define MyAppPublisher "Aurora Language"
#define MyAppURL "https://github.com/mhfahim22/Aurora"
#define MyAppExeName "aurorac.exe"

[Setup]
AppId={{B8C5D6E7-F8A9-0123-4567-89ABCDEF0123}
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
; Source: "..\build\Release\aurora_bindgen.exe"; DestDir: "{app}"; Flags: ignoreversion
; Source: "..\build\Release\aurora_cppwrap.exe"; DestDir: "{app}"; Flags: ignoreversion

; ── Runtime DLLs (required for library imports) ──
; NOTE: These are external dependencies — uncomment if present in build\Release\ before compiling
; Source: "..\build\Release\glfw3.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "..\build\Release\cpp_interop_test.dll"; DestDir: "{app}"; Flags: ignoreversion
; Source: "..\build\Release\mysql_native.dll"; DestDir: "{app}"; Flags: ignoreversion

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

; ── Helper scripts ──
Source: "run.bat"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Aurora REPL"; Filename: "{app}\aurorac.exe"; Parameters: "--repl"; WorkingDir: "{app}"
Name: "{group}\Aurora Documentation"; Filename: "{app}\docs\language.md"
Name: "{group}\Aurora Examples"; Filename: "{app}\examples"
Name: "{group}\Uninstall Aurora"; Filename: "{uninstallexe}"
Name: "{commondesktop}\Aurora REPL"; Filename: "{app}\aurorac.exe"; Parameters: "--repl"; WorkingDir: "{app}"

[Run]
; Optionally run after install — disabled by default
; Filename: "{app}\aurorac.exe"; Parameters: "--repl"; Description: "Launch Aurora REPL"; Flags: postinstall nowait skipifsilent unchecked

[Registry]
; Note: PATH is managed by install.ps1 to avoid Inno Setup type-mismatch errors
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "AURORA_PATH"; ValueData: "{app}\libc"
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "AURORA_LIB"; ValueData: "{app}\libc"

; ── File association: .aura → run.bat ──
Root: HKCU; Subkey: "Software\Classes\.aura"; ValueType: string; ValueName: ""; ValueData: "AuroraSourceFile"; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\AuroraSourceFile"; ValueType: string; ValueName: ""; ValueData: "Aurora Source File"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\AuroraSourceFile\shell"; ValueType: string; ValueName: ""; ValueData: "run"
Root: HKCU; Subkey: "Software\Classes\AuroraSourceFile\shell\run"; ValueType: string; ValueName: ""; ValueData: "Run with Aurora"
Root: HKCU; Subkey: "Software\Classes\AuroraSourceFile\shell\run\command"; ValueType: string; ValueName: ""; ValueData: """{app}\run.bat"" ""%1"""

; [Code] section intentionally empty — PATH is managed by install.ps1
