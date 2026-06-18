@echo off
setlocal
cd /d D:\Downloads\aurora_restructured\examples

:: Kill old server
taskkill /f /im todo_server_gzip.exe 2>nul
ping 127.0.0.1 -n 3 > nul

:: Clear DB
del todos.db 2>nul

:: Start server in background
start /B todo_server_gzip.exe > srv_out3.txt 2>&1
ping 127.0.0.1 -n 4 > nul
echo Server started

:: Create todos using PowerShell
powershell -Command "$wc=New-Object System.Net.WebClient;$wc.Headers.Add('Content-Type','application/json');foreach($i in 1..10){$wc.UploadString('http://localhost:8080/todos','POST','{\"title\":\"gzip test padding to exceed 512 bytes threshold\",\"completed\":false}')|Out-Null};Write-Host 'Created 10 todos'"

:: Test with gzip using PowerShell
powershell -Command "$tcp=New-Object System.Net.Sockets.TcpClient;$tcp.Connect('127.0.0.1',8080);$tcp.ReceiveTimeout=3000;$st=$tcp.GetStream();$req='GET /todos HTTP/1.1`r`nHost: localhost:8080`r`nAccept-Encoding: gzip`r`nConnection: close`r`n`r`n';$b=[System.Text.Encoding]::ASCII.GetBytes($req);$st.Write($b,0,$b.Length);Start-Sleep 1;$ms=New-Object System.IO.MemoryStream;$buf=New-Object byte[] 65536;while($st.DataAvailable){$n=$st.Read($buf,0,$buf.Length);if($n -gt 0){$ms.Write($buf,0,$n)};Start-Sleep -Milliseconds 200};$tcp.Close();$data=$ms.ToArray();$txt=[System.Text.Encoding]::ASCII.GetString($data);$idx=$txt.IndexOf(\"`r`n`r`n\");if($idx -ge 0){$txt.Substring(0,$idx)-split\"`r`n\"|%%{Write-Host $_};$body=$data[($idx+4)..($data.Length-1)];Write-Host \"Body: $($body.Length) bytes\";if($body.Length -ge 2 -and $body[0] -eq 0x1F -and $body[1] -eq 0x8B){Write-Host 'GZIP OK!'}else{Write-Host 'NOT GZIP - first bytes:' ('0x{0:X2} 0x{1:X2}' -f $body[0],$body[1])}}"

:: Show server log
echo === SERVER LOG ===
type srv_out3.txt
taskkill /f /im todo_server_gzip.exe 2>nul
