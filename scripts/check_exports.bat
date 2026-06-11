@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
dumpbin /exports "D:\Downloads\aurora_restructured\build\Debug\test_pypi_thread_safety.exe" 2>&1 | findstr "aurora"
