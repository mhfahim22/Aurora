@echo off
call "C:\Program Files\Microsoft Visual Studio\17\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
echo [BUILD] Configuring CMake...
cmake -S "%~dp0.." -B "%~dp0..\build" -DCMAKE_BUILD_TYPE=Debug 2>&1 | findstr /V "\.cpp$"
echo [BUILD] Building test_pypi_thread_safety...
cmake --build "%~dp0..\build" --config Debug --target test_pypi_thread_safety 2>&1
if %errorlevel% neq 0 exit /b 1
echo [BUILD] Build complete.
