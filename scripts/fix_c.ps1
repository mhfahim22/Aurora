$path = "D:\Downloads\aurora_restructured\left-pad_npm\left-pad_npm.c"
$bytes = [System.IO.File]::ReadAllBytes($path)
$result = [System.Collections.Generic.List[byte]]::new($bytes.Count)

$i = 0
while ($i -lt $bytes.Count) {
    # Fix 1: ' + LF + ' -> text '\n'
    if ($i + 2 -lt $bytes.Count -and $bytes[$i] -eq 0x27 -and $bytes[$i+1] -eq 0x0A -and $bytes[$i+2] -eq 0x27) {
        $result.Add(0x27)   # '
        $result.Add(0x5C)   # \
        $result.Add(0x6E)   # n
        $result.Add(0x27)   # '
        $i += 3
        continue
    }
    # Fix 2: " + LF inside fprintf etc - just skip LF if inside a string context
    # We'll handle this differently
    $result.Add($bytes[$i])
    $i++
}

$text = [System.Text.Encoding]::UTF8.GetString($result.ToArray())
# Fix JS_Eval missing semicolon
$text = $text.Replace("JS_FreeValue(g_ctx,_ev)}`n  g_inited", "JS_FreeValue(g_ctx,_ev)};`n  g_inited")
# Fix g_loaded=\n issue
$text = $text.Replace(";`n    g_loaded=1;", ";\`n    g_loaded=1;")

[System.IO.File]::WriteAllText("D:\Downloads\aurora_restructured\left-pad_npm\left-pad_npm_fixed.c", $text)
Write-Host "Done. File size: $($text.Length)"
