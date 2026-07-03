@echo off
REM ════════════════════════════════════════════════════════════
REM build_apk.bat — Aurora Android APK build script
REM ════════════════════════════════════════════════════════════
REM Prerequisites:
REM   - Android SDK (ANDROID_HOME set)
REM   - Android NDK (ANDROID_NDK_HOME set)
REM   - Gradle (in PATH or ANDROID_HOME/gradle)
REM ════════════════════════════════════════════════════════════

setlocal enabledelayedexpansion

set AURORA_DIR=%~dp0..
set ANDROID_PROJECT=%AURORA_DIR%\packages\android
set BUILD_DIR=%AURORA_DIR%\build\android

if "%ANDROID_HOME%"=="" (
    echo [ERROR] ANDROID_HOME is not set. Set it to your Android SDK path.
    exit /b 1
)

echo [android] Building Aurora runtime for Android...

REM Step 1: Build aurora_runtime as a static library for Android using CMake cross-compilation
mkdir "%BUILD_DIR%" 2>nul
cd "%BUILD_DIR%"

cmake -G Ninja ^
    -DCMAKE_TOOLCHAIN_FILE="%ANDROID_NDK_HOME%\build\cmake\android.toolchain.cmake" ^
    -DANDROID_ABI=arm64-v8a ^
    -DANDROID_PLATFORM=android-24 ^
    -DANDROID_STL=c++_shared ^
    "%AURORA_DIR%"

if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake configuration failed
    exit /b 1
)

cmake --build . --target aurora_app

if %ERRORLEVEL% neq 0 (
    echo [ERROR] Build failed
    exit /b 1
)

REM Step 2: Copy native library to Android project
copy /Y "%BUILD_DIR%\libaurora_app.so" "%ANDROID_PROJECT%\app\src\main\jniLibs\arm64-v8a\"

REM Step 3: Build APK with Gradle
cd "%ANDROID_PROJECT%"
call gradlew assembleDebug

if %ERRORLEVEL% equ 0 (
    echo [android] APK built: %ANDROID_PROJECT%\app\build\outputs\apk\debug\app-debug.apk
) else (
    echo [ERROR] Gradle build failed
    exit /b 1
)

endlocal
