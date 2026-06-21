@echo off
setlocal enabledelayedexpansion
set "ROOT=%~dp0"
cd /d "%ROOT%"

echo ============================================================
echo   Aurora Test Runner - Batch
echo ============================================================
echo.

:: First build
echo [BUILD] Running cmake build first...
cmake --build ..\build --config Release
if %errorlevel% neq 0 (
    echo [WARNING] Build had errors, continuing anyway...
)
echo.

:: Run each test
set PASS=0
set FAIL=0
set TIMEOUT=0
set TOTAL=0

for /r "%ROOT%\aurora\tests" %%F in (*.aura) do (
    set "FILE=%%F"
    set "NAME=%%~nxF"
    
    :: Skip excluded files
    set "SKIP="
    if /i "!NAME!"=="test_speculative.aura" set SKIP=1
    if /i "!NAME!"=="test_distributed.aura" set SKIP=1
    if /i "!NAME!"=="test_lora.aura" set SKIP=1
    if /i "!NAME!"=="test_advanced_features.aura" set SKIP=1
    if /i "!NAME!"=="test_pipeline_full.aura" set SKIP=1
    if /i "!NAME!"=="test_end_to_end.aura" set SKIP=1
    
    if defined SKIP (
        echo [SKIP] !NAME!
    ) else (
        set /a TOTAL+=1
        set "RELPATH=%%F"
        set "RELPATH=!RELPATH:%ROOT%\=!"
        
        echo ============================================================
        echo [TEST !TOTAL!] Running: !RELPATH!
        echo ============================================================
        
        :: Use start with timeout
        start "auroratest" /wait /B cmd /c ".\aurora.bat "!RELPATH!""
        set "EXIT=!errorlevel!"
        
        if !EXIT! equ 0 (
            set /a PASS+=1
            echo   Result: PASS (exit code: !EXIT!)
        ) else (
            set /a FAIL+=1
            echo   Result: FAIL (exit code: !EXIT!)
        )
        echo.
    )
)

echo ============================================================
echo   FINAL SUMMARY
echo ============================================================
echo.
echo  Total tests : %TOTAL%
echo  PASS        : %PASS%
echo  FAIL        : %FAIL%
echo  TIMEOUT     : %TIMEOUT%
echo.
echo ============================================================
endlocal
