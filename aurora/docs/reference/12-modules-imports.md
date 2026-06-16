# Modules & Imports

## Import Forms

```aura
import "path/to/file.aura"       # file path
import stdio                      # library module
import kernel32                   # FFI module
import MrCode                     # third-party package
import libc:string                # scoped import
from "utils" import helper        # selective import
```

## Module / Package / Namespace

```aura
module com.example.myapp          # module declaration
package myapp                     # package declaration
namespace Graphics:               # namespace with block
    function render()
        output("rendering")

# Namespace without colon:
namespace mylib
    function helper()
        pass
```

## Alias

```aura
alias math = Aurora.Math
alias MyInt = int
```

## Global / Outer

```aura
global :: varName                 # access global scope
outer :: varName                  # access outer scope
```

## Module Resolution Order

1. Exact file path
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

## Package Commands (programmatic)

```aura
import package_manager
install("my-package")
update("my-package")
results = search("keyword")
```
