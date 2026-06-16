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

# Catch without variable
try:
    body()
catch:
    handle_error()
```

## Try-Expression

```aura
result = try(might_fail(5))     # returns value or propagates
```

## Throw

```aura
throw "something went wrong"
throw 42                        # throw any value
```

## Ensure

`ensure` runs cleanup on scope exit (like `defer` in other languages):

```aura
ensure:
    cleanup()

# Or inline:
ensure cleanup()
```

## Panic

```aura
panic "fatal error"              # abort execution
panic "mock_call: unexpected"    # used in test mocks
```
