$exe = "D:\Downloads\aurora_restructured\examples\todo_server_gzip.exe"
$db  = "D:\Downloads\aurora_restructured\examples\todos.db"
$log = "D:\Downloads\aurora_restructured\examples\srv_gzip.log"

# Clean
Remove-Item $db -ErrorAction SilentlyContinue
Remove-Item $log -ErrorAction SilentlyContinue

# Start server (redirect stdout to file for analysis later)
$p = Start-Process -FilePath $exe -WindowStyle Hidden -PassThru -RedirectStandardOutput $log
Start-Sleep -Seconds 3

# Create 10 todos using WebClient (fast)
$wc = New-Object System.Net.WebClient
$wc.Headers.Add("Content-Type", "application/json")
for ($i=1; $i -le 10; $i++) {
    $wc.UploadString("http://localhost:8080/todos", "POST", '{"title":"test gzip padding to exceed 512 bytes threshold easily","completed":false}') | Out-Null
}
Write-Host "Created 10 todos"

# === RAW TCP TEST 1: Send Accept-Encoding: gzip ===
Write-Host "=== TEST 1: With Accept-Encoding: gzip ==="
$tcp = New-Object System.Net.Sockets.TcpClient
$tcp.Connect("127.0.0.1", 8080)
$tcp.ReceiveTimeout = 3000
$stream = $tcp.GetStream()
$writer = New-Object System.IO.StreamWriter($stream)
$writer.AutoFlush = $true
$writer.WriteLine("GET /todos HTTP/1.1")
$writer.WriteLine("Host: localhost:8080")
$writer.WriteLine("Accept-Encoding: gzip")
$writer.WriteLine("Connection: close")
$writer.WriteLine("")

# Read all data
Start-Sleep -Milliseconds 500
$ms = New-Object System.IO.MemoryStream
$buf = New-Object byte[] 65536
do {
    if ($stream.DataAvailable) {
        $n = $stream.Read($buf, 0, $buf.Length)
        if ($n -gt 0) { $ms.Write($buf, 0, $n) }
    }
    Start-Sleep -Milliseconds 200
} while ($stream.DataAvailable)
$tcp.Close()

$bytes = $ms.ToArray()
$text = [System.Text.Encoding]::ASCII.GetString($bytes)
$hdrEnd = $text.IndexOf("`r`n`r`n")
if ($hdrEnd -ge 0) {
    $headers = $text.Substring(0, $hdrEnd)
    $headers -split "`r`n" | ForEach-Object { Write-Host $_ }
    $bodyBytes = $bytes[($hdrEnd+4)..($bytes.Length-1)]
    Write-Host "Body size: $($bodyBytes.Length)"
    if ($bodyBytes.Length -ge 2 -and $bodyBytes[0] -eq 0x1F -and $bodyBytes[1] -eq 0x8B) {
        Write-Host "<<< GZIP DETECTED >>>"
        $ms2 = New-Object System.IO.MemoryStream($bodyBytes)
        $gz = New-Object System.IO.Compression.GZipStream($ms2, [System.IO.Compression.CompressionMode]::Decompress)
        $sr = New-Object System.IO.StreamReader($gz)
        $text2 = $sr.ReadToEnd()
        Write-Host "Decompressed: $($text2.Length) chars"
    } else {
        Write-Host "<<< NOT gzip >>>"
    }
}

Write-Host ""

# === RAW TCP TEST 2: Without Accept-Encoding (to verify body is large enough) ===
Write-Host "=== TEST 2: Without gzip (baseline) ==="
$tcp2 = New-Object System.Net.Sockets.TcpClient
$tcp2.Connect("127.0.0.1", 8080)
$tcp2.ReceiveTimeout = 3000
$stream2 = $tcp2.GetStream()
$writer2 = New-Object System.IO.StreamWriter($stream2)
$writer2.AutoFlush = $true
$writer2.WriteLine("GET /todos HTTP/1.1")
$writer2.WriteLine("Host: localhost:8080")
$writer2.WriteLine("Connection: close")
$writer2.WriteLine("")

Start-Sleep -Milliseconds 500
$ms2 = New-Object System.IO.MemoryStream
do {
    if ($stream2.DataAvailable) {
        $n = $stream2.Read($buf, 0, $buf.Length)
        if ($n -gt 0) { $ms2.Write($buf, 0, $n) }
    }
    Start-Sleep -Milliseconds 200
} while ($stream2.DataAvailable)
$tcp2.Close()

$bytes2 = $ms2.ToArray()
$text2 = [System.Text.Encoding]::ASCII.GetString($bytes2)
$hdrEnd2 = $text2.IndexOf("`r`n`r`n")
if ($hdrEnd2 -ge 0) {
    $headers2 = $text2.Substring(0, $hdrEnd2)
    $headers2 -split "`r`n" | ForEach-Object { Write-Host $_ }
    $bodyBytes2 = $bytes2[($hdrEnd2+4)..($bytes2.Length-1)]
    Write-Host "Baseline body size: $($bodyBytes2.Length) bytes"
}

# Stop server and show log
$p | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 500
Write-Host "=== SERVER LOG (gzip lines) ==="
if (Test-Path $log) { Get-Content $log | Where-Object { $_ -match "gzip|server|http" } }
