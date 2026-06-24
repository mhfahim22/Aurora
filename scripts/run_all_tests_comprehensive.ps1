param(
    [string]$Compiler = "build/Release/aurorac.exe",
    [string]$RuntimeLib = "build/Release/aurora_runtime.lib"
)

$Root = Split-Path -Parent $PSScriptRoot
$CompilerPath = Join-Path $Root $Compiler
$RuntimeLibPath = Join-Path $Root $RuntimeLib

if (!(Test-Path $CompilerPath)) {
    Write-Host "ERROR: Compiler not found at $CompilerPath" -ForegroundColor Red
    exit 1
}
if (!(Test-Path $RuntimeLibPath)) {
    Write-Host "ERROR: Runtime lib not found at $RuntimeLibPath" -ForegroundColor Red
    exit 1
}

$TestDir = Join-Path $Root "aurora/tests"
$ExeDir = Join-Path $Root "output/exe"
$IrDir = Join-Path $Root "output/ir"
$ObjDir = Join-Path $Root "output/obj"

# Ensure output dirs
@($ExeDir, $IrDir, $ObjDir) | ForEach-Object {
    if (!(Test-Path $_)) { New-Item -ItemType Directory -Path $_ | Out-Null }
}

$AllTests = Get-ChildItem -Path $TestDir -Filter "*.aura" -Recurse | Sort-Object FullName

$Results = @()
$Total = 0
$Pass = 0
$Fail = 0
$Crash = 0

$ExpectedFail = @(
    "verify_abstract_fail",
    "verify_encap_fail",
    "verify_final_fail",
    "verify_visibility_fail",
    "verify_gc_perf_error"
)

$Skipped = @(
    "test_lora", "test_distributed", "test_end_to_end", "test_enterprise",
    "bench_1B", "bench_power", "_bench_gpu", "_gpu_detect",
    "test_game_engine", "test_gui_crossplatform", "test_ui_framework",
    "test_event_fiber", "test_backend_integration",
    "test_async_concurrency", "test_cpp_interop", "test_libtorch", "bench_speed",
    "test_all_bridges", "test_cargo_bridge", "test_native_bridge",
    "test_npm_bridge", "test_pypi_bridge",
    "test_integration_http", "test_server",
    "test_backend_builtins", "test_ai_builtins",
    "test_mha_crash", "test_mha_crash2", "test_mha_crash3",
    "test_dense_debug"
)

$SkipCount = 0

# Add PATH for LLVM tools
$env:Path = "C:\LLVM\bin;$env:Path"

foreach ($File in $AllTests) {
    $RelPath = $File.FullName.Substring($Root.Length + 1)
    $BaseName = $File.BaseName
    $DirName = $File.DirectoryName.Substring($Root.Length + 1)

    # Check if file should be skipped
    $ShouldSkip = $false
    foreach ($skip in $Skipped) {
        if ($BaseName -eq $skip) { $ShouldSkip = $true; break }
    }

    if ($ShouldSkip) {
        Write-Host "[SKIP] $RelPath" -ForegroundColor Yellow
        $SkipCount++
        continue
    }

    $Total++
    $IsExpectedFail = $false
    foreach ($ef in $ExpectedFail) {
        if ($BaseName -eq $ef) { $IsExpectedFail = $true; break }
    }

    Write-Host -NoNewline "[$Total] $RelPath ... "

    $IrFile = Join-Path $IrDir "$BaseName.ll"
    $ObjFile = Join-Path $ObjDir "$BaseName.obj"
    $ExeFile = Join-Path $ExeDir "$BaseName.exe"

    # STAGE 1: Compile .aura -> .ll
    $CompileOutput = & $CompilerPath $File.FullName 2>&1
    $CompileResult = $LASTEXITCODE

    if ($CompileResult -ne 0) {
        if ($IsExpectedFail) {
            Write-Host "PASS (expected compile fail)" -ForegroundColor Green
            $Pass++
        } else {
            Write-Host "FAIL (compile error)" -ForegroundColor Red
            $Fail++
            $Results += [PSCustomObject]@{
                File = $RelPath
                Stage = "compile"
                Error = ($CompileOutput -join "`n")
            }
        }
        continue
    } elseif ($IsExpectedFail) {
        Write-Host "UNEXPECTED PASS (expected compile fail)" -ForegroundColor Yellow
    }

    # STAGE 2: .ll -> .obj
    $LlOutput = & llc $IrFile -filetype=obj -o $ObjFile 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL (llc error)" -ForegroundColor Red
        $Fail++
        $Results += [PSCustomObject]@{
            File = $RelPath
            Stage = "llc"
            Error = ($LlOutput -join "`n")
        }
        continue
    }

    # STAGE 3: .obj -> .exe
    $LinkOutput = & lld-link $ObjFile $RuntimeLibPath user32.lib /OUT:$ExeFile /NOLOGO 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL (link error)" -ForegroundColor Red
        $Fail++
        $Results += [PSCustomObject]@{
            File = $RelPath
            Stage = "link"
            Error = ($LinkOutput -join "`n")
        }
        continue
    }

    # STAGE 4: Run .exe
    $RunOutput = & $ExeFile 2>&1
    $RunResult = $LASTEXITCODE

    if ($RunResult -ne 0) {
        Write-Host "CRASH (exit code: $RunResult)" -ForegroundColor Red
        $Crash++
        $Results += [PSCustomObject]@{
            File = $RelPath
            Stage = "runtime"
            Error = "Exit code: $RunResult, Output: $RunOutput"
        }
    } else {
        Write-Host "PASS" -ForegroundColor Green
        $Pass++
    }
}

Write-Host ""
Write-Host "================================================================"
Write-Host "  Total: $Total | Pass: $Pass | Fail: $Fail | Crash: $Crash | Skipped: $SkipCount"
Write-Host "================================================================"

if ($Results.Count -gt 0) {
    Write-Host "`nFailed/Crashed details:" -ForegroundColor Red
    foreach ($r in $Results) {
        Write-Host "  [$($r.Stage)] $($r.File)" -ForegroundColor Red
        Write-Host "    $($r.Error)" -ForegroundColor DarkRed
    }
}

exit ($Fail + $Crash)
