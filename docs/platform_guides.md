# Platform Build Guides (Phase 12.4)

## Windows

**Requirements**: Visual Studio 2022, LLVM 19, CMake 3.20+

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
# Output: build/Release/aurorac.exe
```

**Installer**: `voss bundle --target windows` or run `release/build_installer.ps1`

**Signing**: Use `signtool.exe` from Windows SDK:
```bash
signtool sign /fd SHA256 /a /f cert.pfx aurorac.exe
```

## Linux

**Requirements**: LLVM 19, CMake 3.20+, g++ or clang++

```bash
cmake -B build -G "Unix Makefiles"
cmake --build build
# Output: build/aurorac
```

**AppImage**: `voss bundle --target linux --format appimage`
**Debian package**: `voss bundle --target linux --format deb`

## macOS

**Requirements**: Xcode 15+, LLVM 19 (Homebrew), CMake 3.20+

```bash
cmake -B build -G "Unix Makefiles"
cmake --build build
# Output: build/aurorac
```

**DMG**: `voss bundle --target macos --format dmg`
**Code Signing**: `codesign --force --deep -s "Developer ID" aurorac`

## Android

**Requirements**: Android NDK 25+, CMake 3.20+

```bash
cmake -B build/android \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a
cmake --build build/android
```

**APK/AAB**: `voss bundle --target android`

## iOS

**Requirements**: Xcode 15+, iOS SDK 17+

```bash
cmake -B build/ios \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos
cmake --build build/ios
```

**IPA**: `voss bundle --target ios --format ipa`
