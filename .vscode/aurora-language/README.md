# Aurora Language — VS Code Extension

Affects **only `.aura` files** — your existing VS Code theme stays untouched.

## Features

| Feature | Scope |
|---------|-------|
| **Syntax highlighting** | `.aura` files only |
| **Language Server (LSP)** | Completions, hover, goto-def, references, diagnostics, symbols, signature help, formatting, rename, folding, code actions, semantic tokens |
| **Auto-indent** | `.aura` files only |
| **Bracket matching** | `.aura` files only |
| **Auto-closing quotes** | `.aura` files only |
| **Comment toggling** | `.aura` files only |
| **Aurora Twilight theme** | Optional dark theme |
| **Font auto-install** | System-wide (needed for best look) |

## Installation

### From the Marketplace

Search for **Aurora Language** in the VS Code Extensions panel.

### From source

```powershell
Copy-Item -Recurse ".vscode\aurora-language" "$env:USERPROFILE\.vscode\extensions\"
```

Restart VS Code.

### From a `.vsix` package

```powershell
cd .vscode\aurora-language
npm install -g @vscode/vsce
vsce package
code --install-extension aurora-language-1.0.0.vsix
```

## Language Server

The extension auto-discovers the `aurora_lsp` binary (searching the repo
`build/` folders, then `PATH`). Point it explicitly if needed:

```json
"aurora.lsp.path": "C:\\path\\to\\aurora_lsp.exe"
"aurora.lsp.enabled": true
```

If `aurora_lsp` is not found, the extension still provides syntax
highlighting; language features (autocomplete, diagnostics, etc.) are
disabled until the binary is available.

## Publishing to the Marketplace

1. `cd .vscode\aurora-language`
2. `npm install -g @vscode/vsce`
3. `vsce login aurora-lang` (with a Publisher ID matching `package.json`)
4. `vsce publish`

`package.json` sets `"publisher": "aurora-lang"`; create that publisher at
https://marketplace.visualstudio.com/manage before publishing. The
extension is MIT-licensed (see `LICENSE`).

## Font Auto-Install

On first `.aura` file open, extension offers to install **JetBrains Mono**.
Or manually: `Ctrl+Shift+P` → **Aurora: Install JetBrains Mono Font**

## Syntax Highlighting

The grammar assigns proper TextMate scopes (`keyword`, `string`, `comment`, `function`, etc.) so your current theme colors `.aura` files correctly out of the box. No separate theme needed.
