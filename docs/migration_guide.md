# Migration Guide (Phase 12.4)

## From Electron

| Electron Concept | Aurora Equivalent |
|-----------------|-------------------|
| `BrowserWindow` | `aurora_gui_window_new()` |
| `ipcMain/ipcRenderer` | Direct function calls (single process) |
| `package.json` | `aurora.pkg` manifest |
| `npm install` | `voss install` |
| `webpack` | Built-in compiler |
| `main.js` | `main.aura` |
| React components | Widget functions + layout |

**Migration strategy**:
1. Create `aurora.pkg` with app metadata
2. Replace `BrowserWindow` with `aurora_gui_window_new()`
3. Replace HTML/CSS with widget functions and layout
4. Replace IPC with direct Aurora function calls

## From Flutter

| Flutter Concept | Aurora Equivalent |
|----------------|-------------------|
| `MaterialApp` | `app_init()` |
| `Scaffold` | Column + layout widgets |
| `Text` | `aurora_gui_label_new()` |
| `ElevatedButton` | `aurora_gui_button_new()` |
| `setState()` | Widget property setters |
| `pubspec.yaml` | `aurora.pkg` |
| Hot Reload | `aurora_hotreload_*` + GUI hot reload |

## From React Native

| React Native Concept | Aurora Equivalent |
|---------------------|-------------------|
| `View` | `aurora_gui_column_new()` / `aurora_gui_row_new()` |
| `Text` | `aurora_gui_label_new()` |
| `TextInput` | `aurora_gui_textbox_new()` |
| `ScrollView` | `aurora_gui_scrollview_new()` |
| `Switch` | `aurora_gui_switch_new()` |
| `Slider` | `aurora_gui_slider_new()` |
| `package.json` | `aurora.pkg` |
| JSX | Aurora syntax (Python-like) |

## Key Differences

1. **Single language**: Aurora is both frontend and backend — no separate JS/CSS/HTML
2. **Single binary**: Compile to native executable (no Node.js or browser engine)
3. **Native widgets**: Uses platform-native controls, not web rendering
4. **Cross-platform**: Same codebase compiles for all 5 platforms
