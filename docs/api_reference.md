# Aurora API Reference (Phase 12.4)

## Widget Types (42 types)

| ID | Constant | Description |
|----|----------|-------------|
| 1 | `AURORA_WIDGET_WINDOW` | Top-level window |
| 2 | `AURORA_WIDGET_BUTTON` | Clickable button |
| 3 | `AURORA_WIDGET_LABEL` | Text label |
| 4 | `AURORA_WIDGET_TEXTBOX` | Single-line text input |
| 5 | `AURORA_WIDGET_LISTBOX` | List of selectable items |
| 6 | `AURORA_WIDGET_PASSWORDBOX` | Password input (masked) |
| 7 | `AURORA_WIDGET_CHECKBOX` | Checkbox toggle |
| 8 | `AURORA_WIDGET_RADIOBUTTON` | Radio button |
| 9 | `AURORA_WIDGET_SLIDER` | Horizontal slider |
| 10 | `AURORA_WIDGET_PROGRESSBAR` | Progress indicator |
| 11 | `AURORA_WIDGET_COMBOBOX` | Dropdown with editable text |
| 12 | `AURORA_WIDGET_DROPDOWN` | Dropdown selection |
| 13 | `AURORA_WIDGET_TREEVIEW` | Hierarchical tree |
| 14 | `AURORA_WIDGET_TABLE` | Data table |
| 15 | `AURORA_WIDGET_TABVIEW` | Tabbed container |
| 16 | `AURORA_WIDGET_SCROLLVIEW` | Scrollable container |
| 17 | `AURORA_WIDGET_CANVAS` | Custom drawing surface |
| 18 | `AURORA_WIDGET_IMAGE` | Image display |
| 19 | `AURORA_WIDGET_TOOLBAR` | Toolbar container |
| 20 | `AURORA_WIDGET_STATUSBAR` | Status bar |
| 21 | `AURORA_WIDGET_MENUBAR` | Menu bar |
| 22 | `AURORA_WIDGET_SPLITVIEW` | Split panel |
| 23 | `AURORA_WIDGET_SWITCH` | On/off switch |
| 24 | `AURORA_WIDGET_GROUPBOX` | Grouped container |
| 25 | `AURORA_WIDGET_DIALOG` | Modal dialog |
| 26-42 | Layout types | Row, Column, Stack, Grid, Wrap, Flow, Spacer, Padding, Margin, Center, Align, Expand, Flexible, Container, Divider, AspectRatio |

## Widget Functions

### Creation
```python
aurora_gui_window_new(title, w, h) -> ptr
aurora_gui_button_new(parent, text, x, y, w, h) -> ptr
aurora_gui_label_new(parent, text, x, y, w, h) -> ptr
aurora_gui_textbox_new(parent, x, y, w, h) -> ptr
aurora_gui_slider_new(parent, x, y, w, h) -> ptr
aurora_gui_switch_new(parent, x, y, w, h) -> ptr
aurora_gui_checkbox_new(parent, text, x, y, w, h) -> ptr
aurora_gui_progress_new(parent, x, y, w, h) -> ptr
aurora_gui_listbox_new(parent, x, y, w, h) -> ptr
aurora_gui_scrollview_new(parent, x, y, w, h) -> ptr
```

### Properties
```python
aurora_gui_widget_get_text(widget) -> str
aurora_gui_widget_get_type(widget) -> int
aurora_gui_widget_get_id(widget) -> int
aurora_gui_widget_get_bounds(widget, &x, &y, &w, &h)
aurora_gui_widget_get_parent(widget) -> ptr
aurora_gui_widget_count() -> int
aurora_gui_widget_get_by_index(idx) -> ptr
```

### Layout
```python
aurora_gui_column_new(parent, x, y, w, h) -> ptr
aurora_gui_row_new(parent, x, y, w, h) -> ptr
aurora_gui_column_set_spacing(col, spacing)
aurora_gui_row_set_spacing(row, spacing)
aurora_gui_widget_set_align(widget, align)
aurora_gui_widget_set_padding(widget, padding)
aurora_gui_widget_set_margin(widget, margin)
```

### Navigation
```python
aurora_app_nav_push(window, page)
aurora_app_nav_pop(window)
aurora_app_nav_replace(window, page)
```

### Theme
```python
aurora_app_theme_set_mode("light" | "dark")
aurora_app_theme_get_mode() -> str
aurora_app_theme_set_primary_color(hex)
aurora_app_theme_get_primary_color() -> str
```

## Auto-Update
```python
aurora_updater_init(app_name, version, url)
aurora_updater_check() -> bool
aurora_updater_download() -> bool
aurora_updater_apply() -> bool
```

## Virtual Scroll
```python
aurora_virtual_scroll_create(total, item_h, viewport_h) -> ptr
aurora_virtual_scroll_set_scroll_offset(vs, offset)
aurora_virtual_scroll_get_first_visible(vs) -> int
aurora_virtual_scroll_get_last_visible(vs) -> int
```

## Security
```python
aurora_app_sec_init()
aurora_app_sec_declare_permission(perm)
aurora_app_sec_check_permission(perm) -> bool
aurora_app_sec_sanitize_input(input, &out, size) -> int
aurora_app_sec_validate_path(path) -> bool
```
