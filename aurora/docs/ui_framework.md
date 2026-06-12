# Aurora UI Framework

A component-based UI framework with layout, styling, animations, and cross-platform rendering.

---

## 1. Core Concepts

### Component

A component is a reusable UI element with state, properties, render logic, and lifecycle.

```aura
component Counter
    state count = 0
    properties title = "Counter"

    function increment()
        self.count += 1
        render()

    function render()
        output("Button: " + self.title + " (" + self.count + ")")

Counter("My Counter")
```

### Component Tree

Components can contain child components:

```aura
component App
    function render()
        Layout("column")
            Header("My App")
            Body("Content here")
            Footer("Copyright 2026")

component Header
    properties title = ""

    function render()
        output("Header: " + self.title)
```

### Lifecycle Methods

| Method | Called When |
|--------|-------------|
| `init()` | Component created |
| `render()` | Component should render |
| `update(props)` | Properties changed |
| `destroy()` | Component being removed |

```aura
component LifecycleDemo
    function init()
        output("created")

    function render()
        output("rendering")

    function destroy()
        output("destroyed")
```

---

## 2. Layout

### `<Layout>` Component

```aura
# Vertical layout (default)
Layout("column")
    Child1()
    Child2()
    Child3()

# Horizontal layout
Layout("row")
    Child1()
    Child2()
```

---

## 3. Styling

### Style Keyword

```aura
style my_button
    background = "#3498db"
    color = "#ffffff"
    padding = "8px 16px"
    border_radius = "4px"

component StyledButton
    function render()
        Style(my_button)
        output("Click me")
```

### Theme Keyword

```aura
theme dark_theme
    primary = "#bb86fc"
    secondary = "#03dac6"
    background = "#121212"
    surface = "#1e1e1e"
    error = "#cf6679"
    on_primary = "#000000"

component ThemedApp
    function render()
        Theme(dark_theme)
        output("Themed Content")
```

---

## 4. Routes & Pages

```aura
route "/home"
    render_home()

route "/profile/:username"
    render_profile(username)

route "/settings"
    render_settings()

page Dashboard
    function render()
        output("Dashboard")
```

---

## 5. Animation & Transition

```aura
animate fade_in
    from opacity = 0
    to opacity = 1
    duration = 300

animate slide_in
    from x = -100
    to x = 0
    duration = 200

component AnimatedBox
    function render()
        Animate(fade_in)
        output("Fading in")
```

```aura
transition button_hover
    from background = "#3498db"
    to background = "#2980b9"
    duration = 150

component HoverButton
    function render()
        Transition(button_hover)
        output("Hover me")
```

---

## 6. State Management

Local state is declared with `state`:

```aura
component Toggle
    state on = false

    function toggle()
        self.on = not self.on
        render()

    function render()
        if self.on
            output("ON")
        else
            output("OFF")

Toggle()
```

---

## 7. Cross-Platform Rendering

The framework supports:
- **Windows**: Win32 GUI (native windows, GDI rendering)
- **Linux**: X11 backend
- **macOS**: Cocoa backend (NSWindow, NSButton, NSTextField, NSTableView)

Selected automatically at compile time via platform detection.

```aura
import gui

function main()
    win = gui_window_new("My Window", 800, 600)
    btn = gui_button_new(win, "Click Me", 10, 10, 100, 30)
    gui_run()
```

---

## 8. API Reference

### Component Functions

| Function | Description |
|----------|-------------|
| `aurora_component_create()` | Create new component |
| `aurora_component_destroy(comp)` | Destroy component |
| `aurora_component_add_child(parent, child)` | Add child component |
| `aurora_component_set_render_fn(comp, fn)` | Set render callback |
| `aurora_component_set_state(comp, key, value)` | Set state value |
| `aurora_component_get_state(comp, key)` | Get state value |
| `aurora_component_mount(comp)` | Mount component to tree |
| `aurora_component_render_tree(comp)` | Render entire tree |
| `aurora_component_update_tree(comp)` | Update entire tree |

### Style Functions

| Function | Description |
|----------|-------------|
| `aurora_style_apply(style_name)` | Apply named style |
| `aurora_style_set(key, value)` | Set style property |

### Route Functions

| Function | Description |
|----------|-------------|
| `aurora_route_register(path, handler)` | Register route |
| `aurora_route_navigate(path)` | Navigate to route |
| `aurora_route_params()` | Get current route params |

### Animation Functions

| Function | Description |
|----------|-------------|
| `aurora_animation_play(name)` | Play animation |
| `aurora_animation_stop(name)` | Stop animation |
| `aurora_animation_set(name, key, value)` | Set animation property |
