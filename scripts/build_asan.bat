@echo off
setlocal enabledelayedexpansion

set BUILD_DIR=%~dp0build_asan
set SRC_DIR=%~dp0

echo === Configuring ASAN build ===
cmake -S "%SRC_DIR%" -B "%BUILD_DIR%" ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_CXX_FLAGS_DEBUG="/fsanitize=address /MDd /Od /Zi /D_ITERATOR_DEBUG_LEVEL=0" ^
    -DCMAKE_EXE_LINKER_FLAGS_DEBUG="/DEBUG" ^
    -DLLVM_DIR="C:\Program Files\Microsoft Visual Studio\18\Community\Tools\LLVM\lib\cmake\llvm"
if %ERRORLEVEL% neq 0 exit /b 1

echo.
echo === Building all targets ===
cmake --build "%BUILD_DIR%" --config Debug
if %ERRORLEVEL% neq 0 exit /b 1

echo.
echo === ASAN build complete ===
echo To run with ASAN, ensure the ASAN runtime DLL is in PATH:
echo     set PATH=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64;%%PATH%%
echo.
echo Example:
echo     "%BUILD_DIR%\Debug\test_bridge_e2e.exe"
echo     "%BUILD_DIR%\Debug\test_pypi_thread_safety.exe"
echo     "%BUILD_DIR%\Debug\test_ffi_memory_safety.exe"
