# Aurora Developer Tools

## Formatter
The Aurora formatter automatically formats `.aura` source files.

```aura
extern function aurora_format_code(source: str, options: str): str
```

Options JSON:
```json
{"indent": 4, "max_line_length": 100}
```

## Linter
The Aurora linter checks code for common issues.

```aura
extern function aurora_lint_code(source: str): str
```

Returns JSON array of diagnostics:
```json
[{"line": 5, "col": 3, "severity": "warning", "message": "...", "rule_id": "no-unused-variable"}]
```

## Debugger
The Aurora debugger supports breakpoints, stepping, and variable inspection.

```aura
extern function aurora_debugger_attach(process_id: str): int
extern function aurora_debugger_detach(): int
extern function aurora_debugger_set_breakpoint(file: str, line: int): int
extern function aurora_debugger_get_stack_trace(): str
extern function aurora_debugger_get_variables(): str
```

## Profiler
The Aurora profiler records execution hotspots.

```aura
extern function aurora_profiler_start()
extern function aurora_profiler_stop()
extern function aurora_profiler_get_report(): str
```

## Test Suite
Test files in `Workflow/tests/`:
- **Web framework**: `test_web_server.aura` through `test_web_middleware.aura` (12 files)
- **Desktop GUI**: `test_linux_gui.aura`, `test_mac_gui.aura`, `test_linux_desktop.aura`, `test_mac_desktop.aura`
- **Complex widgets**: `test_webview.aura`, `test_media.aura`, `test_map.aura`
- **Developer tools**: `test_formatter.aura`, `test_linter.aura`, `test_debugger.aura`, `test_profiler.aura`
