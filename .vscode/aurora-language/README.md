# Aurora Language — VS Code Extension

Auto-installs **JetBrains Mono** font and provides full Aurora language support with a vibrant neon theme.

## Installation

```powershell
Copy-Item -Recurse ".vscode\aurora-language" "$env:USERPROFILE\.vscode\extensions\"
```

Restart VS Code. Open any `.aura` file.

## Font Auto-Install

When you open your first `.aura` file, the extension checks if **JetBrains Mono** is installed:

- **Not installed** → A notification pops up: *"Install JetBrains Mono? "*
- Click **Install Font** → downloads + installs automatically
- Restart VS Code when done
- **Already installed** → no prompt, works silently

You can also trigger install manually anytime via:
- Command Palette (`Ctrl+Shift+P`) → **Aurora: Install JetBrains Mono Font**

## Variable Font Design

Same JetBrains Mono family, different weight/style per token type:

| Token type | Font style | Effect |
|------------|-----------|--------|
| Keywords (if, for, return...) | **Bold** | Strong presence |
| Declarations (function, class) | ***Bold Italic*** | Most important |
| Function/method calls | *Italic* | Active, dynamic |
| Parameters | *Italic* | Secondary, lighter |
| Attributes (`@name`) | **Bold Underline** | Special, unique |
| Strings, Numbers, Vars | Regular | Clean, readable |
| Comments | *Italic* | Classic style |
| Escape characters | **Bold** | Stands out |

## Color Palette

| Token | Color | Hex |
|-------|-------|-----|
| Control flow keywords | Hot Pink | `#ff79c6` |
| Logic keywords | Hot Pink | `#ff79c6` |
| Operators | Hot Pink | `#ff79c6` |
| `function` / `class` | Neon Green | `#50fa7b` |
| Types | Neon Green | `#50fa7b` |
| Function calls | Neon Green | `#50fa7b` |
| Strings | Lemon Yellow | `#f1fa8c` |
| Attributes | Gold bold | `#f1fa8c` |
| Builtins | Bright Cyan | `#8be9fd` |
| Async keywords | Bright Cyan | `#8be9fd` |
| Numbers | Soft Purple | `#bd93f9` |
| `true`/`false`/`null` | Hot Pink bold | `#ff79c6` |
| Comments | Muted Blue italic | `#6272a4` |
| OOP/module keywords | Lavender bold | `#c79aff` |
| Memory keywords | Peach bold | `#ffb86c` |
| Parameters | Peach italic | `#ffb86c` |
| Variables | Soft White | `#d4d6e8` |

To activate theme: `Ctrl+K Ctrl+T` → **Aurora Synthwave**
