# Error Handling

## Try / Catch / Finally

```aura
try
    result = risky_operation()
    output(result)
catch error
    output("Caught: " + error)
finally
    cleanup()

# With colons:
try:
    throw 42
catch err:
    outputf("Error: %d\n", err)

# Brace blocks:
try {
    throw "error"
} catch err {
    output(err)
} finally {
    cleanup()
}

# Catch with parentheses:
try:
    throw 42
catch(err):
    outputf("Error: %d\n", err)
```

### Catch without variable (ignore error value)

```aura
try:
    body()
catch:
    handle_error()
```

## Try-Expression

```aura
result = try(might_fail(5))     # returns value or propagates error
```

## Throw

```aura
throw "something went wrong"
throw 42                        # throw any value (int, string, object...)
throw Error("timeout")          # custom error pattern
throw                           # re-throw current error (inside catch)
```

## Ensure / Finally

`ensure` and `finally` are **aliases** — they run cleanup on scope exit (like `defer` in Go):

```aura
try:
    body()
ensure:
    cleanup()

try:
    body()
finally:
    cleanup()                   # same as ensure
```

### Inline ensure

```aura
ensure cleanup()

# Practical: ensure file is closed
function read_file(path)
    file = open(path)
    ensure close(file)           # runs even if an error occurs
    return read_all(file)
```

## Panic

```aura
panic "fatal error"              # abort execution immediately
panic "mock_call: unexpected"    # used in test mocks
```

> **Difference:** `throw` is catchable; `panic` is not. Use `panic` for unrecoverable errors, `throw` for recoverable ones.

---

**Next:** [Async & Concurrency](11-async-concurrency.md)
