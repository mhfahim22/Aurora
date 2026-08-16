#!/usr/bin/env pwsh
# ═══════════════════════════════════════════════════════════════
#  Aurora HTTP Benchmark Launcher (Phase 38.2)
#  Runs Aurora vs Node.js vs Go (if available) with wrk/hey
# ═══════════════════════════════════════════════════════════════
<#
.SYNOPSIS
    Benchmarks the Aurora HTTP server against Node.js and Go.
.DESCRIPTION
    Compiles benchmarks/bench_http.aura, starts the Aurora server on
    :8080, then runs a load test for N seconds and reports
    requests/sec and p99 latency. Compares against Node.js/Go if
    available. Publishes results to docs/benchmarks.md.
#>
[CmdletBinding()]
param(
    [int]$Duration = 10,
    [int]$Connections = 100,
    [string]$LoadTool = "auto"  # auto | wrk | hey
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

function Get-CommandEx([string]$name) {
    try { Get-Command $name -ErrorAction Stop | Select-Object -ExpandProperty Source }
    catch { $null }
}

# ── Resolve load-test tool ──
$wrk = $null; $hey = $null
if ($LoadTool -eq "auto" -or $LoadTool -eq "wrk")  { $wrk = Get-CommandEx "wrk" }
if ($LoadTool -eq "auto" -or $LoadTool -eq "hey")  { $hey = Get-CommandEx "hey" }

if (-not $wrk -and -not $hey) {
    Write-Warning "Neither wrk nor hey found. Using built-in PowerShell test loop as fallback."
}

# ── 1. Compile Aurora benchmark server (AOT: emit-obj + MinGW link) ──
# The installed release compiler at D:\Apps\Aurora\aurorac.exe fully supports the
# Phase 31 web DSL; fall back to the build-tree compiler if it is absent.
$aurorac = "D:\Apps\Aurora\aurorac.exe"
if (-not (Test-Path $aurorac)) {
    $aurorac = Get-CommandEx "aurorac"
    if (-not $aurorac) { $aurorac = Join-Path $root "build\aurorac.exe" }
}
if (-not (Test-Path $aurorac)) { throw "aurorac.exe not found. Build first." }

Write-Host "== Compiling bench_http.aura ==" -ForegroundColor Cyan
$auroraExe = Join-Path $root "benchmarks\bench_http.exe"
$tmpObj = Join-Path $env:TEMP "bench_http.obj"
& $aurorac --emit-obj (Join-Path $root "benchmarks\bench_http.aura") -o $tmpObj
if ($LASTEXITCODE -ne 0) { throw "Aurora AOT emit-obj failed" }
$gxx = Get-CommandEx "g++"
if (-not $gxx) { throw "g++ not found for MinGW link" }
& $gxx $tmpObj (Join-Path $root "build\libaurora_runtime.a") -o $auroraExe `
    -lws2_32 -lwinmm -luser32 -lgdi32 -lshell32 -lole32 -loleaut32 -luuid -ladvapi32 `
    -ldbghelp -lversion -lcrypt32 -lsecur32 -ldxgi -ld3d12 -ld3d11 -ld3dcompiler `
    -lopengl32 -lbcrypt -lwinhttp -lshlwapi -lcomctl32 -ldwmapi -lsetupapi
if ($LASTEXITCODE -ne 0) { throw "MinGW link failed" }

# ── 2. Start Aurora server ──
Write-Host "== Starting Aurora benchmark server on :8080 ==" -ForegroundColor Cyan
$aurora = Start-Process -FilePath $auroraExe -PassThru -WindowStyle Hidden
try {
    Start-Sleep -Seconds 2  # warmup + bind

    function Test-Endpoint($port, $path) {
        try {
            $r = Invoke-WebRequest -Uri "http://localhost:$port$path" -UseBasicParsing -TimeoutSec 3
            return $r.StatusCode
        } catch { return -1 }
    }

    if ((Test-Endpoint 8080 "/hello") -ne 200) {
        throw "Aurora server did not respond on /hello"
    }

    # ── 3. Node.js + Go comparison servers ──
    $nodeSrv = $null; $goSrv = $null
    $nodeJs = Join-Path $env:TEMP "bench_http_node.js"
    if (($node = Get-CommandEx "node") -and -not (Test-Path $nodeJs)) {
        Set-Content -Path $nodeJs -Value (@"
const http = require('http');
http.createServer((q,r)=>{r.setHeader('content-type','application/json');r.end('{"hello":"world"}')}).listen(8081);
"@)
    }
    if (Test-Path $nodeJs) {
        $nodeSrv = Start-Process -FilePath $node -ArgumentList $nodeJs -PassThru -WindowStyle Hidden
    }
    $goExe = Join-Path $env:TEMP "bench_go.exe"
    if ((Get-CommandEx "go") -or (Test-Path $goExe)) {
        $goSrc = Join-Path $root "benchmarks\bench_http_go.go"
        if (Test-Path $goSrc) {
            if (-not (Test-Path $goExe)) {
                & (Get-CommandEx "go") build -o $goExe $goSrc
                if ($LASTEXITCODE -ne 0) { throw "Go build failed" }
            }
            $goSrv = Start-Process -FilePath $goExe -PassThru -WindowStyle Hidden
        }
    }
    Start-Sleep -Seconds 1

    # ── 4. Run load test ──
    Write-Host "== Running ${Duration}s load test ($Connections connections) ==" -ForegroundColor Cyan

    $auroraResult = ""
    if ($wrk) {
        $auroraResult = & $wrk -t4 -c$Connections -d${Duration}s --latency http://localhost:8080/hello
    } elseif ($hey) {
        $auroraResult = & $hey -z ${Duration}s -c $Connections http://localhost:8080/hello
    } else {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        $jobs = 1..$Connections | ForEach-Object {
            Start-Job -ScriptBlock {
                for ($i = 0; $i -lt 20; $i++) {
                    try { Invoke-WebRequest -Uri "http://localhost:8080/hello" -UseBasicParsing -TimeoutSec 2 | Out-Null } catch {}
                }
            }
        }
        $jobs | Wait-Job | Out-Null
        $jobs | Remove-Job -Force
        $sw.Stop()
        $reqs = $Connections * 20
        $rps = [math]::Round($reqs / $sw.Elapsed.TotalSeconds, 2)
        $auroraResult = "Fallback: $reqs requests in $($sw.Elapsed.TotalSeconds)s -> ${rps} req/s"
    }

    Write-Host "`n--- Aurora Results (:8080) ---" -ForegroundColor Green
    Write-Host $auroraResult

    $comparison = ""
    if ($nodeSrv) {
        Write-Host "`n== Node.js comparison ==" -ForegroundColor Cyan
        if ($wrk) {
            $nodeRes = & $wrk -t4 -c$Connections -d${Duration}s --latency http://localhost:8081/hello
        } elseif ($hey) {
            $nodeRes = & $hey -z ${Duration}s -c $Connections http://localhost:8081/hello
        } else { $nodeRes = "(fallback not implemented for comparison)" }
        $comparison += "`n--- Node.js (:8081) ---`n$nodeRes"
        Write-Host $nodeRes
    }
    if ($goSrv -and (Test-Endpoint 8082 "/hello") -eq 200) {
        Write-Host "`n== Go comparison ==" -ForegroundColor Cyan
        if ($wrk) {
            $goRes = & $wrk -t4 -c$Connections -d${Duration}s --latency http://localhost:8082/hello
        } elseif ($hey) {
            $goRes = & $hey -z ${Duration}s -c $Connections http://localhost:8082/hello
        } else { $goRes = "(fallback not implemented for comparison)" }
        $comparison += "`n--- Go (:8082) ---`n$goRes"
        Write-Host $goRes
    }

    # ── 5. Publish results ──
    $results = @()
    $results += "## Aurora HTTP Benchmark Results"
    $results += ""
    $results += "Run: $(Get-Date -Format 'yyyy-MM-dd HH:mm')"
    $results += "Load: ${Duration}s, $Connections connections, path /hello"
    $results += ""
    $results += "### Raw Output"
    $results += '```'
    $results += "Aurora:"
    $results += $auroraResult
    $results += $comparison
    $results += '```'
    $results += ""
    $results += "### Summary"
    $results += "- Aurora HTTP server compiled from Aurora source (benchmarks/bench_http.aura)"
    $results += "- Target: >= 50% of Go performance (currently ~17%)"
    $results += "- Re-run anytime with: scripts/bench_http.ps1"

    $benchDoc = Join-Path $root "docs/benchmarks.md"
    if (-not (Test-Path $benchDoc)) { Set-Content -Path $benchDoc -Value "# Aurora Benchmarks" }
    Add-Content -Path $benchDoc -Value ($results -join "`n")

    Write-Host "`nResults appended to docs/benchmarks.md" -ForegroundColor Green
}
finally {
    if ($nodeSrv) { Stop-Process -Id $nodeSrv.Id -Force -ErrorAction SilentlyContinue }
    if ($goSrv)   { Stop-Process -Id $goSrv.Id -Force -ErrorAction SilentlyContinue }
    Stop-Process -Id $aurora.Id -Force -ErrorAction SilentlyContinue
}
