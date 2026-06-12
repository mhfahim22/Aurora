param(
    [switch]$Silent
)

$FontName = "JetBrains Mono"
$FontDir = "$env:LOCALAPPDATA\Microsoft\Windows\Fonts"
$RegPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Fonts"
$TempZip = "$env:TEMP\jetbrains-mono.zip"
$ExtractDir = "$env:TEMP\jetbrains-mono"

# ── Check if already installed ──
$installed = Get-ItemProperty -Path $RegPath -Name "*JetBrains Mono*" -ErrorAction SilentlyContinue
if ($installed) {
    if (-not $Silent) { Write-Host "✓ JetBrains Mono already installed" -ForegroundColor Green }
    return $true
}

# Font dir may not exist
if (-not (Test-Path $FontDir)) { New-Item -ItemType Directory -Path $FontDir -Force | Out-Null }

# ── Download JetBrains Mono from GitHub ──
$Url = "https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip"
if (-not $Silent) {
    Write-Host "Downloading JetBrains Mono..." -ForegroundColor Cyan
}
try {
    $wc = New-Object System.Net.WebClient
    $wc.DownloadFile($Url, $TempZip)
} catch {
    if (-not $Silent) { Write-Host "✗ Download failed: $_" -ForegroundColor Red }
    return $false
}

# ── Extract ──
if (Test-Path $ExtractDir) { Remove-Item -Recurse -Force $ExtractDir }
Expand-Archive -Path $TempZip -DestinationPath $ExtractDir -Force | Out-Null

# ── Find variable TTF files (woff2 are for web) ──
$ttfFiles = Get-ChildItem -Path $ExtractDir -Recurse -Filter "*.ttf" | Where-Object {
    $_.Name -like "JetBrainsMono-*" -and $_.Name -notlike "*NL*"
}

$count = 0
foreach ($font in $ttfFiles) {
    $dest = Join-Path $FontDir $font.Name
    Copy-Item -Path $font.FullName -Destination $dest -Force

    # Register in registry so apps detect it
    $regName = "JetBrains Mono (TrueType)"
    if ($font.Name -match "Italic") { $regName = "JetBrains Mono Italic (TrueType)" }
    elseif ($font.Name -match "Bold") { $regName = "JetBrains Mono Bold (TrueType)" }
    elseif ($font.Name -match "ExtraBold") { $regName = "JetBrains Mono ExtraBold (TrueType)" }
    elseif ($font.Name -match "Light") { $regName = "JetBrains Mono Light (TrueType)" }
    elseif ($font.Name -match "Medium") { $regName = "JetBrains Mono Medium (TrueType)" }
    elseif ($font.Name -match "SemiBold") { $regName = "JetBrains Mono SemiBold (TrueType)" }
    elseif ($font.Name -match "Thin") { $regName = "JetBrains Mono Thin (TrueType)" }
    elseif ($font.Name -match "Variable") { $regName = "JetBrains Mono Variable (TrueType)" }
    Set-ItemProperty -Path $RegPath -Name $regName -Value $font.Name -Force
    $count++
}

# ── Clean up ──
Remove-Item -Path $TempZip -Force -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $ExtractDir -ErrorAction SilentlyContinue

if (-not $Silent) {
    Write-Host "✓ JetBrains Mono installed ($count font files)" -ForegroundColor Green
    Write-Host "  Location: $FontDir" -ForegroundColor Cyan
    Write-Host "`nRestart VS Code to apply the font." -ForegroundColor Yellow
}
return $true
