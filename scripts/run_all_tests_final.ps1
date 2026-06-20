param(
    [string]$Compiler = "build/Release/aurorac.exe",
    [string]$RuntimeLib = "build/Release/aurora_runtime.lib"
)

$Root = Split-Path -Parent $PSScriptRoot
$CompilerPath = Join-Path $Root $Compiler
$RuntimeLibPath = Join-Path $Root $RuntimeLib

if (!(Test-Path $CompilerPath)) { Write-Host "ERROR: Compiler not at $CompilerPath" -ForegroundColor Red; exit 1 }
if (!(Test-Path $RuntimeLibPath)) { Write-Host "ERROR: Runtime not at $RuntimeLibPath" -ForegroundColor Red; exit 1 }

# Ensure output dirs
@("output/exe", "output/ir", "output/obj") | ForEach-Object {
    $p = Join-Path $Root $_
    if (!(Test-Path $p)) { New-Item -ItemType Directory -Path $p | Out-Null }
}

$env:Path = "C:\LLVM\bin;$env:Path"

$AllTests = Get-ChildItem -Path (Join-Path $Root "aurora/tests") -Filter "*.aura" -Recurse | Sort-Object FullName

# Known non-compilable/test files
$ExcludeFiles = @(
    "color_demo.aura"          # Syntax coloring demo, not runnable
)

# Expected compile failures (should fail to compile)
$ExpectedFail = @(
    "verify_abstract_fail",
    "verify_encap_fail",
    "verify_final_fail",
    "verify_visibility_fail",
    "verify_gc_perf_error"
)

# Tests that need special runtime/dependencies
$SkipFiles = @(
    "test_game_engine", "test_gui_crossplatform", "test_ui_framework",
    "test_event_fiber", "test_backend_integration",
    "test_async_concurrency",
    "test_cpp_interop", "test_libtorch", "bench_speed",
    "test_all_bridges", "test_cargo_bridge", "test_native_bridge",
    "test_npm_bridge", "test_pypi_bridge", "test_integration_http",
    "bench_1B", "bench_power", "_bench_gpu", "_gpu_detect",
    "test_lora", "test_distributed", "test_end_to_end", "test_enterprise"
)

$Results = @()
$Pass = 0; $Fail = 0; $Crash = 0; $Skip = 0; $Total = 0

foreach ($File in $AllTests) {
    $RelPath = $File.FullName.Substring($Root.Length + 1)
    $BaseName = $File.BaseName

    # Skip excluded files
    if ($BaseName -in $ExcludeFiles -or $File.Name -in $ExcludeFiles) {
        Write-Host "[SKIP] $RelPath (excluded)" -ForegroundColor Yellow; $Skip++; continue
    }

    # Skip files with deps
    $shouldSkip = $false
    foreach ($s in $SkipFiles) { if ($BaseName -eq $s) { $shouldSkip = $true; break } }
    if ($shouldSkip) { Write-Host "[SKIP] $RelPath (deps)" -ForegroundColor Yellow; $Skip++; continue }

    $Total++
    $IsExpectedFail = $BaseName -in $ExpectedFail
    Write-Host -NoNewline "[$Total] $RelPath ... "

    # Compile
    $CompOut = & $CompilerPath $File.FullName 2>&1
    $CompRes = $LASTEXITCODE

    if ($CompRes -ne 0) {
        if ($IsExpectedFail) { Write-Host "PASS (expected fail)" -ForegroundColor Green; $Pass++; continue }
        Write-Host "FAIL (compile)" -ForegroundColor Red; $Fail++
        $Results += [PSCustomObject]@{ File = $RelPath; Stage = "compile"; Error = $CompOut }
        continue
    }
    if ($IsExpectedFail) { Write-Host "UNEXPECTED PASS" -ForegroundColor Yellow }

    # llc
    $IrFile = Join-Path $Root "output/ir/$BaseName.ll"
    $ObjFile = Join-Path $Root "output/obj/$BaseName.obj"
    $LlOut = & llc $IrFile -filetype=obj -o $ObjFile 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL (llc)" -ForegroundColor Red; $Fail++
        $Results += [PSCustomObject]@{ File = $RelPath; Stage = "llc"; Error = $LlOut }
        continue
    }

    # Link
    $ExeFile = Join-Path $Root "output/exe/$BaseName.exe"
    $LinkOut = & lld-link $ObjFile $RuntimeLibPath user32.lib /OUT:$ExeFile /NOLOGO 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL (link)" -ForegroundColor Red; $Fail++
        $Results += [PSCustomObject]@{ File = $RelPath; Stage = "link"; Error = $LinkOut }
        continue
    }

    # Run (with timeout via job)
    $job = Start-Job -ScriptBlock { param($e) & $e 2>&1 } -ArgumentList $ExeFile
    $RunOut = $job | Wait-Job -Timeout 30 2>&1
    if ($job.State -eq "Running") { Stop-Job $job; Remove-Job $job; Write-Host "TIMEOUT" -ForegroundColor Yellow; $Crash++; continue }
    $RunOut = Receive-Job $job; Remove-Job $job
    $RunRes = $LASTEXITCODE

    if ($RunRes -ne 0 -and $RunRes -ne $null) {
        Write-Host "CRASH (exit: $RunRes)" -ForegroundColor Red; $Crash++
        $Results += [PSCustomObject]@{ File = $RelPath; Stage = "runtime"; Error = "Exit: $RunRes" }
    } else {
        Write-Host "PASS" -ForegroundColor Green; $Pass++
    }
}

Write-Host "`n========================================"
Write-Host "  Total: $Total | Pass: $Pass | Fail: $Fail | Crash: $Crash | Skip: $Skip"
Write-Host "========================================"

if ($Results.Count -gt 0) {
    Write-Host "`nIssues:" -ForegroundColor Red
    foreach ($r in $Results) { Write-Host "  [$($r.Stage)] $($r.File)" -ForegroundColor Red }
}

exit ($Fail + $Crash)
