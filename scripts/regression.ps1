#!/usr/bin/env pwsh
param(
    [string]$BuildDir = "",
    [string]$OptPath = ""
)

$Root = Split-Path -Parent $PSScriptRoot
$IsWinOS = $env:OS -eq "Windows_NT"

if (-not $BuildDir) {
    $BuildDir = if ($IsWinOS) { "build/Release" } else { "build" }
}
$Compiler = Join-Path (Join-Path $Root $BuildDir) $(if ($IsWinOS) { "aurorac.exe" } else { "aurorac" })
$RuntimeLib = Join-Path (Join-Path $Root $BuildDir) $(if ($IsWinOS) { "aurora_runtime.lib" } else { "libaurora_runtime.a" })
if (-not $OptPath) {
    $OptPath = if ($IsWinOS) { "opt.exe" } else { "opt" }
}
$TestSrcDir = Join-Path $PSScriptRoot "test_src"
$ExamplesDir = Join-Path $Root "examples"

$global:StagePass = 0; $global:StageFail = 0; $global:StageTotal = 0

function Write-Step($msg) {
    Write-Host "`n========================================================" -ForegroundColor Cyan
    Write-Host "  $msg" -ForegroundColor Cyan
    Write-Host "========================================================" -ForegroundColor Cyan
}

function Test-Result($name, $ok) {
    $global:StageTotal++
    if ($ok) { $global:StagePass++; Write-Host "  [PASS] $name" -ForegroundColor Green }
    else { $global:StageFail++; Write-Host "  [FAIL] $name" -ForegroundColor Red }
}

# Stage 0: Pre-checks
Write-Step "Stage 0: Pre-checks"
$preOk = $true
if (-not (Test-Path $Compiler)) { Write-Host "FAIL: Compiler not found: $Compiler" -ForegroundColor Red; $preOk = $false }
if (-not (Test-Path $RuntimeLib)) { Write-Host "FAIL: Runtime lib not found: $RuntimeLib" -ForegroundColor Red; $preOk = $false }
if (-not (Test-Path $OptPath)) {
    $found = Get-Command "opt" -ErrorAction SilentlyContinue
    if (-not $found) { $found = Get-Command "opt.exe" -ErrorAction SilentlyContinue }
    if ($found) { $OptPath = $found.Source }
}
if (-not (Test-Path $OptPath)) { Write-Host "FAIL: opt not found: $OptPath" -ForegroundColor Red; $preOk = $false }
if (-not (Test-Path $TestSrcDir)) { Write-Host "FAIL: Test src dir missing" -ForegroundColor Red; $preOk = $false }
if (-not (Test-Path $ExamplesDir)) { Write-Host "FAIL: Examples dir missing" -ForegroundColor Red; $preOk = $false }
if (-not $preOk) { Write-Host "Pre-checks failed" -ForegroundColor Red; exit 1 }
Test-Result "Pre-checks" $true

# Stage 1: CTest
Write-Step "Stage 1: CTest (runtime tests)"
Push-Location (Join-Path $Root "build")
$ctestOut = & ctest --output-on-failure -C Release --timeout 60 2>&1
$ctestExit = $LASTEXITCODE
Pop-Location
$ctestOk = ($ctestOut -match "100% tests passed") -and ($ctestExit -eq 0)
Test-Result "CTest (fiber + autograd + server)" $ctestOk
if (-not $ctestOk) { Write-Host $ctestOut -ForegroundColor DarkRed }

# Stage 2: IR verification
Write-Step "Stage 2: IR verification - all examples"
$irVerifyScript = Join-Path $PSScriptRoot "test_ir_verify.ps1"
if (Test-Path $irVerifyScript) {
    Push-Location $Root
    $irOut = & powershell -ExecutionPolicy Bypass -File $irVerifyScript -Compiler $Compiler 2>&1
    $irExit = $LASTEXITCODE
    Pop-Location
} else {
    $irExit = 1
}
Test-Result "IR verify (49 examples)" ($irExit -eq 0)
if ($irExit -ne 0 -and $irOut) { Write-Host $irOut -ForegroundColor DarkRed }

