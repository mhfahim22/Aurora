# Release Process

## Versioning

Current version: **0.2.0** (pre-1.0 development)

Version format: `MAJOR.MINOR.PATCH` (semver after 1.0)

## Creating a Release

1. **Update version** — Edit `VERSION` file with the new version
2. **Update CHANGELOG.md** — Move "Unreleased" items to a dated release section
3. **Tag the release:**
   ```bash
   git tag -a v$(cat VERSION) -m "Release v$(cat VERSION)"
   git push origin v$(cat VERSION)
   ```
4. **Build all artifacts:**
   ```bash
   cmake --build build --config Release -j$(nproc)
   ```
5. **Run full test suite:**
   ```bash
   build\Release\test_bridge_e2e.exe
   build\Release\test_ffi_memory_safety.exe
   build\Release\test_pypi_thread_safety.exe
   build\Release\test_integration_http.exe
   ```
6. **Run ASAN:**
   ```bash
   .\build_asan.bat
   build_asan\Debug\test_ffi_memory_safety.exe
   ```
7. **Create GitHub Release:**
   ```bash
   gh release create v$(cat VERSION) --title "v$(cat VERSION)" --notes "$(cat CHANGELOG.md | head -20)"
   ```

## Pre-Release Checklist

- [ ] All Phases 1–7 complete (PRODUCTION_PLAN.md)
- [ ] 25/25 bridge e2e tests PASS
- [ ] All FFI memory safety tests PASS under ASAN
- [ ] All PyPI thread safety tests PASS
- [ ] All integration HTTP tests PASS
- [ ] No known critical or high-severity issues
- [ ] Bridge DLLs compile on clean checkout
