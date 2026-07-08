# App Development Best Practices

## Code Organization

### Project Structure
```
project/
  main.aura         # Entry point
  lib/              # Reusable modules
    database.aura
    networking.aura
  tests/            # Test files
    test_main.aura
  assets/           # Images, fonts, etc.
    icon.png
  app.json          # App metadata
  aurora.pkg        # Package manifest
```

### Naming Conventions
- **Files**: snake_case (`my_module.aura`)
- **Functions**: snake_case (`get_user_data`)
- **Variables**: snake_case (`user_count`)
- **Constants**: UPPER_SNAKE_CASE (`MAX_RETRIES`)

## Performance

### Startup Optimization
- Keep the main function minimal — initialize only what is needed
- Lazy-load heavy modules
- Use `aurorac -O2` for release builds

### Render Performance
- Keep widget count under 500 for mobile
- Use `layout_set_gap` instead of individual margins
- Batch UI updates in event handlers

### Memory
- The GC handles cleanup automatically
- Avoid creating objects in hot loops
- Set pointers to `null` when done to help GC

## UX Guidelines

### Cross-Platform
- Design for smallest screen first (mobile)
- Use flexbox layout for responsive sizing
- Test on both light and dark themes

### Platform Conventions
- **Windows**: Buttons on right side of dialogs
- **macOS**: Buttons on right, window close on left
- **Linux**: Follow GNOME or KDE conventions
- **Android**: Bottom navigation, Material Design
- **iOS**: Tab bar at bottom, back swipe gesture

### Accessibility
- Use `app_set_font_size` with relative sizes
- Ensure color contrast (WCAG AA minimum)
- Test with system font scaling

## Security

### Data Storage
- Use `permissions: ["storage"]` for file access
- Avoid hardcoding secrets
- Use the security module for encryption

### Network
- Use permissions: `["network"]` for HTTP
- Validate all server responses
- Use HTTPS in production

### User Input
- Validate input length before processing
- Use parameterized queries (via the db module)

## Testing

```python
import "test"

function test_addition()
    assert_equal(add(2, 3), 5)
end

function test_empty_input()
    assert_equal(process(""), null)
end
```

## Error Handling

```python
function safe_process(data)
    if data == null
        output("Error: null input")
        return null
    end
    result = process_inner(data)
    if result == null
        output("Error: processing failed")
    end
    return result
end
```

## Versioning

Follow semver:
- **Major**: Breaking API changes
- **Minor**: New features, backwards compatible
- **Patch**: Bug fixes

## Publishing Checklist

1. All tests pass
2. Version bumped in `aurora.pkg` and `app.json`
3. App icon and screenshots ready
4. Privacy policy created
5. Code signed for target platform
6. Tested on actual device/simulator
7. Package built and verified