# Stage 3: Compiler feature tests
Write-Step "Stage 3: Compiler feature tests"
$tmpDir = Join-Path $Root "output/regression_tmp"
if (Test-Path $tmpDir) { Remove-Item -Recurse -Force $tmpDir -ErrorAction SilentlyContinue }
New-Item -ItemType Directory -Path $tmpDir -Force | Out-Null

$featureTests = @(
    @{ File = "demo/simple.aura";      Desc = "Simple output function" },
    @{ File = "demo/test_import.aura"; Desc = "Import demo" },
    @{ File = "simple_test.aura";      Desc = "Simple JIT test" }
)

Push-Location $Root
foreach ($test in $featureTests) {
    $src = Join-Path $ExamplesDir $test.File
    if (-not (Test-Path $src)) { Test-Result "$($test.Desc) - source not found" $false; continue }

    $outDir = Join-Path $Root "output/ir"
    if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir -ErrorAction SilentlyContinue }

    $compOut = & $Compiler $src 2>&1
    if ($LASTEXITCODE -ne 0) { if ($compOut) { $compOut | ForEach-Object { Write-Host "  $_" } }; Test-Result "$($test.Desc) - compile" $false; continue }

    $llFile = Get-ChildItem -Path $outDir -Filter "*.ll" | Select-Object -First 1
    if (-not $llFile) { Test-Result "$($test.Desc) - no .ll output" $false; continue }

    $verifyOut = & $OptPath -passes=verify -o nul $llFile.FullName 2>&1
    Test-Result $test.Desc ($LASTEXITCODE -eq 0)
}
Pop-Location

# Stage 4: Stdlib compilation tests
Write-Step "Stage 4: Stdlib compilation tests"
$stdlibTests = @(
    @{ File = "test_math.aura";      Desc = "Stdlib math" },
    @{ File = "test_generics.aura";  Desc = "Generics identity[T]" },
    @{ File = "test_crypto.aura";    Desc = "Stdlib crypto" },
    @{ File = "test_regex.aura";     Desc = "Stdlib regex" },
    @{ File = "test_json.aura";      Desc = "Stdlib JSON" }
)

Push-Location $Root
foreach ($test in $stdlibTests) {
    $src = Join-Path $TestSrcDir $test.File
    if (-not (Test-Path $src)) { Test-Result "$($test.Desc) - source not found" $false; continue }

    $outDir = Join-Path $Root "output/ir"
    if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir -ErrorAction SilentlyContinue }

    $compOut = & $Compiler $src 2>&1
    if ($LASTEXITCODE -ne 0) { if ($compOut) { $compOut | ForEach-Object { Write-Host "  $_" } }; Test-Result "$($test.Desc) - compile" $false; continue }

    $llFile = Get-ChildItem -Path $outDir -Filter "*.ll" | Select-Object -First 1
    if (-not $llFile) { Test-Result "$($test.Desc) - no .ll output" $false; continue }

    $verifyOut = & $OptPath -passes=verify -o nul $llFile.FullName 2>&1
    Test-Result $test.Desc ($LASTEXITCODE -eq 0)
}
Pop-Location

# Stage 5: JIT execution tests
Write-Step "Stage 5: JIT execution tests"
Push-Location $Root
$jitOut = & $Compiler (Join-Path $ExamplesDir "simple_test.aura") --run 2>&1
$jitOk = ($LASTEXITCODE -eq 0) -and ($jitOut -match "Hello from Aurora")
Test-Result "JIT: simple_test.aura" $jitOk
Pop-Location

# Stage 6: Error-detection tests (expected compile failures)
Write-Step "Stage 6: Error-detection tests"
$errorTests = @(
    @{ File = "err_type_mismatch.aura";    Desc = "Unexpected 'then' syntax error" },
    @{ File = "err_undefined_var.aura";   Desc = "Undefined variable error" },
    @{ File = "err_wrong_arg_count.aura"; Desc = "Wrong argument count error" },
    @{ File = "err_bad_syntax.aura";       Desc = "Bad syntax error" }
)

