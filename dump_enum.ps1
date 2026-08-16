$lines = Get-Content 'C:\msys64\ucrt64\include\llvm\IR\Attributes.h'
for ($i = 88; $i -le 230 -and $i -le $lines.Count; $i++) {
    Write-Output ("{0}: {1}" -f $i, $lines[$i - 1])
}