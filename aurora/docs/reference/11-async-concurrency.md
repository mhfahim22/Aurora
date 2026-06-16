# Async & Concurrency

## Async Function

```aura
async function fetch_data(url)
    result = http_get(url)
    return result

async function simple_async():
    output("Async body")

async:
    output("Async block")           # async block (standalone)
```

## Await / Wait

```aura
data = await fetch_data("https://example.com")
result = wait task                  # wait is synonym for await
```

## Spawn

```aura
task = spawn long_running()
task = spawn(fetch_data())          # function-call style
result = wait task
```

## Parallel

```aura
parallel
    task1()
    task2()
    task3()
```

## Channels

```aura
ch = chan()                         # unbuffered channel
ch = chan(5)                        # buffered channel (capacity 5)
send(ch, "hello")
msg = recv(ch)                      # blocking receive
```

## Fibers

```aura
f = fiber_create(my_function, arg)  # create fiber
fiber_resume(f)                     # resume execution
done = fiber_is_done(f)             # check completion
result = fiber_get_result(f)        # get return value
fiber_destroy(f)                    # cleanup
fiber_yield()                       # yield control (within fiber)
```

## Event Bus

```aura
event_on("user.login", handler)
event_emit("user.login", data)
event_off("user.login", handler)
```

## Signal / Emit (alternative event syntax)

```aura
signal my_signal                    # declare signal
emit my_signal(42)                  # emit signal with args
```

## Event Declaration

```aura
event my_event:
    # handler body
```

## Thread

```aura
thread expr                         # spawn OS thread
```

## Built-in Async Functions

| Function              | Description                  |
|-----------------------|------------------------------|
| `spawn(expr)`         | Spawn async task             |
| `await(expr)`         | Wait for async result        |
| `chan(size)`          | Create channel               |
| `send(ch, val)`       | Send to channel              |
| `recv(ch)`            | Receive from channel         |
| `fiber_create(f, a)`  | Create fiber                 |
| `fiber_resume(f)`     | Resume fiber                 |
| `fiber_yield()`       | Yield fiber                  |
| `fiber_is_done(f)`    | Check fiber done             |
| `fiber_get_result(f)` | Get fiber result             |
| `fiber_destroy(f)`    | Destroy fiber                |
| `event_on(n, h)`      | Register event handler       |
| `event_emit(n, a)`    | Emit event                   |
| `event_off(n, h)`     | Remove event handler         |
