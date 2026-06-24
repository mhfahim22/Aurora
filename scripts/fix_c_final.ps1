$path = "D:\Downloads\aurora_restructured\left-pad_npm\left-pad_npm.c"
$bytes = [System.IO.File]::ReadAllBytes($path)
$result = [System.Collections.Generic.List[byte]]::new()
$inStr = $false; $inChar = $false; $esc = $false

for ($i = 0; $i -lt $bytes.Count; $i++) {
    $ch = $bytes[$i]
    if ($inStr) {
        if ($ch -eq 0x5C -and !$esc) {
            $esc = $true; [void]$result.Add($ch)
        } elseif ($ch -eq 0x22 -and !$esc) {
            $inStr = $false; [void]$result.Add($ch)
        } elseif ($ch -eq 0x0A -or $ch -eq 0x0D) {
            # CR/LF inside string literal: insert \n instead
            if ($ch -eq 0x0D) { continue }  # skip CR
            $esc = $false
            [void]$result.Add(0x5C); [void]$result.Add(0x6E)  # \n
        } else { $esc = $false; [void]$result.Add($ch) }
    } elseif ($inChar) {
        if ($ch -eq 0x5C -and !$esc) {
            $esc = $true; [void]$result.Add($ch)
        } elseif ($ch -eq 0x27 -and !$esc) {
            $inChar = $false; [void]$result.Add($ch)
        } elseif ($ch -eq 0x0A -or $ch -eq 0x0D) {
            if ($ch -eq 0x0D) { continue }
            $esc = $false; $inChar = $false
            [void]$result.Add(0x5C); [void]$result.Add(0x6E)  # \n
        } else { $esc = $false; [void]$result.Add($ch) }
    } else {
        if ($ch -eq 0x22) { $inStr = $true; $esc = $false; [void]$result.Add($ch) }
        elseif ($ch -eq 0x27) { $inChar = $true; $esc = $false; [void]$result.Add($ch) }
        else { [void]$result.Add($ch) }
    }
}

[System.IO.File]::WriteAllBytes($path, $result.ToArray())
Write-Host "Fixed. Size: $((Get-Item $path).Length), str=$inStr char=$inChar"
exit 0
