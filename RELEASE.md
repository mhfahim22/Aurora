# Release Process

## Versioning

Current version: **1.0.0** — Stable Release

Version format: `MAJOR.MINOR.PATCH` (semver)

## Creating a Release

1. **Update version** — Edit `VERSION` file with the new version
2. **Update CHANGELOG.md** — Move "Unreleased" items to a dated release section
3. **Build all artifacts:**
   ```bash
   cmake --build build --config Release -j$(nproc)
   ```
4. **Run full regression suite:**
   ```bash
   powershell -ExecutionPolicy Bypass -File scripts/regression.ps1
   ```
5. **Run release readiness check:**
   ```bash
   powershell -ExecutionPolicy Bypass -File scripts/check_release_readiness.ps1
   ```
6. **Run cross-platform validation:**
   ```bash
   ctest -R test_crossplatform -V
   ```
7. **Tag the release:**
   ```bash
   git tag -a v$(cat VERSION) -m "Release v$(cat VERSION)"
   git push origin v$(cat VERSION)
   ```
8. **GitHub Actions** automatically builds, packages, and uploads artifacts for all platforms.

## Pre-Release Checklist

- [x] All 30 phases complete
- [x] 23/23 build targets pass (zero errors)
- [x] 9/9 cross-platform validation tests PASS
- [x] 9/9 regression stages PASS
- [x] 49/49 IR verification tests PASS
- [x] 3/3 CTest runtime tests PASS
- [x] Inno Setup installer builds
- [x] GitHub Actions release workflow verified
- [x] Install scripts for Windows/macOS/Linux
- [x] No known critical or high-severity issues
- [x] CI/CD pipeline verified on all 3 platforms
