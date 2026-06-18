@echo off
setlocal
cd /d D:\Downloads\aurora_restructured\examples

taskkill /f /im todo_server_gzip.exe 2>nul
ping 127.0.0.1 -n 3 > nul
del todos.db 2>nul

:: Start server (stdout now unbuffered - printf appears immediately)
start /B todo_server_gzip.exe
ping 127.0.0.1 -n 4 > nul
echo Server started

:: Create 15 todos
powershell -Command "$wc=New-Object System.Net.WebClient;$wc.Headers.Add('Content-Type','application/json');foreach($i in 1..15){$wc.UploadString('http://localhost:8080/todos','POST','{\"title\":\"gzip test padding 512 bytes\",\"completed\":false}')|Out-Null};Write-Host 'Created 15 todos'"

:: Test with gzip
echo === GZIP TEST ===
C:\Windows\System32\curl.exe -s -i -H "Accept-Encoding: gzip" -H "Connection: close" http://localhost:8080/todos 2>&1

echo === DONE ===
taskkill /f /im todo_server_gzip.exe 2>nul
