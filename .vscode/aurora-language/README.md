# Aurora Language — VS Code Extension

Affects **only `.aura` files** — your existing VS Code theme stays untouched.

## Features

| Feature | Scope |
|---------|-------|
| **Syntax highlighting** | `.aura` files only |
| **Auto-indent** | `.aura` files only |
| **Bracket matching** | `.aura` files only |
| **Auto-closing quotes** | `.aura` files only |
| **Comment toggling** | `.aura` files only |
| **Font auto-install** | System-wide (needed for best look) |

## Installation

```powershell
Copy-Item -Recurse ".vscode\aurora-language" "$env:USERPROFILE\.vscode\extensions\"
```

Restart VS Code.

## Font Auto-Install

On first `.aura` file open, extension offers to install **JetBrains Mono**.
Or manually: `Ctrl+Shift+P` → **Aurora: Install JetBrains Mono Font**

## Syntax Highlighting

The grammar assigns proper TextMate scopes (`keyword`, `string`, `comment`, `function`, etc.) so your current theme colors `.aura` files correctly out of the box. No separate theme needed.
