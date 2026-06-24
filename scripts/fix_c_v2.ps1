$path = "D:\Downloads\aurora_restructured\left-pad_npm\left-pad_npm.c"
$bytes = [System.IO.File]::ReadAllBytes($path)
$result = [System.Collections.Generic.List[byte]]::new()

$i = 0
while ($i -lt $bytes.Count) {
    # ' + LF + ' -> '\n'
    if ($i + 2 -lt $bytes.Count -and $bytes[$i] -eq 0x27 -and $bytes[$i+1] -eq 0x0A -and $bytes[$i+2] -eq 0x27) {
        $result.Add(0x27)  # '
        $result.Add(0x5C)  # \
        $result.Add(0x6E)  # n
        $result.Add(0x27)  # '
        $i += 3
        continue
    }
    $result.Add($bytes[$i])
    $i++
}

# Write fixed bytes
[System.IO.File]::WriteAllBytes($path, $result.ToArray())

# Fix JS_Eval semicolon in text mode (for the CRLF case)
$text = [System.IO.File]::ReadAllText($path)
$text = $text.Replace("JS_FreeValue(g_ctx,_ev)}`n  g_inited", "JS_FreeValue(g_ctx,_ev)};`n  g_inited")
$text = $text.Replace("JS_FreeValue(g_ctx,_ev)}\r\n  g_inited", "JS_FreeValue(g_ctx,_ev)};\r\n  g_inited")
[System.IO.File]::WriteAllText($path, $text)

Write-Host "Fixed. File size: $((Get-Item $path).Length)"
exit 0
