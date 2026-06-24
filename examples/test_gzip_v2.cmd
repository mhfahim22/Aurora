@echo off
setlocal
cd /d D:\Downloads\aurora_restructured\examples

:: Kill old server
taskkill /f /im todo_server_gzip.exe 2>nul
ping 127.0.0.1 -n 3 > nul

:: Clear DB
del todos.db 2>nul

:: Start server in background (NO stdout redirect to avoid buffer issues)
start /B todo_server_gzip.exe 8080
ping 127.0.0.1 -n 4 > nul
echo Server started

:: Create todos using PowerShell
powershell -Command "$wc=New-Object System.Net.WebClient;$wc.Headers.Add('Content-Type','application/json');foreach($i in 1..15){$wc.UploadString('http://localhost:8080/todos','POST','{\"title\":\"gzip test padding to exceed 512 bytes threshold very easily with this long title\",\"completed\":false}')|Out-Null};Write-Host 'Created 15 todos'"

:: Test with gzip using curl.exe (send full request at once)
echo === GZIP TEST ===
C:\Windows\System32\curl.exe -s -i -H "Accept-Encoding: gzip" -H "Connection: close" http://localhost:8080/todos 2>&1

echo === DONE ===
taskkill /f /im todo_server_gzip.exe 2>nul
