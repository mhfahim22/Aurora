@echo off
setlocal enabledelayedexpansion

REM Build Aurora with AddressSanitizer (ASAN) for memory error detection

set BUILD_DIR=build_asan
set LLVM_DIR=C:\Program Files\LLVM\lib\cmake\llvm

if not exist "%LLVM_DIR%" (
    echo [ERROR] LLVM not found at %LLVM_DIR%
    echo Set LLVM_DIR to your LLVM installation path and rerun.
    exit /b 1
)

echo [INFO] Configuring ASAN build...
cmake -B %BUILD_DIR% -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_CXX_FLAGS_DEBUG="/fsanitize=address /MDd /Od /Zi /D_ITERATOR_DEBUG_LEVEL=0" ^
    -DCMAKE_EXE_LINKER_FLAGS_DEBUG="/DEBUG" ^
    -DLLVM_DIR="%LLVM_DIR%"

if %errorlevel% neq 0 (
    echo [ERROR] CMake configuration failed
    exit /b %errorlevel%
)

echo [INFO] Building ASAN targets...
cmake --build %BUILD_DIR% --config Debug --target test_ffi_memory_safety --target bench_npm_bridge -j2

if %errorlevel% neq 0 (
    echo [ERROR] ASAN build failed
    exit /b %errorlevel%
)

echo [INFO] ASAN build complete.
echo [INFO] Run tests: build_asan\Debug\test_ffi_memory_safety.exe
