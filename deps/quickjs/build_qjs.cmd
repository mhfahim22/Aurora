@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set QJSDIR=%~dp0
set OUTDIR=%QJSDIR%build
if not exist "%OUTDIR%" mkdir "%OUTDIR%"

echo Compiling QuickJS core with MSVC...
cl.exe /nologo /O2 /GL /W3 /EHsc /MD /c /Fo"%OUTDIR%\quickjs.obj" "%QJSDIR%quickjs.c" /I"%QJSDIR%"
cl.exe /nologo /O2 /GL /W3 /EHsc /MD /c /Fo"%OUTDIR%\cutils.obj" "%QJSDIR%cutils.c" /I"%QJSDIR%"
cl.exe /nologo /O2 /GL /W3 /EHsc /MD /c /Fo"%OUTDIR%\libregexp.obj" "%QJSDIR%libregexp.c" /I"%QJSDIR%"
cl.exe /nologo /O2 /GL /W3 /EHsc /MD /c /Fo"%OUTDIR%\libunicode.obj" "%QJSDIR%libunicode.c" /I"%QJSDIR%"

echo Linking quickjs_static.lib...
lib.exe /nologo /LTCG /OUT:"%OUTDIR%\quickjs_static.lib" ^
    "%OUTDIR%\quickjs.obj" "%OUTDIR%\cutils.obj" ^
    "%OUTDIR%\libregexp.obj" "%OUTDIR%\libunicode.obj"

echo Done.
