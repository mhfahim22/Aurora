@echo off
setlocal enabledelayedexpansion
set "AURORAC=%~dp0build_release\Release\aurorac.exe"
set "SRC=%~1"
if "!SRC!"=="" (
    echo Usage: aurora file.aura [--aot / --run]
    exit /b 1
)
shift
set "MODE=run"
:parse
if /i "%~1"=="--aot" set "MODE=aot" && shift && goto :parse
if /i "%~1"=="--run" set "MODE=run" && shift && goto :parse

if /i "!MODE!"=="aot" (
    for %%F in ("%SRC%") do set "OUT=%%~nF.exe"
    "!AURORAC!" "!SRC!" --aot -O2 %*
) else (
    "!AURORAC!" "!SRC!" --run -O2 %*
)