Push-Location $Root
foreach ($test in $errorTests) {
    $src = Join-Path $TestSrcDir $test.File
    if (-not (Test-Path $src)) { Test-Result "$($test.Desc) - source not found" $false; continue }

    $outDir = Join-Path $Root "output/ir"
    if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir -ErrorAction SilentlyContinue }

    $compOut = & $Compiler $src 2>&1
    $expectFail = ($LASTEXITCODE -ne 0)
    Test-Result $test.Desc $expectFail
    if (-not $expectFail) {
        Write-Host "  (expected compile failure, got success)" -ForegroundColor Yellow
    }
}
Pop-Location

# Stage 7: Phase 35 Web framework test compilation
Write-Step "Stage 7: Phase 35 Web framework test compilation"
$webTestDir = Join-Path $Root "Workflow/tests"
$webTests = @(
    @{ File = "test_web_server.aura";       Desc = "Web server lifecycle" },
    @{ File = "test_web_routes.aura";       Desc = "Web route registration" },
    @{ File = "test_web_params.aura";       Desc = "Web param accessors" },
    @{ File = "test_web_cors.aura";         Desc = "Web CORS" },
    @{ File = "test_web_csrf.aura";         Desc = "Web CSRF" },
    @{ File = "test_web_session.aura";      Desc = "Web session management" },
    @{ File = "test_web_auth.aura";         Desc = "Web auth" },
    @{ File = "test_web_websocket.aura";    Desc = "Web websocket/SSE" },
    @{ File = "test_web_template.aura";     Desc = "Web template engine" },
    @{ File = "test_web_validation.aura";   Desc = "Web request validation" },
    @{ File = "test_web_rate_limit.aura";   Desc = "Web rate limiting" },
    @{ File = "test_web_middleware.aura";   Desc = "Web middleware" },
    @{ File = "test_web_dsl_full.aura";     Desc = "Web full DSL integration" },
    @{ File = "test_webview.aura";          Desc = "WebView widget" },
    @{ File = "test_media.aura";            Desc = "Media widget" },
    @{ File = "test_map.aura";              Desc = "Map widget" },
    @{ File = "test_formatter.aura";        Desc = "Code formatter" },
    @{ File = "test_linter.aura";           Desc = "Code linter" },
    @{ File = "test_debugger.aura";         Desc = "Debugger" },
    @{ File = "test_profiler.aura";         Desc = "Profiler" }
)

Push-Location $Root
foreach ($test in $webTests) {
    $src = Join-Path $webTestDir $test.File
    if (-not (Test-Path $src)) { Test-Result "$($test.Desc) - source not found" $false; continue }

    $outDir = Join-Path $Root "output/ir"
    if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir -ErrorAction SilentlyContinue }

    $compOut = & $Compiler $src 2>&1
    if ($LASTEXITCODE -ne 0) { if ($compOut) { $compOut | ForEach-Object { Write-Host "  $_" } }; Test-Result "$($test.Desc) - compile" $false; continue }

    $llFile = Get-ChildItem -Path $outDir -Filter "*.ll" | Select-Object -First 1
    if (-not $llFile) { Test-Result "$($test.Desc) - no .ll output" $false; continue }

    $verifyOut = & $OptPath -passes=verify -o nul $llFile.FullName 2>&1
    if ($LASTEXITCODE -ne 0) { if ($verifyOut) { $verifyOut | ForEach-Object { Write-Host "  opt: $_" } }; Test-Result $test.Desc $false; continue }
    Test-Result $test.Desc $true
}
Pop-Location

# Cleanup
if (Test-Path $tmpDir) { Remove-Item -Recurse -Force $tmpDir -ErrorAction SilentlyContinue }

# Summary
Write-Host "`n"
Write-Host ("=" * 55) -ForegroundColor Cyan
Write-Host "  REGRESSION TEST SUMMARY" -ForegroundColor Cyan
Write-Host ("=" * 55) -ForegroundColor Cyan
Write-Host "  Total:  $global:StageTotal" -ForegroundColor White
Write-Host "  Passed: $global:StagePass" -ForegroundColor Green
Write-Host "  Failed: $global:StageFail" -ForegroundColor Red
Write-Host ("=" * 55) -ForegroundColor Cyan

if ($global:StageFail -gt 0) { Write-Host "FAILED" -ForegroundColor Red; exit 1 }
else { Write-Host "ALL PASSED" -ForegroundColor Green; exit 0 }
