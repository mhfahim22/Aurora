@echo off
setlocal enabledelayedexpansion

cd /d D:\Downloads\aurora_restructured

set "CL=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe"
set "LIB=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\lib.exe"

set "INCLUDES=-Iaurora\src\runtime\backend -Iaurora\include -Iaurora\src -Ibuild\_deps\llvm-18.1.8-src\include -Ibuild\_deps\curl-8.5.0\include -Ibuild\_deps\sqlite3-3.45.0 -Ibuild\_deps\zlib-1.3.1 -Ibuild\generated"
set "INCLUDES=%INCLUDES% -I"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\include""
set "INCLUDES=%INCLUDES% -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\ucrt""
set "INCLUDES=%INCLUDES% -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um""
set "INCLUDES=%INCLUDES% -I"C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared""

echo Compiling server.cpp...
"%CL%" /c /nologo /O2 /MD /EHsc /std:c++17 %INCLUDES% /Fobuild\server_new.obj aurora\src\runtime\backend\server.cpp
if %ERRORLEVEL% NEQ 0 (
    echo COMPILATION FAILED!
    exit /b 1
)
echo server.cpp compiled OK

echo Finding server.obj in library...
"%LIB%" /LIST build\Release\aurora_runtime.lib > build\lib_contents.txt
findstr /i "server" build\lib_contents.txt
