@echo off
setlocal
set "AURORAC=%~dp0aurorac.exe"
set "ARGS=%* --run"
echo.
powershell -NoProfile -ExecutionPolicy Bypass -Command "try { $pinfo = New-Object System.Diagnostics.ProcessStartInfo; $pinfo.FileName = '%AURORAC:\=\\%'; $pinfo.Arguments = '%ARGS%'; $pinfo.RedirectStandardOutput = $true; $pinfo.RedirectStandardError = $true; $pinfo.UseShellExecute = $false; $p = [System.Diagnostics.Process]::Start($pinfo); $stdout = $p.StandardOutput.ReadToEnd(); $stderr = $p.StandardError.ReadToEnd(); $p.WaitForExit(); if ($stderr) { Write-Host $stderr.TrimEnd() }; Write-Host ''; Write-Host '-----OUTPUT----'; if ($stdout) { Write-Host $stdout.TrimEnd() }; Write-Host ''; Write-Host '---------------'; exit $p.ExitCode } catch { Write-Host '[ERROR] ' + $_.Exception.Message; exit 1 }"
if %ERRORLEVEL% neq 0 (
    echo [ERROR]
)
echo.
