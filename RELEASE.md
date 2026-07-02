# Release Process

## Versioning

Current version: **1.0.0-rc.1** — Release Candidate 1

Version format: `MAJOR.MINOR.PATCH[-PRERELEASE]` (semver)

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
5. **Run profiling benchmark:**
   ```bash
   powershell -ExecutionPolicy Bypass -File scripts/profile.ps1
   ```
6. **Tag the release:**
   ```bash
   git tag -a v$(cat VERSION) -m "Release v$(cat VERSION)"
   git push origin v$(cat VERSION)
   ```
7. **GitHub Actions** automatically builds, packages, and uploads artifacts for all platforms.

## Pre-Release Checklist

- [x] All Phases 0-3 complete
- [x] Phase 4.1: Regression test suite automated
- [x] Phase 4.2: Performance profiling & optimization
- [x] Phase 4.3: Documentation & release packaging
- [x] 20/20 profiling benchmarks PASS
- [x] 9/9 regression stages PASS
- [x] 49/49 IR verification tests PASS
- [x] 3/3 CTest runtime tests PASS
- [x] No known critical or high-severity issues
- [x] CI/CD pipeline verified on all 3 platforms
