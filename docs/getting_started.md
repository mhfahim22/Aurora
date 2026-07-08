# Aurora Getting Started Guide (Phase 12.4)

## 5-Minute Quick Start

### 1. Install Aurora

**Windows**:
```bash
# Download the installer from the releases page
# Or use the one-command installer:
curl -fsSL https://aurora-lang.dev/install.ps1 | powershell -c -
```

**Linux/macOS**:
```bash
curl -fsSL https://aurora-lang.dev/install.sh | sh
```

### 2. Create Your First App

```bash
mkdir hello_aurora && cd hello_aurora
voss init HelloApp
```

### 3. Write Your App

Create `main.aura`:
```python
import "app"
import "gui"

function main()
    win = app_init("Hello Aurora", 400, 300)
    aurora_gui_label_new(win, "Hello from Aurora!", 50, 50, 300, 30)
    btn = aurora_gui_button_new(win, "Click Me", 50, 100, 100, 30)
    aurora_gui_button_set_callback(btn, lambda()
        output("Button clicked!")
    end)
    app_run(win)
end
```

### 4. Run It

```bash
aurorac main.aura --run
```

### 5. Build for Distribution

```bash
voss bundle --target windows
voss bundle --target linux
voss bundle --target macos
```

## Next Steps

- [API Reference](api_reference.md) — All widget types and functions
- [Platform Build Guides](platform_guides.md) — Build for Android, iOS, etc.
- [Cookbook](cookbook.md) — Common patterns and recipes
- [Best Practices](app_best_practices.md) — Idiomatic Aurora
- [Migration Guide](migration_guide.md) — From other frameworks
