# Migration Guide

Guide for migrating Aurora code across major versions.

## From v0.x to v1.0 (Upcoming)

### Renamed: `TokenType` → `TokenKind`

If you work with the compiler API or use tokens directly:

```diff
- TokenType::Integer
+ TokenKind::Integer
```

### Renamed: `Token::type` → `Token::kind`

```diff
- token.type == TokenType::Identifier
+ token.kind == TokenKind::Identifier
```

This change was necessary to resolve a Windows SDK conflict with the `TOKEN_INFORMATION_CLASS` type.

### New Module Structure

Modules from Phases 14–25 have been added under `std/` headers and `libc/` bindings:

| Module | Header | Bindings | Phase |
|--------|--------|----------|-------|
| Serialization | `std/serial.hpp` | `serial.auf` | 14 |
| Database | `std/db.hpp` | `db.auf` | 15 |
| Desktop | `std/desktop.hpp` | `desktop.auf` | 18 |
| Game | `std/game.hpp` | `game.auf` | 19 |
| Plugin | `std/plugin.hpp` | `plugin.auf` | 20 |
| Package | `std/pkg.hpp` | `pkg.auf` | 21 |
| Hot Reload | `std/hotreload.hpp` | `hotreload.auf` | 23 |
| Testing | `std/test.hpp` | `test.auf` | 24 |
| Developer Tools | `std/dev.hpp` | `dev.auf` | 25 |

Include via:
```aura
use "libc/serial.auf"
use "libc/db.auf"
```

### Mobile APIs Are Now Platform-Conditional

Android and iOS functions are only available on their respective platforms. Desktop builds provide stubs that return error codes.

```aura
use "libc/android.auf"   # Android only
use "libc/ios.auf"       # iOS only
use "libc/mobile_widgets.auf"  # Cross-platform
```

### Build System Flags

The compiler now supports `--jobs N` for parallel compilation and `--target triple` for cross-compilation:

```bash
aurora build --jobs 8
aurora build --target aarch64-linux-gnu
```

## From v1.0 to v1.1

*(Placeholder for future changes.)*
