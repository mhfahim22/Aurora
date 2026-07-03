# Best Practices

Recommended conventions and patterns for Aurora code.

## Naming Conventions

| Construct | Convention | Example |
|-----------|-----------|---------|
| Variables | `snake_case` | `user_name` |
| Functions | `snake_case` | `get_user()` |
| Components | `PascalCase` | `UserProfile` |
| Constants | `SCREAMING_SNAKE` | `MAX_SIZE` |
| Types | `PascalCase` | `UserRecord` |
| Files | `snake_case` | `user_profile.aura` |

## Code Organization

### Keep files focused
One module per file. Group related functions.

```aura
# math_utils.aura
fn add(a, b) { a + b }
fn sub(a, b) { a - b }
```

### Use modules for namespacing
```aura
module "math"
fn add(a, b) { a + b }
```

## Error Handling

### Prefer assertions in tests
```aura
assert_eq(computed, expected)
assert_gt(count, 0)
```

### Use early returns for validation
```aura
fn divide(a, b)
    if b == 0
        error("division by zero")
    a / b
```

## Database

### Always close connections
```aura
db = db_open("data.db")
# ... work ...
db_close(db)
```

### Use transactions for multi-step operations
```aura
db_begin(db)
# multiple writes...
db_commit(db)  # or db_rollback(db) on error
```

### Use prepared statements for repeated queries
```aura
stmt = db_prepare(db, "INSERT INTO t VALUES (?)")
for i in range(100)
    db_bind_int(stmt, 1, i)
    db_step(stmt)
    db_reset(stmt)
db_finalize(stmt)
```

## Performance

### Batch file operations
```aura
# Prefer: one write call
write("out.txt", large_content)
```

### Use serialization format appropriate to data size
- JSON for human-readable config
- Binary for compact/performance-critical data

### Use benchmarking to identify bottlenecks
```aura
bench_start("parse")
    data = decode(json_str)
bench_end("parse")
```

## Testing

### Structure tests by feature
```aura
test_suite("UserModule")
test_case("create_user")
    assert_eq(create_user("Alice"), true)
test_case("delete_user")
    assert_eq(delete_user(1), true)
```

### Use snapshot tests for complex output
```aura
snap_shot("render_result", rendered_html)
snap_assert("render_result")
```

## Platform-Specific Code

### Guard platform-dependent code
```aura
if os() == "windows"
    desktop_clipboard_set_text("hello")
```

### Use mobile widgets for cross-platform UIs
```aura
btn = mw_create(MW_BUTTON, parent)
mw_set_text(btn, "Click me")
```

## Hot Reload Workflow

### Watch project directories during development
```aura
hr_watch("src/")
hr_on_code_change(lambda()
    hr_code_reload()
)
```

### Preserve state across reloads
```aura
hr_state_set("scroll_pos", str(scroll_y))
# after reload:
scroll_y = int(hr_state_get("scroll_pos"))
```
