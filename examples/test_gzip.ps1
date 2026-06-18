$log = "D:\Downloads\aurora_restructured\examples\srv_gzip.log"
Remove-Item $log -ErrorAction SilentlyContinue

# Start server
$p = Start-Process -FilePath "D:\Downloads\aurora_restructured\examples\todo_server_gzip.exe" -ArgumentList "8085" -WindowStyle Hidden -PassThru -RedirectStandardOutput $log
Start-Sleep -Seconds 2

# Create test data using .NET HttpClient (fast)
$client = New-Object System.Net.Http.HttpClient
$client.Timeout = [TimeSpan]::FromSeconds(10)

for ($i = 0; $i -lt 30; $i++) {
    $body = "{`"title`":`"Task $i with padding to exceed gzip 512 byte threshold easily`",`"completed`":false}"
    $content = New-Object System.Net.Http.StringContent($body, [System.Text.Encoding]::UTF8, "application/json")
    $resp = $client.PostAsync("http://localhost:8085/todos", $content).Result
    $resp.Dispose()
}
Write-Host "Created 30 todos"

# Now test with Accept-Encoding: gzip
$req = New-Object System.Net.Http.HttpRequestMessage([System.Net.Http.HttpMethod]::Get, "http://localhost:8085/todos")
$req.Headers.TryAddWithoutValidation("Accept-Encoding", "gzip") | Out-Null
$req.Headers.ConnectionClose = $true

$resp = $client.SendAsync($req).Result
Write-Host "Status: $($resp.StatusCode)"
foreach ($h in $resp.Headers) {
    Write-Host "$($h.Key): $($h.Value -join ',')"
}
if ($resp.Content.Headers.Contains("Content-Encoding")) {
    Write-Host "Content-Encoding: $($resp.Content.Headers.GetValues('Content-Encoding') -join ',')"
}
Write-Host "Content-Length (body): $($resp.Content.Headers.ContentLength)"
$bodyBytes = $resp.Content.ReadAsByteArrayAsync().Result
Write-Host "Raw body size: $($bodyBytes.Length)"

# Try to detect if it's gzip
if ($bodyBytes.Length -ge 2 -and $bodyBytes[0] -eq 0x1F -and $bodyBytes[1] -eq 0x8B) {
    Write-Host "*** GZIP DETECTED (starts with 0x1F8B) ***"
    # Decompress
    $ms = New-Object System.IO.MemoryStream($bodyBytes)
    $gz = New-Object System.IO.Compression.GZipStream($ms, [System.IO.Compression.CompressionMode]::Decompress)
    $sr = New-Object System.IO.StreamReader($gz)
    $text = $sr.ReadToEnd()
    Write-Host "Decompressed size: $($text.Length)"
} else {
    Write-Host "*** NOT GZIP ***"
    Write-Host "Body preview: $([System.Text.Encoding]::UTF8.GetString($bodyBytes, 0, [Math]::Min(200, $bodyBytes.Length)))"
}

$client.Dispose()
$p | Stop-Process -Force -ErrorAction SilentlyContinue

# Show server log
Write-Host "=== SERVER LOG ==="
Get-Content $log -Tail 15
