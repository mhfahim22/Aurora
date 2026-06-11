$path = "D:\Downloads\aurora_restructured\left-pad_npm\left-pad_npm.c"
$bytes = [System.IO.File]::ReadAllBytes($path)
$result = [System.Collections.Generic.List[byte]]::new()

$i = 0
while ($i -lt $bytes.Count) {
    $found = $false

    # Pattern 1: ' + LF + '  (LF only, Unix)
    if ($i + 2 -lt $bytes.Count -and $bytes[$i] -eq 0x27 -and $bytes[$i+1] -eq 0x0A -and $bytes[$i+2] -eq 0x27) {
        $result.Add(0x27); $result.Add(0x5C); $result.Add(0x6E); $result.Add(0x27)
        $i += 3
        $found = $true
    }

    # Pattern 2: ' + CR + LF + '  (Windows)
    if (-not $found -and $i + 3 -lt $bytes.Count -and $bytes[$i] -eq 0x27 -and $bytes[$i+1] -eq 0x0D -and $bytes[$i+2] -eq 0x0A -and $bytes[$i+3] -eq 0x27) {
        $result.Add(0x27); $result.Add(0x5C); $result.Add(0x6E); $result.Add(0x27)
        $i += 4
        $found = $true
    }

    if (-not $found) {
        $result.Add($bytes[$i])
        $i++
    }
}

# Also fix JS_Eval semicolon (binary-safe)
$raw = $result.ToArray()
$text = [System.Text.Encoding]::UTF8.GetString($raw)
$text = $text.Replace("JS_FreeValue(g_ctx,_ev)}`r`n  g_inited", "JS_FreeValue(g_ctx,_ev)};`r`n  g_inited")

[System.IO.File]::WriteAllText($path, $text)
Write-Host "Fixed. File size: $((Get-Item $path).Length)"
exit 0
