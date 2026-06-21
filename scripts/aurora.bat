@echo off
cd /d "%~dp0.."
setlocal

:: Auto-detect build config (prefer Release, fallback Debug)
if exist "build\Release\aurorac.exe" (
    set "BUILD_DIR=build\Release"
) else if exist "build\Debug\aurorac.exe" (
    set "BUILD_DIR=build\Debug"
) else (
    echo [BUILD] No pre-built aurorac found — building Release...
    cmake --build build --config Release --target aurorac 2>nul
    if exist "build\Release\aurorac.exe" (
        set "BUILD_DIR=build\Release"
    ) else (
        cmake --build build --config Debug --target aurorac 2>nul
        set "BUILD_DIR=build\Debug"
    )
)

if "%~1"=="" (
    echo.
    echo [USAGE] .\aurora.bat ^<file.aura^>    - compile ^& run
    echo.
    echo   Examples:
    echo     .\aurora.bat aurora\tests\test.aura
    echo     .\aurora.bat mycode.aura
    echo.
    goto :eof
)

set "SRC=%~1"
set "BASENAME=%~n1"
set "SRCDIR=%~dp1"

set "IR_DIR=output\ir"
set "OBJ_DIR=output\obj"
set "EXE_DIR=output\exe"

set "LLFILE=%IR_DIR%\%BASENAME%.ll"
set "OBJFILE=%OBJ_DIR%\%BASENAME%.obj"
set "EXEFILE=%EXE_DIR%\%BASENAME%.exe"

if not exist "output" mkdir output
if not exist "%IR_DIR%" mkdir %IR_DIR%
if not exist "%OBJ_DIR%" mkdir %OBJ_DIR%
if not exist "%EXE_DIR%" mkdir %EXE_DIR%

echo [COMPILE] %SRC%
"%BUILD_DIR%\aurorac.exe" "%SRC%"
if %errorlevel% NEQ 0 (
    echo [FAILED] Compilation error!
    exit /b 1
)

echo [LLC]    LLVM IR --^> Object
llc "%LLFILE%" -filetype=obj -o "%OBJFILE%"
if %errorlevel% NEQ 0 (
    echo [FAILED] llc error!
    pause
    exit /b 1
)
echo         --^> %OBJFILE%

echo [LINK]   Object --^> EXE
lld-link "%OBJFILE%" "%BUILD_DIR%\aurora_runtime.lib" user32.lib /OUT:"%EXEFILE%"
if %errorlevel% NEQ 0 (
    echo [FAILED] Link error!
    exit /b 1
)
echo         --^> %EXEFILE%

echo [RUN]    %EXEFILE%
echo ============================================
set "_t0=%time: =0%"
"%EXEFILE%"
set "_t1=%time: =0%"
echo ============================================
echo [DONE] Exit code: %ERRORLEVEL%

:: Execution time (leading space replaced with 0 for single-digit hours)
set /a "_h0=1%_t0:~0,2%-100, _m0=1%_t0:~3,2%-100, _s0=1%_t0:~6,2%-100, _c0=1%_t0:~9,2%-100"
set /a "_h1=1%_t1:~0,2%-100, _m1=1%_t1:~3,2%-100, _s1=1%_t1:~6,2%-100, _c1=1%_t1:~9,2%-100"
set /a "_start = _h0*360000 + _m0*6000 + _s0*100 + _c0"
set /a "_end   = _h1*360000 + _m1*6000 + _s1*100 + _c1"
set /a "_diff  = _end - _start"
if %_diff% lss 0 set /a "_diff += 8640000"
set /a "_diff_ms = _diff * 10"
echo.
echo Run Time: %_diff_ms%ms
endlocal & exit /b %ERRORLEVEL%
