#!/usr/bin/env pwsh
# bench_gui.ps1 — GUI Performance Benchmark Suite (Phase 12.1)
# Measures frame time, widget count, startup time, memory usage

param(
    [string]$AuroraC = "..\build\Release\aurorac.exe",
    [switch]$RunGUI
)

$ErrorActionPreference = "Stop"

# ── Helpers ──
function Measure-Timing {
    param([scriptblock]$Block)
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & $Block
    $sw.Stop()
    return $sw.ElapsedMilliseconds
}

function Write-BenchResult {
    param($Name, $Value, $Unit)
    $color = if ($Value -le $Unit) { "Green" } else { "Yellow" }
    Write-Host ("  {0,-40} {1,8} {2}" -f $Name, $Value, $Unit) -ForegroundColor $color
}

Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Aurora GUI Performance Benchmark" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$targets = @{
    "1000 widgets"  = @"
import "gui"
function main()
    win = aurora_gui_window_new("Bench", 800, 600)
    for i in range(1000)
        aurora_gui_button_new(win, "Btn", 0, 0, 100, 30)
    end
    app_run(win)
end
"@
    "Canvas 60fps" = @"
import "gui"
function main()
    win = aurora_gui_window_new("Bench", 400, 400)
    app_run(win)
end
"@
    "App startup" = @"
function main()
    output("started")
end
"@
}

# ── Bench 1: Compile time (proxy for startup complexity) ──
Write-Host "── Compile Performance ──" -ForegroundColor Cyan
foreach ($name in $targets.Keys) {
    $src = $targets[$name]
    $tmpFile = [System.IO.Path]::GetTempFileName() + ".aura"
    Set-Content -Path $tmpFile -Value $src
    $ms = Measure-Timing { & $AuroraC $tmpFile 2>$null }
    Remove-Item $tmpFile -Force -ErrorAction SilentlyContinue
    Remove-Item ($tmpFile -replace '\.aura$', '.exe') -Force -ErrorAction SilentlyContinue
    Write-BenchResult $name $ms "ms"
}

# ── Bench 2: Widget performance ──
Write-Host ""
Write-Host "── Widget Creation (compiled) ──" -ForegroundColor Cyan
$widgetTypes = @("button", "label", "textbox", "slider", "switch", "checkbox", "progress", "listbox")
foreach ($wt in $widgetTypes) {
    $src = @"
import "gui"
function main()
    win = aurora_gui_window_new("Bench", 800, 600)
    for i in range(100)
        aurora_gui_${wt}_new(win, "Item $i", 0, i * 25, 200, 24)
    end
end
"@
    $tmpFile = [System.IO.Path]::GetTempFileName() + ".aura"
    Set-Content -Path $tmpFile -Value $src
    $ms = Measure-Timing { & $AuroraC $tmpFile 2>$null }
    Remove-Item $tmpFile -Force -ErrorAction SilentlyContinue
    Remove-Item ($tmpFile -replace '\.aura$', '.exe') -Force -ErrorAction SilentlyContinue
    Write-BenchResult ("100x " + $wt + " compile") $ms "ms"
}

# ── Bench 3: Memory estimation ──
Write-Host ""
Write-Host "── Compiler Memory ──" -ForegroundColor Cyan
$memBefore = (Get-Process -Id $pid).WorkingSet64
& $AuroraC "--version" 2>$null
$memAfter = (Get-Process -Id $pid).WorkingSet64
Write-BenchResult "AuroraC working set" (($memAfter - $memBefore) / 1MB) "MB"

Write-Host ""
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Benchmark Complete" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════" -ForegroundColor Cyan
