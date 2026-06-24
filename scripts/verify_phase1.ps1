# ═══════════════════════════════════════════════════════════════
# Phase 1 Verification — Core Language Completeness
# ═══════════════════════════════════════════════════════════════
$RepoRoot = Split-Path $PSScriptRoot -Parent
$Compiler = Join-Path $RepoRoot "build\Release\aurorac.exe"
$TestDir = Join-Path $RepoRoot "aurora\tests\core"

$Phase1Tests = @(
    "test_struct_decl",
    "test_enum_decl",
    "test_interface_decl",
    "test_typealias",
    "test_match_comprehensive",
    "test_match",
    "test_switch",
    "test_abstract",
    "test_encap_pass",
    "test_encapsulation",
    "test_methods",
    "test_methods2",
    "test_methods3",
    "test_polymorphism"
)

Write-Host "╔════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  Phase 1 Verification — Core Language     ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$Pass = 0; $Fail = 0
foreach ($t in $Phase1Tests) {
    $src = Join-Path $TestDir "$t.aura"
    if (-not (Test-Path $src)) {
        Write-Host "  SKIP: $t (file not found)" -ForegroundColor Yellow
        continue
    }
    $exe = Join-Path $RepoRoot "build\Release\v_$t.exe"
    Write-Host "  Compiling $t.aura ... " -NoNewline
    $output = & $Compiler $src --emit-obj -o $exe 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "PASS" -ForegroundColor Green
        $Pass++
    } else {
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host "    $output"
        $Fail++
    }
}

Write-Host ""
Write-Host "╔════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  Results                                  ║" -ForegroundColor Cyan
Write-Host "╠════════════════════════════════════════════╣"
Write-Host ("║  PASS: $Pass  |  FAIL: $Fail  |  Total: $(($Pass+$Fail))") -ForegroundColor $(if ($Fail -eq 0) { "Green" } else { "Red" })
Write-Host "╚════════════════════════════════════════════╝" -ForegroundColor Cyan
exit $Fail
