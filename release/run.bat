@echo off
setlocal
set "TMPFILE=%TEMP%\aurora_err_%RANDOM%.tmp"
echo.
echo ---- Output ----
"%~dp0aurorac.exe" %* --run 2>"%TMPFILE%"
if %ERRORLEVEL% neq 0 (
    echo [ERROR]
    type "%TMPFILE%"
)
del "%TMPFILE%" 2>nul
echo -----------------
echo.
