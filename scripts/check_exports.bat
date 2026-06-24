@echo off
call "C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
dumpbin /exports "%~dp0..\build\Debug\test_pypi_thread_safety.exe" 2>&1 | findstr "aurora"
