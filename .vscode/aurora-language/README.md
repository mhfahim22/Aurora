# Aurora Language — VS Code Extension

## Installation (2 ways)

### Way 1: Symlink (recommended)

```powershell
# Run as Administrator
New-Item -ItemType Junction -Path "$env:USERPROFILE\.vscode\extensions\aurora-language" -Target "$PWD\.vscode\aurora-language"
```

Then restart VS Code. Open any `.aura` file.

### Way 2: Copy

```powershell
Copy-Item -Recurse ".vscode\aurora-language" "$env:USERPROFILE\.vscode\extensions\"
```

---

## What you get

| Feature | Description |
|---------|-------------|
| **Syntax highlighting** | Full TextMate grammar — keywords, strings, comments, numbers, operators, functions, methods, attributes |
| **Auto-indent** | Increases indent after `:` (function, class, if, for, etc.) |
| **Bracket matching** | `()`, `[]`, `{}` with colorful brackets |
| **Auto-closing** | Quotes and brackets auto-close |
| **Comment toggling** | `#` for line comments, `/* */` for block |
| **Custom theme** | **Aurora Synthwave** — dark neon theme with vibrant colors |

## Color scheme

| Token | Color | Hex |
|-------|-------|-----|
| Keywords | Hot Pink | `#ff79c6` |
| Function defs | Neon Green | `#50fa7b` |
| Strings | Lemon Yellow | `#f1fa8c` |
| Numbers | Soft Purple | `#bd93f9` |
| Builtins | Bright Cyan | `#8be9fd` |
| Comments | Muted Blue | `#6272a4` |
| OOP keywords | Lavender | `#c79aff` |
| Memory keywords | Peach | `#ffb86c` |
| Attributes | Gold (bold) | `#f1fa8c` |

To activate the theme: `Ctrl+K Ctrl+T` → select **Aurora Synthwave**
