@echo off
setlocal
cd /d D:\Downloads\aurora_restructured\examples

taskkill /f /im todo_server_gzip.exe 2>nul
ping 127.0.0.1 -n 3 > nul
del todos.db 2>nul

:: Start server, redirect BOTH stdout and stderr to files
start /B cmd /c "todo_server_gzip.exe > srv_out.txt 2> srv_err.txt"
ping 127.0.0.1 -n 4 > nul

:: Create 15 todos
powershell -Command "$wc=New-Object System.Net.WebClient;$wc.Headers.Add('Content-Type','application/json');foreach($i in 1..15){$wc.UploadString('http://localhost:8080/todos','POST','{\"title\":\"gzip test\",\"completed\":false}')|Out-Null}"
echo Created 15 todos

:: Test with gzip
C:\Windows\System32\curl.exe -s -i -H "Accept-Encoding: gzip" -H "Connection: close" http://localhost:8080/todos 2>&1

echo === STDERR ===
type srv_err.txt

echo === STDOUT ===
type srv_out.txt

taskkill /f /im todo_server_gzip.exe 2>nul
