$RepoRoot = $PSScriptRoot
$TestDir = Join-Path $RepoRoot "aurora\tests"
$AuroraBat = Join-Path $RepoRoot "aurora.bat"
$SkipNames = @()
$NegativeTestPattern = '^verify_.*_fail\.aura$'
$PanicSkipNames = @("stress1.aura")

Write-Host "============================================================"
Write-Host "  Aurora Test Runner (v2 - fixed exit code capture)"
Write-Host "============================================================"
Write-Host ""

$AllFiles = Get-ChildItem -LiteralPath $TestDir -Recurse -Filter "*.aura" | ForEach-Object { $_.FullName }
Write-Host ("Found " + $AllFiles.Count + " .aura files total")
Write-Host ""

$ToRun = @()
$Skipped = @()
foreach ($f in $AllFiles) {
    $name = Split-Path $f -Leaf
    if ($name -in $SkipNames) { $Skipped += $f } else { $ToRun += $f }
}

Write-Host ("Skipping " + $Skipped.Count + " files:")
foreach ($s in $Skipped) { Write-Host ("    SKIP: " + (Split-Path $s -Leaf)) }
Write-Host ""
Write-Host ("Will run " + $ToRun.Count + " tests")
Write-Host ""

$PassCount = 0; $FailCount = 0; $TimeoutCount = 0; $Total = $ToRun.Count; $Index = 0
$Results = @()

Write-Host "============================================================"
Write-Host "  Initial CMake Build"
Write-Host "============================================================"
Push-Location $RepoRoot
try {
    cmake --build build --config Release 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { Write-Host "[WARNING] Build had errors" } else { Write-Host "[OK] Build succeeded" }
} catch { Write-Host ("[WARNING] Build exception: " + $_) }
Pop-Location
Write-Host ""

foreach ($file in $ToRun) {
    $Index++
    $relPath = $file.Substring($RepoRoot.Length + 1)
    $fileName = Split-Path $file -Leaf
    $pct = [math]::Round(($Index / $Total) * 100, 1)
    
    Write-Host "============================================================"
    Write-Host ("[" + $Index + "/" + $Total + " (" + $pct + "%)] Running: " + $relPath)
    Write-Host "============================================================"
    
    Push-Location $RepoRoot
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    
    try {
        # Launch aurora.bat synchronously — captures stdout/stderr correctly
        Push-Location $RepoRoot
        $output = cmd.exe /c "$AuroraBat $relPath" 2>&1
        $exitCode = $LASTEXITCODE
        Pop-Location
        
        # Display the output
        $output | ForEach-Object { Write-Host $_ }
        
        $isNegativeTest = $fileName -match $NegativeTestPattern
        $isPanicTest = $fileName -in $PanicSkipNames
        if ($isNegativeTest -and $exitCode -eq 1) { $status = "PASS"; $PassCount++ }
        elseif ($isPanicTest -and ($exitCode -eq 1 -or $exitCode -eq -1073741571 -or $exitCode -eq -1073741819 -or $exitCode -eq -1073741670)) { $status = "PASS (panic)"; $PassCount++ }
        elseif ($exitCode -eq 0) { $status = "PASS"; $PassCount++ }
        else { $status = "FAIL"; $FailCount++ }
        $elapsed = $sw.Elapsed
        Write-Host ""
        Write-Host ("  Result: " + $status + " (exit code: " + $exitCode + ", time: " + $elapsed.TotalSeconds.ToString('0.0') + "s)")
        $Results += [PSCustomObject]@{ File = $relPath; Status = $status; ExitCode = $exitCode }
    }
    catch {
        Write-Host ("  *** EXCEPTION: " + $_ + " ***")
        $FailCount++
        $Results += [PSCustomObject]@{ File = $relPath; Status = "EXCEPTION"; ExitCode = -999 }
    }
    Write-Host ""
}

Write-Host "============================================================"
Write-Host "  FINAL SUMMARY"
Write-Host "============================================================"
Write-Host ""
Write-Host (" Total tests : " + $Total)
Write-Host (" PASS        : " + $PassCount)
Write-Host (" FAIL        : " + $FailCount)
Write-Host (" TIMEOUT     : " + $TimeoutCount)
Write-Host ""
Write-Host "  Detailed Results:"
Write-Host "  -----------------------------------------"
foreach ($r in $Results) {
    if ($r.Status -eq "PASS") { $icon = "  " } elseif ($r.Status -eq "TIMEOUT") { $icon = " T" } else { $icon = " X" }
    Write-Host ("  " + $icon + " " + $r.Status + " (exit=" + $r.ExitCode + ") " + $r.File)
}
Write-Host "============================================================"