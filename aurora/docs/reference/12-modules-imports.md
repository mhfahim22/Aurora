# Modules & Imports

## Import Forms

```aura
import "path/to/file.aura"       # file path (relative or absolute)
import stdio                      # library module (std lib)
import kernel32                   # FFI module (Windows API)
import MrCode                     # third-party package
import libc:string                # scoped import (from libc package)
from "utils" import helper        # selective import
```

## Module / Package / Namespace

```aura
module com.example.myapp          # module declaration (dotted path)
package myapp                     # package declaration
package com.example.app           # package with dotted path
```

### Namespace

```aura
namespace Graphics:               # namespace with block
    function render()
        output("rendering")

# Namespace without colon:
namespace mylib
    function helper()
        pass

# Brace block:
namespace mylib {
    function helper()
        pass
}
```

## Alias

```aura
alias math = Aurora.Math
alias MyInt = int                 # type alias
```

## Global / Outer

```aura
global :: varName                 # access global scope
outer :: varName                  # access outer scope
```

## `from` / `import` Selective

```aura
from "path/to/utils" import helper, format
from libc import string, math
```

## Module Resolution Order

When you write `import "module"`, Aurora searches:

1. Exact file path match
2. Path + `.aura` extension
3. Search path / name
4. Search path / name.aura
5. Search path / lib / name
6. Search path / lib / name.aura

## Package Manifest (`aurora.pkg`)

```json
{
    "name": "myapp",
    "version": "1.0.0",
    "author": "Alice",
    "description": "My Aurora application",
    "entry": "main.aura",
    "depends": ["MrCode >= 2.0"]
}
```

| Field         | Required | Description             |
|---------------|----------|-------------------------|
| `name`        | Yes      | Package name            |
| `version`     | Yes      | SemVer version          |
| `author`      | No       | Author name             |
| `description` | No       | Short description       |
| `entry`       | Yes      | Main entry point file   |
| `depends`     | No       | Dependency list         |

---

**Next:** [FFI](13-ffi.md)
