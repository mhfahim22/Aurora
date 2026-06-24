$path = "D:\Downloads\aurora_restructured\aurora\tools\voss\bridge_npm.cpp"
$bytes = [System.IO.File]::ReadAllBytes($path)
$result = [System.Collections.Generic.List[byte]]::new($bytes.Count)

# State tracking
$inStr = $false          # inside a "..." string literal
$inChar = $false         # inside a '...' char constant
$inBlockComment = $false # inside /* ... */
$inLineComment = $false  # inside // ... \n
$esc = $false            # escape flag for strings/chars

# Byte values
$dq = 0x22  # "
$sq = 0x27  # '
$bs = 0x5c  # \
$lf = 0x0a  # \n (newline)
$n  = 0x6e  # n
$sl = 0x2f  # /
$st = 0x2a  # *
$cr = 0x0d  # \r

for ($i = 0; $i -lt $bytes.Count; $i++) {
    $ch = $bytes[$i]

    if ($inBlockComment) {
        # Looking for */
        if ($ch -eq $st) {
            # Could be start of */
            if ($i + 1 -lt $bytes.Count -and $bytes[$i + 1] -eq $sl) {
                [void]$result.Add($st)
                [void]$result.Add($sl)
                $i++  # skip */
                $inBlockComment = $false
            } else {
                [void]$result.Add($ch)
            }
        } else {
            [void]$result.Add($ch)
        }
    } elseif ($inLineComment) {
        # Line comment ends at LF
        if ($ch -eq $lf) {
            $inLineComment = $false
            [void]$result.Add($ch)
        } else {
            [void]$result.Add($ch)
        }
    } elseif ($inStr) {
        # Inside a C++ string literal "..." (or generated C code string)
        if ($ch -eq $bs -and !$esc) {
            $esc = $true
            [void]$result.Add($ch)
        } elseif ($ch -eq $dq -and !$esc) {
            $inStr = $false
            [void]$result.Add($ch)
        } elseif ($ch -eq $lf) {
            # Embedded LF inside string --- original \n escape was corrupted
            $esc = $false
            [void]$result.Add($bs)
            [void]$result.Add($n)
        } else {
            $esc = $false
            [void]$result.Add($ch)
        }
    } elseif ($inChar) {
        # Inside a char constant '...' 
        if ($ch -eq $bs -and !$esc) {
            $esc = $true
            [void]$result.Add($ch)
        } elseif ($ch -eq $sq -and !$esc) {
            $inChar = $false
            [void]$result.Add($ch)
        } elseif ($ch -eq $lf) {
            $esc = $false
            $inChar = $false
            [void]$result.Add($ch)
        } else {
            $esc = $false
            [void]$result.Add($ch)
        }
    } else {
        # Outside any string/char/comment — track starts
        if ($ch -eq $sl) {
            # Could be /* or //
            if ($i + 1 -lt $bytes.Count) {
                $next = $bytes[$i + 1]
                if ($next -eq $st) {
                    [void]$result.Add($sl); [void]$result.Add($st)
                    $i++
                    $inBlockComment = $true
                } elseif ($next -eq $sl) {
                    [void]$result.Add($sl); [void]$result.Add($sl)
                    $i++
                    $inLineComment = $true
                } else {
                    [void]$result.Add($ch)
                }
            } else {
                [void]$result.Add($ch)
            }
        } elseif ($ch -eq $dq) {
            $inStr = $true
            [void]$result.Add($ch)
        } elseif ($ch -eq $sq) {
            $inChar = $true
            [void]$result.Add($ch)
        } else {
            [void]$result.Add($ch)
        }
    }
}

[System.IO.File]::WriteAllBytes($path, $result.ToArray())

# Summary
$newBytes = [System.IO.File]::ReadAllBytes($path)
$lfCount = 0; $bsnCount = 0
for ($i = 0; $i -lt $newBytes.Count; $i++) { if ($newBytes[$i] -eq 0x0A) { $lfCount++ } }
for ($i = 0; $i -lt $newBytes.Count - 1; $i++) { if ($newBytes[$i] -eq 0x5C -and $newBytes[$i+1] -eq 0x6E) { $bsnCount++ } }

Write-Host "Size: $($newBytes.Count), LF: $lfCount, BS+N: $bsnCount"
Write-Host "Final states: str=$inStr char=$inChar blk=$inBlockComment line=$inLineComment"
