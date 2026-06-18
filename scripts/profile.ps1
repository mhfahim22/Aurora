#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Aurora Profiling Harness — automated performance run across all 5 domains
.DESCRIPTION
    Runs benchmarks for each Phase and reports timing results.
    Usage: .\scripts\profile.ps1 [-OutputDir <path>]
#>
param(
    [string]$OutputDir = "profile_results"
)

$ErrorActionPreference = "Continue"
$startTime = Get-Date

if (!(Test-Path -LiteralPath $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

$results = @()
$logFile = Join-Path $OutputDir "profile_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"

function Write-Log {
    param([string]$Message)
    $timestamp = Get-Date -Format "HH:mm:ss.fff"
    $line = "[$timestamp] $Message"
    Write-Host $line
    Add-Content -Path $logFile -Value $line
}

function Invoke-Benchmark {
    param(
        [string]$Name,
        [string]$Command,
        [string]$WorkingDir = (Get-Location)
    )
    Write-Log "╔═══ $Name"
    Write-Log "║     $Command"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        $output = & $Command 2>&1 | Out-String
        $sw.Stop()
        $elapsed = $sw.Elapsed.TotalMilliseconds
        $result = "PASS"
        Write-Log "║     Elapsed: $($elapsed.ToString('F1')) ms"
        if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne $null) {
            $result = "FAIL (exit $LASTEXITCODE)"
        }
    } catch {
        $sw.Stop()
        $elapsed = $sw.Elapsed.TotalMilliseconds
        $result = "FAIL ($($_.Exception.Message))"
        Write-Log "║     ERROR: $($_)"
    }
    Write-Log "╚═══ Result: $result"

    $global:results += [PSCustomObject]@{
        Name     = $Name
        ElapsedMs = [math]::Round($elapsed, 1)
        Result   = $result
    }
}

$buildDir = Join-Path (Get-Location) "build"
$releaseDir = Join-Path $buildDir "Release"

# ── Detect available benchmarks ──
$benchAi = Join-Path $releaseDir "bench_ai.exe"
$benchBridge = Join-Path $releaseDir "bench_bridge.exe"
$benchCompiler = Join-Path $releaseDir "bench_compiler.exe"
$benchNative = Join-Path $releaseDir "bench_native_speed.exe"
$vossExe = Join-Path $releaseDir "voss.exe"

Write-Log "╔════════════════════════════════════════════════════════╗"
Write-Log "║   Aurora Profiling Harness                           ║"
Write-Log "║   Started: $startTime"
Write-Log "╚════════════════════════════════════════════════════════╝"
Write-Log ""

# ── Phase 1: Backend HTTP Server ──
Write-Log "─── Phase 1: Backend HTTP Server ───"
$todoServer = Join-Path (Get-Location) "examples" "todo_server.aura"
if (Test-Path -LiteralPath $todoServer) {
    Invoke-Benchmark -Name "Phase 1: Todo server parse" -Command "aurorac.exe `"$todoServer`" --dump-ir" -WorkingDir $buildDir
}

# ── Phase 2: 3D Graphics ──
Write-Log "─── Phase 2: 3D Graphics ───"
$examples = @("cube", "texture", "lighting", "camera", "model", "fps_camera")
$ex3dDir = Join-Path (Get-Location) "examples" "3d"
foreach ($ex in $examples) {
    $path = Join-Path $ex3dDir "$ex.aura"
    if (Test-Path -LiteralPath $path) {
        Invoke-Benchmark -Name "Phase 2: $ex.aura (parse)" -Command "aurorac.exe `"$path`" --dump-ir" -WorkingDir $buildDir
    }
}

# ── Phase 3: GUI ──
Write-Log "─── Phase 3: Desktop GUI ───"
$guiDemo = Join-Path (Get-Location) "examples" "gui_note.aura"
if (Test-Path -LiteralPath $guiDemo) {
    Invoke-Benchmark -Name "Phase 3: gui_note.aura (parse)" -Command "aurorac.exe `"$guiDemo`" --dump-ir" -WorkingDir $buildDir
}

# ── Phase 4: AI Benchmark ──
Write-Log "─── Phase 4: AI/ML Performance ───"
if (Test-Path -LiteralPath $benchAi) {
    Invoke-Benchmark -Name "Phase 4: bench_ai.exe" -Command $benchAi -WorkingDir $buildDir
}
$aiMnist = Join-Path (Get-Location) "examples" "ai_mnist.aura"
if (Test-Path -LiteralPath $aiMnist) {
    Invoke-Benchmark -Name "Phase 4: ai_mnist.aura (parse)" -Command "aurorac.exe `"$aiMnist`" --dump-ir" -WorkingDir $buildDir
}

# ── Phase 5: Bridge Benchmark ──
Write-Log "─── Phase 5: Polyglot Bridge ───"
if (Test-Path -LiteralPath $benchBridge) {
    Invoke-Benchmark -Name "Phase 5: bench_bridge.exe" -Command $benchBridge -WorkingDir $buildDir
}
if (Test-Path -LiteralPath $vossExe) {
    Invoke-Benchmark -Name "Phase 5: voss bridge (pypi test)" -Command "`"$vossExe`" bridge pypi test 1.0.0" -WorkingDir $buildDir
    Remove-Item -Recurse -Force (Join-Path $buildDir "test_pypi") -ErrorAction Ignore
}

# ── Phase 6: Integration examples ──
Write-Log "─── Phase 6: Integration Examples ───"
$phase6Examples = @("todo_full", "chat", "ai_classifier", "poly_pipeline")
$exDir = Join-Path (Get-Location) "examples"
foreach ($ex in $phase6Examples) {
    $path = Join-Path $exDir "$ex.aura"
    if (Test-Path -LiteralPath $path) {
        Invoke-Benchmark -Name "Phase 6: $ex.aura (parse)" -Command "aurorac.exe `"$path`" --dump-ir" -WorkingDir $buildDir
    }
}

# ── Compiler benchmarks ──
Write-Log "─── Compiler Benchmarks ───"
if (Test-Path -LiteralPath $benchCompiler) {
    Invoke-Benchmark -Name "compiler bench" -Command $benchCompiler -WorkingDir $buildDir
}
if (Test-Path -LiteralPath $benchNative) {
    Invoke-Benchmark -Name "native speed bench" -Command $benchNative -WorkingDir $buildDir
}

# ── Summary ──
$endTime = Get-Date
$totalSec = [math]::Round(($endTime - $startTime).TotalSeconds, 1)
Write-Log ""
Write-Log "╔════════════════════════════════════════════════════════╗"
Write-Log "║   Profile Complete                                   ║"
Write-Log "║   Total time: $totalSec seconds"
Write-Log "║   Log: $logFile"
Write-Log "╚════════════════════════════════════════════════════════╝"

$csvPath = Join-Path $OutputDir "results.csv"
$results | Export-Csv -Path $csvPath -NoTypeInformation
Write-Log "Results saved to: $csvPath"

# Print summary table
Write-Host ""
Write-Host "Summary:" -ForegroundColor Cyan
$results | Format-Table Name, ElapsedMs, Result -AutoSize | Out-Host

$passed = ($results | Where-Object { $_.Result -eq "PASS" }).Count
$total = $results.Count
Write-Host "Passed: $passed/$total" -ForegroundColor $(if ($passed -eq $total) { "Green" } else { "Yellow" })
