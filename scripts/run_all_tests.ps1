param(
    [string]$Compiler = "build/Release/aurorac.exe"
)

$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$CompilerPath = Join-Path $Root $Compiler

if (!(Test-Path $CompilerPath)) {
    Write-Host "ERROR: Compiler not found at $CompilerPath" -ForegroundColor Red
    exit 1
}

$TestDirs = @(
    "aurora/tests/core",
    "aurora/tests/lexer",
    "aurora/tests/parser",
    "aurora/tests/Stress",
    "aurora/tests/domains"
)

$TotalPass = 0
$TotalFail = 0
$FailedFiles = @()

foreach ($Dir in $TestDirs) {
    $FullDir = Join-Path $Root $Dir
    if (!(Test-Path $FullDir)) { continue }
    $TestFiles = Get-ChildItem -Path $FullDir -Filter "*.aura"
    foreach ($File in $TestFiles) {
        $RelPath = $File.FullName.Substring($Root.Length + 1)
        Write-Host -NoNewline "  $RelPath ... "
        $Output = & $CompilerPath "`"$($File.FullName)`"" --emit-obj 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "PASS" -ForegroundColor Green
            $TotalPass++
        } else {
            Write-Host "FAIL" -ForegroundColor Red
            $TotalFail++
            $FailedFiles += $RelPath
        }
    }
}

Write-Host ""
Write-Host "==================================="
Write-Host "  Results: $TotalPass passed, $TotalFail failed"
Write-Host "==================================="
if ($FailedFiles.Count -gt 0) {
    Write-Host "Failed files:" -ForegroundColor Red
    foreach ($f in $FailedFiles) { Write-Host "  - $f" -ForegroundColor Red }
}
exit $TotalFail
