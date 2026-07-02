#!/usr/bin/env pwsh
param(
    [string]$BuildDir = "build/Release",
    [string]$OptPath = "C:\LLVM\bin\opt.exe"
)

$Root = Split-Path -Parent $PSScriptRoot
$Compiler = Join-Path (Join-Path $Root $BuildDir) "aurorac.exe"
$RuntimeLib = Join-Path (Join-Path $Root $BuildDir) "aurora_runtime.lib"
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
if (-not (Test-Path $OptPath)) { Write-Host "FAIL: opt not found: $OptPath" -ForegroundColor Red; $preOk = $false }
if (-not (Test-Path $TestSrcDir)) { Write-Host "FAIL: Test src dir missing" -ForegroundColor Red; $preOk = $false }
if (-not (Test-Path $ExamplesDir)) { Write-Host "FAIL: Examples dir missing" -ForegroundColor Red; $preOk = $false }
if (-not $preOk) { Write-Host "Pre-checks failed" -ForegroundColor Red; exit 1 }
Test-Result "Pre-checks" $true

# Stage 1: CTest
Write-Step "Stage 1: CTest (runtime tests)"
Push-Location (Join-Path $Root "build")
$ctestOut = & ctest --output-on-failure -C Release 2>&1
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
    if ($LASTEXITCODE -ne 0) { Test-Result "$($test.Desc) - compile" $false; continue }

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
    @{ File = "test_generics.aura";  Desc = "Generics identity[T]" }
)

Push-Location $Root
foreach ($test in $stdlibTests) {
    $src = Join-Path $TestSrcDir $test.File
    if (-not (Test-Path $src)) { Test-Result "$($test.Desc) - source not found" $false; continue }

    $outDir = Join-Path $Root "output/ir"
    if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir -ErrorAction SilentlyContinue }

    $compOut = & $Compiler $src 2>&1
    if ($LASTEXITCODE -ne 0) { Test-Result "$($test.Desc) - compile" $false; continue }

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
