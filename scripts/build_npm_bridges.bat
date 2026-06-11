@echo off
setlocal enabledelayedexpansion

set QJSDIR=%~dp0quickjs
set OUTDIR=%~dp0

set ZIG=C:\Users\user\AppData\Local\opencode\tools\zig-0.13.0\zig-windows-x86_64-0.13.0\zig.exe

echo === Building left-pad_npm.dll ===
set BASE_FLAGS=-I"%QJSDIR%" -include "%QJSDIR%\quickjs_config.h" -mcpu=x86_64 -O2 -lbcrypt

"%ZIG%" cc -shared -o "%OUTDIR%left-pad_npm\left-pad_npm.dll" ^
    "%OUTDIR%left-pad_npm\left-pad_npm.c" ^
    "%QJSDIR%\quickjs.c" ^
    "%QJSDIR%\cutils.c" ^
    "%QJSDIR%\libregexp.c" ^
    "%QJSDIR%\libunicode.c" ^
    "%QJSDIR%\dtoa.c" ^
    %BASE_FLAGS%
if %ERRORLEVEL% neq 0 echo left-pad FAILED & exit /b 1

echo === Building moment_npm.dll ===
"%ZIG%" cc -shared -o "%OUTDIR%moment_npm\moment_npm.dll" ^
    "%OUTDIR%moment_npm\moment_npm.c" ^
    "%QJSDIR%\quickjs.c" ^
    "%QJSDIR%\cutils.c" ^
    "%QJSDIR%\libregexp.c" ^
    "%QJSDIR%\libunicode.c" ^
    "%QJSDIR%\dtoa.c" ^
    %BASE_FLAGS%
if %ERRORLEVEL% neq 0 echo moment FAILED & exit /b 1

echo === Building uuid_npm.dll ===
"%ZIG%" cc -shared -o "%OUTDIR%uuid_npm\uuid_npm.dll" ^
    "%OUTDIR%uuid_npm\uuid_npm.c" ^
    "%QJSDIR%\quickjs.c" ^
    "%QJSDIR%\cutils.c" ^
    "%QJSDIR%\libregexp.c" ^
    "%QJSDIR%\libunicode.c" ^
    "%QJSDIR%\dtoa.c" ^
    %BASE_FLAGS%
if %ERRORLEVEL% neq 0 echo uuid FAILED & exit /b 1

echo === All npm bridges built successfully ===
