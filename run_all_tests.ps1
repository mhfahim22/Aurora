#!/usr/bin/env pwsh
# Aurora — Run All Test Suites
# Run from repository root: .\run_all_tests.ps1

$RepoRoot = $PSScriptRoot
$BuildDir = Join-Path $RepoRoot "_build"
$ReleaseDir = Join-Path $BuildDir "Release"
$Passed = 0
$Failed = 0
$Skipped = 0

Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║           Aurora Test Suite Runner                       ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

# ── Step 1: Build all targets ──
Write-Host "`n[1/3] Building all targets..." -ForegroundColor Yellow
cmake --build $BuildDir --config Release --target aurorac --target aurora_runtime --target voss --target aurora_lsp -j2 2>&1 | Out-Null
if (-not $?) { Write-Host "  ❌ Build failed" -ForegroundColor Red; exit 1 }

cmake --build $BuildDir --config Release -j2 2>&1 | Out-Null
if (-not $?) { Write-Host "  ⚠️ Some optional targets may have been skipped" -ForegroundColor Yellow }
Write-Host "  ✅ Build complete" -ForegroundColor Green

# ── Step 2: Run all test executables ──
Write-Host "`n[2/3] Running tests..." -ForegroundColor Yellow

$TestList = @(
    @{Name="test_optimizer"; Args=@()},
    @{Name="test_leak_detector"; Args=@()},
    @{Name="test_smart_ptr"; Args=@()},
    @{Name="test_ffi_abi"; Args=@()},
    @{Name="test_ffi_abi_extra"; Args=@()},
    @{Name="test_ffi_abi_edge"; Args=@()},
    @{Name="test_ffi_abi_struct"; Args=@()},
    @{Name="test_unified_type_system"; Args=@()},
    @{Name="test_ffi_memory_safety"; Args=@()},
    @{Name="test_universal_bridge"; Args=@()},
    @{Name="test_integration_http"; Args=@()},
    @{Name="test_tls_schannel"; Args=@()},
    @{Name="test_bridge_e2e"; Args=@()},
    @{Name="test_pypi_thread_safety"; Args=@()},
    @{Name="test_fuzz_parser"; Args=@("1000")},
    @{Name="bench_npm_bridge"; Args=@()},
    @{Name="bench_native_speed"; Args=@()},
    @{Name="bench_fast"; Args=@()}
)

Push-Location $ReleaseDir
foreach ($t in $TestList) {
    $exe = Join-Path $ReleaseDir "$($t.Name).exe"
    if (-not (Test-Path -LiteralPath $exe)) {
        Write-Host "  ⚠️ $($t.Name) — not built, skipping" -ForegroundColor Yellow
        $Skipped++
        continue
    }
    Write-Host "  Running $($t.Name)..." -NoNewline
    $output = & $exe $t.Args 2>&1 | Out-String
    if ($LASTEXITCODE -eq 0) {
        Write-Host " ✅" -ForegroundColor Green
        $Passed++
    } else {
        Write-Host " ❌ (exit $LASTEXITCODE)" -ForegroundColor Red
        $Failed++
        Write-Host "  Output:" -ForegroundColor Red
        $output -split "`n" | Select-Object -Last 20 | ForEach-Object { Write-Host "    $_" }
    }
}
Pop-Location

# ── Step 3: Run .aura test files ──
Write-Host "`n[3/3] Running .aura language tests..." -ForegroundColor Yellow
$AuraTests = Get-ChildItem -Path (Join-Path $RepoRoot "aurora\tests") -Recurse -Filter "*.aura" | Where-Object { $_.Name -like "test_*" }
$AuraPassed = 0; $AuraFailed = 0
foreach ($test in $AuraTests) {
    Write-Host "  $($test.Name)..." -NoNewline
    $output = & (Join-Path $ReleaseDir "aurorac.exe") $test.FullName "--emit-obj" 2>&1 | Out-String
    if ($LASTEXITCODE -eq 0) {
        Write-Host " ✅" -ForegroundColor Green
        $AuraPassed++
    } else {
        Write-Host " ❌" -ForegroundColor Red
        $AuraFailed++
    }
}

# ── Summary ──
Write-Host "`n╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                     Results Summary                       ║" -ForegroundColor Cyan
Write-Host "╠═══════════════════════════════════════════════════════════╣" -ForegroundColor Cyan
Write-Host "║  C++ tests:   $($Passed) passed, $($Failed) failed, $($Skipped) skipped" -ForegroundColor Cyan
Write-Host "║  .aura tests: $($AuraPassed) passed, $($AuraFailed) failed" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

exit ($Failed + $AuraFailed)
