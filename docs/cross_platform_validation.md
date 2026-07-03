# Cross-Platform Validation

Aurora targets 5 platforms: **Windows**, **Linux**, **macOS**, **Android**, and **iOS**.

---

## Build Presets (CMakePresets.json)

All platforms can be configured via CMake presets:

| Preset | Platform | Generator | Arch |
|---|---|---|---|
| `default` | Windows | VS 17 2022 | x64 |
| `windows-debug` | Windows (Debug) | VS 17 2022 | x64 |
| `linux-x64` | Linux | Ninja | x86_64 |
| `linux-x64-clang` | Linux | Ninja | x86_64 |
| `macos-x64` | macOS Intel | Ninja | x86_64 |
| `macos-arm64` | macOS Apple Silicon | Ninja | arm64 |
| `android-arm64` | Android | Ninja (NDK) | arm64-v8a |
| `ios-arm64` | iOS Device | Xcode | arm64 |
| `ios-simulator-arm64` | iOS Simulator | Xcode | arm64 |

```bash
# Windows
cmake --preset default
cmake --build --preset default

# Linux
cmake --preset linux-x64
cmake --build --preset linux-x64

# macOS
cmake --preset macos-arm64
cmake --build --preset macos-arm64

# Android (requires NDK)
cmake --preset android-arm64
cmake --build --preset android-arm64

# iOS (macOS only)
cmake --preset ios-arm64
cmake --build --preset ios-arm64
```

---

## Build Scripts

| Script | Platform | Notes |
|---|---|---|
| `scripts/build_linux.sh` | Linux x64 | GCC, optional test targets |
| `scripts/build_macos.sh` | macOS x64/ARM64 | Apple Clang, auto-detects LLVM |
| `scripts/build_android.sh` | Android ARM64 | NDK cross-compile + Gradle APK |
| `scripts/build_ios.sh` | iOS arm64/sim | Xcode project + native lib |
| `scripts/build_apk.bat` | Android APK (Win) | Windows-native APK builder |
| `scripts/build_ipa.sh` | iOS IPA (macOS) | macOS-native IPA builder |

---

## Validation Test Suite

The `test_crossplatform` CTest target validates core subsystems on every platform:

- **Platform Detection** — OS and architecture identification
- **Threading** — multi-threaded atomic operations
- **Filesystem** — directory/file create, read, write, list, delete
- **Performance** — integer throughput and alloc/free timing

```bash
# Build + run validation
cmake --build build --target test_crossplatform -j$(nproc)
./build/test_crossplatform -v
```

The suite is also registered as a CTest target:
```bash
ctest -R test_crossplatform -V
```

---

## CI Pipeline

GitHub Actions (`.github/workflows/build.yml`) builds and validates on:

| Job | Runner | Validation |
|---|---|---|
| `build-windows` | windows-latest | `test_crossplatform.exe -v` |
| `build-ubuntu` | ubuntu-latest | `./test_crossplatform -v` |
| `build-macos` | macos-latest | `./test_crossplatform -v` |

Android emulator (`scripts/test_android_emulator.sh`) and iOS simulator (`scripts/test_ios_simulator.sh`) validation scripts are available for local/manual testing.

---

## Docker

```bash
# Build the Linux builder image
docker build -t aurora-linux-builder .

# Build Aurora inside container
docker run --rm -v $(pwd):/aurora -w /aurora aurora-linux-builder \
    bash scripts/build_linux.sh
```

---

## Prerequisites by Platform

### Windows
- Visual Studio 2022 with "Desktop development with C++"
- LLVM 19 ([llvm.org](https://llvm.org))
- CMake 3.20+

### Linux (Ubuntu 22.04+)
```bash
sudo apt install cmake g++ ninja-build libcurl4-openssl-dev \
    libgl1-mesa-dev liblld-19-dev
# LLVM 19 via apt.llvm.org
wget https://apt.llvm.org/llvm.sh && chmod +x llvm.sh && sudo ./llvm.sh 19
```

### macOS
```bash
brew install cmake ninja llvm@19 curl
```

### Android
- Android NDK (v26+) — set `ANDROID_NDK_HOME`
- Android SDK — set `ANDROID_HOME`
- Gradle (bundled with SDK)

### iOS (macOS only)
- Xcode 15+
- LLVM 19 (brew)
- CocoaPods (optional)
