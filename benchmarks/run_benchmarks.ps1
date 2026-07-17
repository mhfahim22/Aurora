param(
    [string]$BenchDir = "D:\Downloads\aurora_restructured\benchmarks",
    [string]$Aurorac = "D:\Downloads\aurora_restructured\build_release\Release\aurorac.exe",
    [string]$RuntimeLib = "D:\Downloads\aurora_restructured\build_aot\Release\aurora_runtime.lib",
    [string]$LLD = "C:\LLVM\bin\lld-link.exe"
)

$Benchmarks = @(
    "bench_string.aura",
    "bench_json.aura",
    "bench_file.aura",
    "bench_matmul.aura",
    "bench_sort.aura"
)

$Results = @{}

foreach ($bm in $Benchmarks) {
    $base = [System.IO.Path]::GetFileNameWithoutExtension($bm)
    $src = Join-Path $BenchDir $bm
    $obj = Join-Path $BenchDir "$base.obj"
    $exe = Join-Path $BenchDir "$base.exe"

    Write-Host "=== $base ===" -ForegroundColor Cyan
    
    # Delete old files
    if (Test-Path $obj) { Remove-Item $obj -Force }
    if (Test-Path $exe) { Remove-Item $exe -Force }

    # Compile
    Write-Host "  Compiling..."
    $output = & $Aurorac --build $src -o $exe 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0 -and -not (Test-Path $obj)) {
        # If --build failed to produce obj, try just compiling to obj
        Write-Host "  --build failed, trying --emit-obj..."
        $output = & $Aurorac $src --emit-obj -o $obj 2>&1 | Out-String
    }
    
    if (Test-Path $obj) {
        Write-Host "  Linking..."
        & $LLD $obj "/OUT:$exe" "/SUBSYSTEM:CONSOLE" "/NOLOGO" "/DEFAULTLIB:msvcrt" $RuntimeLib 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  LINK FAILED" -ForegroundColor Red
            $Results[$base] = "LINK FAILED"
            continue
        }
        
        if (Test-Path $exe) {
            Write-Host "  Running..." -ForegroundColor Yellow
            $sw = [System.Diagnostics.Stopwatch]::StartNew()
            $runOutput = & $exe 2>&1 | Out-String
            $sw.Stop()
            Write-Host $runOutput -NoNewline
            Write-Host "  (startup + run: $($sw.ElapsedMilliseconds) ms)" -ForegroundColor Gray
            $Results[$base] = $runOutput.Trim()
        } else {
            Write-Host "  EXE NOT CREATED" -ForegroundColor Red
            $Results[$base] = "EXE NOT CREATED"
        }
    } else {
        Write-Host "  COMPILE FAILED" -ForegroundColor Red
        Write-Host $output
        $Results[$base] = "COMPILE FAILED"
    }
    
    Write-Host ""
}

Write-Host "═" * 60 -ForegroundColor Green
Write-Host "BENCHMARK RESULTS" -ForegroundColor Green
Write-Host "═" * 60 -ForegroundColor Green
foreach ($kv in $Results.GetEnumerator()) {
    Write-Host "$($kv.Key): $($kv.Value)" -ForegroundColor White
}
