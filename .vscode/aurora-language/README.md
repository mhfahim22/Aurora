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

## Color Palette (Aurora Twilight)

A modern, aesthetic palette — warm deep-indigo background with curated pastel accents.

| Token | Color | Hex |
|-------|-------|-----|
| **Background** | Deep Indigo Night | `#1a1b2e` |
| **Foreground** | Warm Soft White | `#cdd6f4` |
| **Comments** | Muted Gray *italic* | `#585b70` |
| **Control flow / Logic** | Soft Lavender **bold** | `#cba6f7` |
| **Exceptions** | Soft Rose **bold** | `#f38ba8` |
| `function` / `class` | Soft Blue ***bold italic*** | `#89b4fa` |
| **Function calls** | Soft Blue *italic* | `#89b4fa` |
| **Types** | Soft Teal **bold** | `#94e2d5` |
| **Strings** | Sage Green | `#a6e3a1` |
| **Numbers** | Warm Peach | `#fab387` |
| **Parameters** | Warm Peach *italic* | `#fab387` |
| `true` / `false` / `null` | Soft Rose **bold** | `#f38ba8` |
| **Operators** | Light Cyan | `#89dceb` |
| **Builtins** | Sky Blue | `#74c7ec` |
| **Async keywords** | Light Cyan **bold** | `#89dceb` |
| **OOP / Module** | Periwinkle **bold** | `#b4befe` |
| **Memory keywords** | Soft Cream **bold** | `#f9e2af` |
| **Attributes** (`@name`) | Soft Cream **bold underline** | `#f9e2af` |

To activate theme: `Ctrl+K Ctrl+T` → **Aurora Twilight**
