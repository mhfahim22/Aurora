# App Deployment Guide

This guide covers packaging and publishing Aurora apps to app stores.

## Prerequisites

### Windows
- Inno Setup (for .exe installer)
- WiX Toolset (for .msi)
- Microsoft Partner Center account (for Store)

### Linux
- appimagetool (for AppImage)
- dpkg-deb (for .deb)

### macOS
- Xcode command line tools
- Apple Developer account

### Android
- Android NDK
- Android Studio / Gradle
- Google Play Developer account

### iOS
- Xcode
- Apple Developer account ($99/year)

## Packaging

Use the voss package command:

```bash
# Package for Windows
voss package --target windows --format exe
voss package --target windows --format msi

# Package for Linux
voss package --target linux --format appimage
voss package --target linux --format deb

# Package for macOS
voss package --target macos --format dmg

# Package for Android
voss package --target android --format apk
voss package --target android --format aab

# Package for iOS
voss package --target ios --format ipa

# Package for all platforms
voss package --target all
```

Or use the build scripts directly:

```bash
scripts/build_android_app.sh
scripts/build_ios_app.sh
```

## Code Signing

### Windows
```bash
signtool sign /fd SHA256 /a /f certificate.pfx /p password myapp.exe
```

### macOS
```bash
codesign --force --sign "Developer ID Application: Your Name" --deep myapp.app
```

### Android
```bash
jarsigner -verbose -sigalg SHA1withRSA -digestalg SHA1 \
  -keystore release-key.jks myapp.apk alias_name
```

### iOS
```bash
# Automatic via Xcode with proper team set in project
xcodebuild -project App.xcodeproj -scheme App \
  -configuration Release \
  DEVELOPMENT_TEAM=YOUR_TEAM_ID \
  PROVISIONING_PROFILE_SPECIFIER="match AppStore com.yourapp" \
  archive
```

## Store Submission

### Apple App Store (iOS)

```bash
scripts/submit_to_appstore.sh \
  --ipa path/to/app.ipa \
  --apple-id your@email.com \
  --password "app-specific-password" \
  --team-id YOUR_TEAM_ID
```

### Google Play Store (Android)

```bash
scripts/submit_to_playstore.sh \
  --aab path/to/app.aab \
  --service-account service-account.json \
  --package-name com.example.myapp
```

### Microsoft Store (Windows)

```bash
scripts/submit_to_windows_store.sh \
  --msix path/to/app.msix \
  --app-id YOUR_APP_ID \
  --tenant-id YOUR_TENANT_ID
```

## App Store Requirements

### iOS App Store
- App icon (1024x1024)
- Screenshots (6.5" and 5.5" displays)
- Privacy policy URL
- No beta APIs
- 64-bit only (arm64)

### Google Play Store
- App icon (512x512)
- Feature graphic (1024x500)
- Screenshots (at least 2)
- Privacy policy
- Content rating questionnaire

### Microsoft Store
- App logo (various sizes)
- Screenshots (at least 1)
- Age rating
- Privacy policy (if data collection)

## CI/CD Integration

### GitHub Actions

```yaml
name: Package and Publish
on:
  release:
    types: [published]

jobs:
  package:
    strategy:
      matrix:
        os: [windows-latest, ubuntu-latest, macos-latest]
    runs-on: ${{ matrix.os }}
    steps:
      - uses: actions/checkout@v4
      - uses: aurora-lang/setup-aurora@v1
      - run: voss package --target all
      - uses: actions/upload-artifact@v4
        with:
          name: release-${{ matrix.os }}
          path: build/*
```

### Manual Release Checklist

1. Bump version in `aurora.pkg` and `app.json`
2. Run tests: `voss test`
3. Build all targets: `voss package --target all`
4. Sign all binaries
5. Submit to stores
6. Tag release: `git tag v1.0.0 && git push --tags`
