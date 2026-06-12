param([switch]$Silent)

$FontDir = "$env:LOCALAPPDATA\Microsoft\Windows\Fonts"
$RegPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Fonts"

# Create registry key if it doesn't exist
$regParent = Split-Path $RegPath -Parent
$regLeaf = Split-Path $RegPath -Leaf
if (-not (Test-Path $RegPath)) {
    if (Test-Path $regParent) {
        $null = New-Item -Path $regParent -Name $regLeaf -Force
    }
}
$TempZip = "$env:TEMP\jetbrains-mono.zip"
$ExtractDir = "$env:TEMP\jetbrains-mono"

$found = $false
try {
    $regVal = Get-ItemProperty -Path $RegPath -Name "*JetBrains Mono*" -ErrorAction Stop
    if ($regVal) { $found = $true }
} catch {}
if ($found) {
    if (-not $Silent) { Write-Host "OK JetBrains Mono already installed" -ForegroundColor Green }
    exit 0
}

if (-not (Test-Path $FontDir)) { New-Item -ItemType Directory -Path $FontDir -Force | Out-Null }

$Url = "https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip"
if (-not $Silent) { Write-Host "Downloading JetBrains Mono..." -ForegroundColor Cyan }

try {
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
    $wc = New-Object System.Net.WebClient
    $wc.DownloadFile($Url, $TempZip)
} catch {
    if (-not $Silent) { Write-Host "FAIL Download failed: $_" -ForegroundColor Red }
    exit 1
}

if (Test-Path $ExtractDir) { Remove-Item -Recurse -Force $ExtractDir }
Expand-Archive -Path $TempZip -DestinationPath $ExtractDir -Force | Out-Null

$ttfFiles = Get-ChildItem -Path $ExtractDir -Recurse -Filter "*.ttf" | Where-Object {
    $_.Name -like "JetBrainsMono-*" -and $_.Name -notlike "*NL*" -and $_.Name -notlike "*NerdFont*"
}

$count = 0
foreach ($font in $ttfFiles) {
    $dest = Join-Path $FontDir $font.Name
    Copy-Item -Path $font.FullName -Destination $dest -Force

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

Remove-Item -Path $TempZip -Force -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $ExtractDir -ErrorAction SilentlyContinue

if (-not $Silent) {
    Write-Host "OK JetBrains Mono installed ($count font files)" -ForegroundColor Green
    Write-Host "  Location: $FontDir" -ForegroundColor Cyan
    Write-Host " Restart VS Code to apply the font." -ForegroundColor Yellow
}
exit 0
