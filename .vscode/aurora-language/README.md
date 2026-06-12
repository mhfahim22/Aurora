# Aurora Language — VS Code Extension

## Installation

```powershell
# Copy to VS Code extensions folder
Copy-Item -Recurse ".vscode\aurora-language" "$env:USERPROFILE\.vscode\extensions\"
```

Restart VS Code. Open any `.aura` file — syntax highlighting activates automatically.

## Font Setup (JetBrains Mono)

This extension uses **variable font** properties — same JetBrains Mono family, different weights and styles per token type:

| Token type | Font style | Visual effect |
|------------|-----------|---------------|
| Keywords (if, for, return...) | **Bold** | Strong, commanding |
| Declarations (function, class) | ***Bold Italic*** | Important + dynamic |
| Function calls | *Italic* | Light, active |
| Parameters | *Italic* | Softer, secondary |
| Attributes (@name) | **Bold Underline** | Distinctive, special |
| Comments | *Italic* | Classic comment style |
| Strings, Numbers, Variables | Regular | Clean, readable |
| Escape characters | **Bold** | Stand out in strings |

### Install JetBrains Mono

1. Download from: https://www.jetbrains.com/lp/mono/
2. Install the font (Windows: right-click → Install)
3. VS Code will auto-use it via the project `.vscode/settings.json`

If JetBrains Mono isn't installed, it falls back to Cascadia Code → Fira Code → Source Code Pro → Consolas.

## Features

| Feature | Description |
|---------|-------------|
| **Syntax highlighting** | Full TextMate grammar — all Aurora tokens |
| **Variable font** | JetBrains Mono with weight/style variations |
| **Auto-indent** | Increases indent after `:` |
| **Bracket matching** | `()`, `[]`, `{}` — colorized pairs |
| **Auto-closing** | Quotes and brackets auto-close |
| **Comment toggling** | `#` line, `/* */` block |
| **Custom theme** | **Aurora Synthwave** — dark neon, retina-popping colors |

## Color Palette

| Token | Color | Hex |
|-------|-------|-----|
| Control flow keywords | Hot Pink | `#ff79c6` |
| Logic keywords | Hot Pink | `#ff79c6` |
| Exception keywords | Hot Pink | `#ff79c6` |
| Operators | Hot Pink | `#ff79c6` |
| `function` / `class` / `lambda` | Neon Green | `#50fa7b` |
| Types (int, string, list...) | Neon Green | `#50fa7b` |
| Function definitions | Neon Green | `#50fa7b` |
| Function calls | Neon Green | `#50fa7b` |
| Strings | Lemon Yellow | `#f1fa8c` |
| Attributes | Gold (bold) | `#f1fa8c` |
| Builtins (output, len, push...) | Bright Cyan | `#8be9fd` |
| Async keywords (async, await...) | Bright Cyan | `#8be9fd` |
| Methods | Bright Cyan | `#8be9fd` |
| Numbers | Soft Purple | `#bd93f9` |
| `true` / `false` / `null` | Hot Pink | `#ff79c6` |
| Comments | Muted Blue *italic* | `#6272a4` |
| OOP keywords | Lavender bold | `#c79aff` |
| Module keywords | Lavender bold | `#c79aff` |
| Memory keywords | Peach bold | `#ffb86c` |
| Parameters | Peach *italic* | `#ffb86c` |
| Variables | Soft White | `#d4d6e8` |
| Escape chars | Hot Pink bold | `#ff79c6` |

To activate the theme: `Ctrl+K Ctrl+T` → **Aurora Synthwave**
