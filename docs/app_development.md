# App Development Guide

## Overview

Aurora lets you build cross-platform apps (Windows, Linux, macOS, Android, iOS) from a single codebase. This guide walks through the full development workflow.

## Quick Start

Create a new app with voss:

```bash
voss new my-app --template desktop-app
cd my-app
voss run main.aura
```

Available templates:
- `desktop-app` — Windows/Linux/macOS GUI
- `mobile-app` — Android/iOS with touch input
- `cross-app` — All 5 platforms from one codebase

## Project Structure

```
my-app/
  aurora.pkg          # Package manifest
  main.aura           # Entry point
  app.json            # App metadata (icon, version, orientation)
  build.sh            # Platform build script
  .gitignore
```

### aurora.pkg

```yaml
name: my-app
version: 0.1.0
description: My Aurora application
entry: main.aura
dependencies:
permissions:
  - ui
  - network
```

### app.json

```json
{
  "name": "my-app",
  "version": "0.1.0",
  "icon": "icon.png",
  "orientation": "portrait",
  "splash_screen": true
}
```

## Writing an App

### Desktop App

```python
import app

function main()
    win = app_init("My App", 800, 600)
    lbl = app_label(win, "Hello World", 10, 10, 780, 30)
    btn = app_button(win, "Click", 10, 50, 120, 30)
    app_on_click(btn, lambda() app_set_text(lbl, "Clicked!") end)
    app_run(win)
end
```

### Mobile App

```python
import app

function main()
    win = app_init("My App", 0, 0)
    btn = app_button(win, "Tap", 10, 50, 200, 40)
    app_on_click(btn, lambda() output("tapped!") end)
    app_run(win)
end
```

### Cross-Platform App

```python
import app

function main()
    win = app_init("My App", 400, 500)
    col = layout_column(win)
    lbl = app_label(col, "Welcome", 0, 0, 360, 40)
    app_set_font_size(lbl, 24)
    btn = app_button(col, "Click", 0, 0, 200, 44)
    app_on_click(btn, lambda()
        counter = counter + 1
        app_set_text(lbl, "Count: " + counter)
    end)
    theme_set_light()
    app_run(win)
end
```

## Layout System

Use flexbox-style layouts for responsive UIs:

```python
col = layout_column(parent)
row = layout_row(parent)

# Alignment
layout_justify_center(col)
layout_align_center(col)

# Spacing
layout_set_padding(col, 16, 16, 16, 16)
layout_set_margin(col, 0, 0, 0, 0)
```

Functions:
- `layout_column(parent)` — vertical stack
- `layout_row(parent)` — horizontal stack
- `layout_justify_start/center/end/space_between/space_around/space_evenly`
- `layout_align_start/center/end/stretch`
- `layout_set_padding/margin(top, right, bottom, left)`
- `layout_set_gap(container, gap)`
- `layout_set_grow/shrink(child, factor)`

## Navigation

```python
nav_register("home", home_screen)
nav_register("settings", settings_screen)
nav_push("home")
nav_push("settings")
nav_pop()
nav_replace("home")
```

## Theming

```python
# Light mode
theme_set_light()

# Dark mode
theme_set_dark()

# Color keys
theme_get_color("primary")
theme_get_color("background")
theme_get_color("text")
theme_get_color("error")
theme_get_color("surface")
```

Color keys: primary, secondary, background, surface, error, on_primary, on_secondary, on_background, on_surface, on_error, text

Font levels: display_large, display_medium, display_small, headline, title, body_large, body_medium, body_small, label_large, label_medium, label_small

## Building

```bash
# JIT run (development)
aurorac main.aura --run

# AOT compile (production)
aurorac main.aura -o myapp

# Cross-compile
aurorac main.aura -o myapp --target x86_64-pc-windows-msvc
aurorac main.aura -o myapp_linux --target x86_64-unknown-linux-gnu
aurorac main.aura -o myapp_mac --target aarch64-apple-darwin

# Mobile targets
aurorac main.aura -o libmyapp.so --shared --target aarch64-linux-android
aurorac main.aura -o libmyapp.a --static --target arm64-apple-ios
```

## Testing

Phase 1 tests verify core functionality:

```bash
aurorac Workflow/tests/test_json.aura --run
aurorac Workflow/tests/test_gc.aura --run
aurorac Workflow/tests/test_thread.aura --run
aurorac Workflow/tests/test_borrow.aura --run
```

## Best Practices

1. **Single-line calls** — The parser does not support multi-line function calls
2. **No && operator** — Use nested if statements instead
3. **Use null, not nil** — Aurora uses `null` for null pointer
4. **Import app** — Always start with `import "app"` for GUI apps
5. **Single entry point** — Define a `main()` function and call it at module level
