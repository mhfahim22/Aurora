@echo off
setlocal enabledelayedexpansion
call "C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 2>nul || call vcvarsall.bat x64 2>nul || echo [WARN] vcvarsall not found — hoping MSVC is in PATH
set SRCDIR=%~dp0..
set OUTDIR=%SRCDIR%build\Debug
set INCLUDES=/I "%SRCDIR%aurora\include"

rem # Pure-Python bridges (/MT = zero runtime deps, no C extension conflicts)
echo [BUILD] Compiling requests_pypi.dll (/MT) ...
cl /MT /LD %INCLUDES% "%SRCDIR%\requests_pypi\requests_pypi.c" /Fe:"%OUTDIR%\requests_pypi\requests_pypi.dll" /link /NODEFAULTLIB:msvcrt
if %errorlevel% neq 0 exit /b 1

echo [BUILD] Compiling markdown_pypi.dll (/MT) ...
cl /MT /LD %INCLUDES% "%SRCDIR%\markdown_pypi\markdown_pypi.c" /Fe:"%OUTDIR%\markdown_pypi\markdown_pypi.dll" /link /NODEFAULTLIB:msvcrt
if %errorlevel% neq 0 exit /b 1

echo [BUILD] Compiling flask_pypi.dll (/MT) ...
cl /MT /LD %INCLUDES% "%SRCDIR%\flask_pypi\flask_pypi.c" /Fe:"%OUTDIR%\flask_pypi\flask_pypi.dll" /link /NODEFAULTLIB:msvcrt
if %errorlevel% neq 0 exit /b 1

rem # Bridges with compiled C extensions NEED /MD CRT to share heap with Python's .pyd files
echo [BUILD] Compiling numpy_pypi.dll (/MD for C ext compat) ...
cl /MD /LD %INCLUDES% "%SRCDIR%\numpy_pypi\numpy_pypi.c" /Fe:"%OUTDIR%\numpy_pypi\numpy_pypi.dll"
if %errorlevel% neq 0 exit /b 1

echo [BUILD] Compiling Pillow_pypi.dll (/MD for C ext compat) ...
cl /MD /LD %INCLUDES% "%SRCDIR%\Pillow_pypi\Pillow_pypi.c" /Fe:"%OUTDIR%\Pillow_pypi\Pillow_pypi.dll"
if %errorlevel% neq 0 exit /b 1

echo [BUILD] All PyPI bridges compiled successfully.
