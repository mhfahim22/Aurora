# Aurora API Reference

Complete reference for all built-in functions, organized by category.

---

## I/O

| Function | Signature | Description |
|----------|-----------|-------------|
| `output` | `output(value)` | Print value to stdout |
| `outputln` | `outputln(value)` | Print with newline |
| `outputN` | `outputN(n, values...)` | Print N values |
| `outputf` | `outputf(fmt, args...)` | Formatted print |
| `input` | `input() -> string` | Read line from stdin |
| `ask` | `ask(prompt) -> string` | Prompt and read input |

```aura
output("hello")
name = ask("Enter name: ")
```

---

## String

| Function | Signature | Description |
|----------|-----------|-------------|
| `len` | `len(s) -> int` | String length |
| `upper` | `upper(s) -> string` | Convert to uppercase |
| `lower` | `lower(s) -> string` | Convert to lowercase |
| `trim` | `trim(s) -> string` | Strip whitespace |
| `replace` | `replace(s, from, to) -> string` | Replace substring |
| `split` | `split(s, sep) -> array` | Split by separator |
| `join` | `join(arr, sep) -> string` | Join array into string |
| `has` | `has(s, sub) -> bool` | Check if contains substring |
| `starts` | `starts(s, prefix) -> bool` | Check prefix |
| `ends` | `ends(s, suffix) -> bool` | Check suffix |
| `reverse` | `reverse(s) -> string` | Reverse string |
| `strlen` | `strlen(s) -> int` | C string length |
| `strcat` | `strcat(a, b) -> string` | Concatenate strings |
| `substr` | `substr(s, start, len) -> string` | Extract substring |
| `index` | `index(s, sub) -> int` | Find substring position |

```aura
s = "Hello, World!"
output(len(s))           # 13
output(upper(s))         # "HELLO, WORLD!"
output(split(s, ", "))   # ["Hello", "World!"]
```

---

## Math

| Function | Signature | Description |
|----------|-----------|-------------|
| `abs` | `abs(x) -> number` | Absolute value |
| `sqrt` | `sqrt(x) -> float` | Square root |
| `floor` | `floor(x) -> int` | Round down |
| `ceil` | `ceil(x) -> int` | Round up |
| `round` | `round(x) -> int` | Round to nearest |
| `pow` | `pow(base, exp) -> float` | Power |
| `clamp` | `clamp(x, min, max) -> number` | Clamp to range |
| `rand` | `rand(min, max) -> int` | Random integer |
| `sum` | `sum(arr) -> number` | Sum of array |
| `min` | `min(a, b) -> number` | Minimum |
| `max` | `max(a, b) -> number` | Maximum |

```aura
output(sqrt(144))        # 12
output(pow(2, 10))       # 1024
output(clamp(150, 0, 100))  # 100
output(rand(1, 6))       # random dice roll
```

---

## Type Conversion

| Function | Signature | Description |
|----------|-----------|-------------|
| `str` | `str(value) -> string` | Convert to string |
| `int` | `int(value) -> int` | Convert to integer |
| `float` | `float(value) -> float` | Convert to float |
| `bool` | `bool(value) -> bool` | Convert to boolean |
| `char` | `char(code) -> string` | Convert ASCII code to char |
| `convert` | `convert(value, type) -> any` | Generic type conversion |
| `clone` | `clone(value) -> any` | Deep copy |
| `typeof` | `typeof(value) -> string` | Get type name |
| `sizeof` | `sizeof(value) -> int` | Get size in bytes |

```aura
output(str(42))          # "42"
output(int("3.14"))      # 3
output(typeof("hello"))  # "string"
```

---

## Collections

| Function | Signature | Description |
|----------|-----------|-------------|
| `push` | `push(arr, value)` | Append to array |
| `pop` | `pop(arr) -> any` | Remove and return last |
| `insert` | `insert(arr, idx, value)` | Insert at index |
| `remove` | `remove(arr, idx)` | Remove at index |
| `clear` | `clear(arr)` | Clear all elements |
| `sort` | `sort(arr)` | Sort in place |
| `unique` | `unique(arr) -> array` | Remove duplicates |
| `map` | `map(arr, fn) -> array` | Transform each element |
| `filter` | `filter(arr, fn) -> array` | Keep matching elements |
| `reduce` | `reduce(arr, fn, init) -> any` | Accumulate |
| `find` | `find(arr, value) -> int` | Find index of value |
| `any` | `any(arr, fn) -> bool` | Any element matches |
| `all` | `all(arr, fn) -> bool` | All elements match |
| `range` | `range(end) -> array` | Generate number range |

```aura
numbers = [1, 2, 3, 4, 5]
doubled = map(numbers, lambda(x) x * 2)      # [2, 4, 6, 8, 10]
evens = filter(numbers, lambda(x) x % 2 == 0) # [2, 4]
sum = reduce(numbers, lambda(a, b) a + b, 0)  # 15
output(any(numbers, lambda(x) x > 3))         # true
```

---

## Runtime Collections

| Function | Signature | Description |
|----------|-----------|-------------|
| `list_get` | `list_get(l, idx)` | Get from list |
| `list_len` | `list_len(l) -> int` | List length |
| `list_push` | `list_push(l, val)` | Push to list |
| `list_free` | `list_free(l)` | Free list |
| `map_get` | `map_get(m, key)` | Get from map |
| `map_has` | `map_has(m, key) -> bool` | Map has key |
| `map_set` | `map_set(m, key, val)` | Set in map |
| `map_free` | `map_free(m)` | Free map |
| `set_add` | `set_add(s, val)` | Add to set |
| `set_has` | `set_has(s, val) -> bool` | Set has value |
| `set_free` | `set_free(s)` | Free set |
| `stack_push` | `stack_push(s, val)` | Push to stack |
| `stack_pop` | `stack_pop(s) -> any` | Pop from stack |
| `stack_empty` | `stack_empty(s) -> bool` | Stack is empty |
| `stack_free` | `stack_free(s)` | Free stack |
| `queue_enqueue` | `queue_enqueue(q, val)` | Enqueue |
| `queue_dequeue` | `queue_dequeue(q) -> any` | Dequeue |
| `queue_empty` | `queue_empty(q) -> bool` | Queue is empty |
| `queue_free` | `queue_free(q)` | Free queue |
| `vector_x` | `vector_x(v) -> float` | Vector X component |
| `vector_y` | `vector_y(v) -> float` | Vector Y component |
| `vector_z` | `vector_z(v) -> float` | Vector Z component |
| `json_free` | `json_free(j)` | Free JSON value |

```aura
m = map()
map_set(m, "name", "Alice")
output(map_get(m, "name"))    # "Alice"
```

---

## File I/O

| Function | Signature | Description |
|----------|-----------|-------------|
| `read` | `read(path) -> string` | Read entire file |
| `write` | `write(path, data)` | Write to file |
| `append` | `append(path, data)` | Append to file |
| `exists` | `exists(path) -> bool` | File exists |
| `delete` | `delete(path)` | Delete file |
| `copy` | `copy(src, dst)` | Copy file |
| `move` | `move(src, dst)` | Move file |
| `download` | `download(url, path)` | Download file |

```aura
data = read("input.txt")
write("output.txt", "Hello")
```

---

## Path

| Function | Signature | Description |
|----------|-----------|-------------|
| `cwd` | `cwd() -> string` | Current working directory |
| `cd` | `cd(path)` | Change directory |
| `path` | `path(parts...) -> string` | Join path components |
| `name` | `name(path) -> string` | File name from path |
| `ext` | `ext(path) -> string` | File extension |
| `dir` | `dir(path) -> string` | Directory from path |

---

## Time

| Function | Signature | Description |
|----------|-----------|-------------|
| `now` | `now() -> string` | Current date/time string |
| `stamp` | `stamp() -> int` | Millisecond timestamp |
| `sleep` | `sleep(ms)` | Sleep for milliseconds |

```aura
start = stamp()
sleep(1000)
output("Elapsed: " + (stamp() - start) + "ms")
```

---

## JSON

| Function | Signature | Description |
|----------|-----------|-------------|
| `encode` | `encode(value) -> string` | Encode to JSON string |
| `decode` | `decode(json_str) -> any` | Decode JSON string |

```aura
data = decode('{"name": "Alice", "age": 30}')
output(data.name)
```

---

## HTTP

| Function | Signature | Description |
|----------|-----------|-------------|
| `get` | `get(url) -> string` | HTTP GET request |
| `post` | `post(url, body) -> string` | HTTP POST request |

---

## OS / Environment

| Function | Signature | Description |
|----------|-----------|-------------|
| `os` | `os() -> string` | OS name |
| `cpu` | `cpu() -> int` | CPU count |
| `mem` | `mem() -> int` | Memory usage |
| `env` | `env(name) -> string` | Environment variable |
| `run` | `run(cmd) -> int` | Run shell command |
| `exit` | `exit(code)` | Exit process |

---

## Error / Debug

| Function | Signature | Description |
|----------|-----------|-------------|
| `error` | `error(msg)` | Raise error |
| `debug` | `debug(value)` | Debug output |
| `panic` | `panic(msg)` | Fatal error (aborts) |

---

## Async

| Function | Signature | Description |
|----------|-----------|-------------|
| `spawn` | `spawn(fn) -> task` | Spawn async task |
| `await` | `await(task) -> any` | Await task result |
| `chan` | `chan() -> channel` | Create channel |
| `send` | `send(ch, value)` | Send to channel |
| `recv` | `recv(ch) -> any` | Receive from channel |

```aura
task = spawn(lambda() long_computation())
result = await(task)
```

---

## Event Bus

| Function | Signature | Description |
|----------|-----------|-------------|
| `event_on` | `event_on(name, handler)` | Register event handler |
| `event_off` | `event_off(name, handler)` | Unregister handler |
| `event_emit` | `event_emit(name, data)` | Emit event |

```aura
event_on("user.login", handler)
event_emit("user.login", {"user": "Alice"})
```

---

## Fiber

| Function | Signature | Description |
|----------|-----------|-------------|
| `fiber_create` | `fiber_create(fn) -> fiber` | Create fiber |
| `fiber_resume` | `fiber_resume(f)` | Resume fiber |
| `fiber_yield` | `fiber_yield(value)` | Yield from fiber |
| `fiber_is_done` | `fiber_is_done(f) -> bool` | Fiber completed |
| `fiber_get_result` | `fiber_get_result(f) -> any` | Get fiber result |
| `fiber_destroy` | `fiber_destroy(f)` | Destroy fiber |

---

## Performance

| Function | Signature | Description |
|----------|-----------|-------------|
| `measure` | `measure(fn) -> int` | Measure execution time (ns) |
| `bench` | `bench(fn, iterations) -> int` | Benchmark (avg ns) |
| `profile` | `profile(fn) -> string` | Profile function |
| `trace` | `trace(value)` | Trace execution |

---

## Reflection

| Function | Signature | Description |
|----------|-----------|-------------|
| `fields` | `fields(obj) -> array` | List object fields |
| `methods` | `methods(obj) -> array` | List object methods |

---

## Package Manager

| Function | Signature | Description |
|----------|-----------|-------------|
| `install` | `install(pkg)` | Install package |
| `update` | `update(pkg)` | Update package |
| `search` | `search(query) -> array` | Search packages |

---

## AI/ML

### Data Loading

| Function | Signature | Description |
|----------|-----------|-------------|
| `csv` | `csv(path) -> tensor` | Load CSV file |
| `data` | `data(values...) -> tensor` | Create data tensor |

### Data Processing

| Function | Signature | Description |
|----------|-----------|-------------|
| `clean` | `clean(tensor) -> tensor` | Clean missing values |
| `normalize` | `normalize(tensor) -> tensor` | Normalize to [0,1] |
| `standard` | `standard(tensor) -> tensor` | Standardize (z-score) |
| `shuffle` | `shuffle(tensor) -> tensor` | Shuffle rows |
| `split_data` | `split_data(tensor, ratio)` | Train/test split |

### Model Lifecycle

| Function | Signature | Description |
|----------|-----------|-------------|
| `model_create` | `model_create(name) -> model` | Create model |
| `model_save` | `model_save(model, path)` | Save model |
| `model_load` | `model_load(path) -> model` | Load model |

### Model Configuration

| Function | Signature | Description |
|----------|-----------|-------------|
| `set_loss` | `set_loss(model, loss_fn)` | Set loss function |
| `set_optimizer` | `set_optimizer(model, opt)` | Set optimizer |
| `set_lr` | `set_lr(model, lr)` | Set learning rate |
| `set_batch_size` | `set_batch_size(model, n)` | Set batch size |
| `set_epochs` | `set_epochs(model, n)` | Set epochs |
| `set_validation_split` | `set_validation_split(model, ratio)` | Set validation split |
| `set_verbose` | `set_verbose(model, bool)` | Set verbose mode |
| `set_early_stop` | `set_early_stop(model, patience)` | Set early stopping |

### Layer Creation

| Function | Signature | Description |
|----------|-----------|-------------|
| `dense` | `dense(units, activation)` | Dense layer |
| `conv` | `conv(filters, kernel_size)` | Convolutional layer |
| `lstm` | `lstm(units)` | LSTM layer |
| `gru` | `gru(units)` | GRU layer |
| `dropout` | `dropout(rate)` | Dropout layer |
| `batchnorm` | `batchnorm()` | Batch normalization |
| `attention` | `attention(heads)` | Multi-head attention |
| `transformer` | `transformer(layers, heads)` | Transformer block |
| `embedding` | `embedding(dim)` | Embedding layer |
| `layernorm` | `layernorm()` | Layer normalization |

### Model Operations

| Function | Signature | Description |
|----------|-----------|-------------|
| `add` | `add(model, layer)` | Add layer to model |
| `fit` | `fit(model, x, y)` | Train model |
| `train` | `train(model, data)` | Train model |
| `test` | `test(model, data)` | Evaluate model |
| `predict` | `predict(model, input)` | Make prediction |
| `retrain` | `retrain(model)` | Continue training |

### Example

```aura
model = model_create("my_net")
add(model, dense(128, "relu"))
add(model, dropout(0.2))
add(model, dense(10, "softmax"))
set_loss(model, "categorical_crossentropy")
set_optimizer(model, "adam")
fit(model, x_train, y_train)
result = predict(model, x_test)
```

---

## Backend Framework

### Routing

| Function | Signature | Description |
|----------|-----------|-------------|
| `route_group` | `route_group(prefix)` | Group routes |
| `next` | `next()` | Call next middleware |

### Middleware

| Function | Signature | Description |
|----------|-----------|-------------|
| `middleware` | `middleware(name)` | Declare middleware |
| `rate_limit` | `rate_limit(max, window_s)` | Rate limiting |
| `cors` | `cors(origin)` | CORS headers |
| `csrf` | `csrf()` | CSRF protection |

### Session

| Function | Signature | Description |
|----------|-----------|-------------|
| `session_get` | `session_get(key)` | Get session value |
| `session_set` | `session_set(key, val)` | Set session value |
| `session_delete` | `session_delete(key)` | Delete session value |

### Cookie

| Function | Signature | Description |
|----------|-----------|-------------|
| `cookie_get` | `cookie_get(name) -> string` | Get cookie |
| `cookie_set` | `cookie_set(name, val, opts)` | Set cookie |
| `cookie_delete` | `cookie_delete(name)` | Delete cookie |

### Proxy / Stream

| Function | Signature | Description |
|----------|-----------|-------------|
| `proxy` | `proxy(target_url)` | Reverse proxy |
| `reverse_proxy` | `reverse_proxy(target)` | Reverse proxy |
| `stream` | `stream(source)` | Stream response |
| `stream_file` | `stream_file(path)` | Stream file |
| `sse` | `sse(path)` | SSE endpoint |
| `webhook` | `webhook(url, event)` | Register webhook |

### Health / Metrics

| Function | Signature | Description |
|----------|-----------|-------------|
| `health` | `health(name, ok)` | Health check |
| `metrics` | `metrics(...)` | Metric counters |
| `trace_id` | `trace_id(id)` | Set trace ID |
| `request_id` | `request_id() -> string` | Get request ID |
| `audit` | `audit(event, data)` | Audit log |

### Sync / Pool

| Function | Signature | Description |
|----------|-----------|-------------|
| `lock` | `lock(name)` | Acquire lock |
| `unlock` | `unlock(name)` | Release lock |
| `atomic` | `atomic(ref, fn)` | Atomic operation |
| `retry` | `retry(fn, attempts)` | Retry on failure |
| `timeout` | `timeout(fn, ms)` | Timeout guard |
| `circuit_breaker` | `circuit_breaker(name, opts)` | Circuit breaker |
| `pool` | `pool(size, factory)` | Connection pool |
| `worker_pool` | `worker_pool(n, handler)` | Worker pool |
| `batch` | `batch(items, fn)` | Batch processing |
| `paginate` | `paginate(query, page, size)` | Pagination |

### Database

| Function | Signature | Description |
|----------|-----------|-------------|
| `index` | `index(table, column)` | Create index |
| `migrate` | `migrate(name)` | Run migration |
| `seed` | `seed(name, data)` | Seed data |
| `schema` | `schema(model)` | Define schema |
| `validate` | `validate(data, rules)` | Validate data |
| `sanitize` | `sanitize(input)` | Sanitize input |

### Crypto / Security

| Function | Signature | Description |
|----------|-----------|-------------|
| `sign` | `sign(payload, secret)` | Sign token |
| `verify` | `verify(token, secret)` | Verify token |
| `secret` | `secret(key)` | Get secret |
| `vault` | `vault(path)` | Vault access |

### Compress / Serialize

| Function | Signature | Description |
|----------|-----------|-------------|
| `compress` | `compress(data)` | Compress data |
| `decompress` | `decompress(data)` | Decompress data |
| `serialize` | `serialize(obj)` | Serialize object |
| `deserialize` | `deserialize(data)` | Deserialize object |

### Event / PubSub

| Function | Signature | Description |
|----------|-----------|-------------|
| `event` | `event(name)` | Declare event |
| `emit` | `emit(name, data)` | Emit event |
| `listen` | `listen(name, handler)` | Listen for event |
| `publish` | `publish(channel, msg)` | Publish message |
| `subscribe` | `subscribe(channel)` | Subscribe to channel |

### RPC / Cluster

| Function | Signature | Description |
|----------|-----------|-------------|
| `rpc` | `rpc(service)` | RPC service |
| `discover` | `discover(registry)` | Service discovery |
| `cluster` | `cluster(config)` | Cluster config |
| `node_id` | `node_id(id)` | Set node ID |
| `leader` | `leader() -> string` | Get leader |
| `shard` | `shard(key, n)` | Shard key |
| `replica` | `replica(count)` | Replica count |

### Backup

| Function | Signature | Description |
|----------|-----------|-------------|
| `backup` | `backup(path)` | Create backup |
| `restore` | `restore(path)` | Restore backup |

### Monitor

| Function | Signature | Description |
|----------|-----------|-------------|
| `monitor` | `monitor(handler)` | Monitoring handler |
| `profile_request` | `profile_request(id)` | Profile request |
| `memory_snapshot` | `memory_snapshot()` | Memory snapshot |
| `gc_collect` | `gc_collect()` | Force GC |
| `hot_reload` | `hot_reload(module)` | Hot reload module |
| `plugin` | `plugin(name, handler)` | Plugin system |
| `feature_flag` | `feature_flag(name) -> bool` | Feature flag |

### Multi-Tenant

| Function | Signature | Description |
|----------|-----------|-------------|
| `tenant` | `tenant(id)` | Tenant context |
| `tenant_context` | `tenant_context()` | Current tenant |

### Geo / Security

| Function | Signature | Description |
|----------|-----------|-------------|
| `geoip` | `geoip(ip) -> string` | GeoIP lookup |
| `captcha_verify` | `captcha_verify(token)` | Verify captcha |

### Payment

| Function | Signature | Description |
|----------|-----------|-------------|
| `payment` | `payment(amount, method)` | Process payment |
| `invoice` | `invoice(data)` | Create invoice |
| `analytics` | `analytics(event, data)` | Analytics event |

---

## Search / AI Agents

| Function | Signature | Description |
|----------|-----------|-------------|
| `search_engine` | `search_engine(config)` | Create search engine |
| `vector_search` | `vector_search(index, query)` | Vector search |
| `semantic_search` | `semantic_search(query)` | Semantic search |
| `embed_store` | `embed_store(key, vector)` | Store embedding |
| `embed_query` | `embed_query(text) -> vector` | Embed text |
| `ai_agent` | `ai_agent(config)` | Create AI agent |
| `tool` | `tool(name, fn)` | Register tool |
| `workflow` | `workflow(name, steps)` | Define workflow |
| `pipeline` | `pipeline(stages...)` | Create pipeline |
| `step` | `step(name, fn)` | Pipeline step |

---

## Language AI

| Function | Signature | Description |
|----------|-----------|-------------|
| `ai` | `ai(config)` | AI context |
| `chat` | `chat(messages)` | Chat completion |
| `embed` | `embed(text)` | Text embedding |
| `classify` | `classify(text, labels)` | Text classification |
| `translate` | `translate(text, target_lang)` | Translation |
| `summarize` | `summarize(text)` | Text summarization |
| `code` | `code(prompt)` | Code generation |

---

## Serialization (Phase 14)

| Function | Signature | Description |
|----------|-----------|-------------|
| `serial_json_encode` | `serial_json_encode(val) -> string` | Encode to JSON |
| `serial_json_decode` | `serial_json_decode(str) -> any` | Decode from JSON |
| `serial_binary_encode` | `serial_binary_encode(val) -> bytes` | Encode to binary |
| `serial_binary_decode` | `serial_binary_decode(data) -> any` | Decode from binary |
| `serial_write_json` | `serial_write_json(path, val)` | Write JSON to file |
| `serial_read_json` | `serial_read_json(path) -> any` | Read JSON from file |
| `serial_write_binary` | `serial_write_binary(path, val)` | Write binary to file |
| `serial_read_binary` | `serial_read_binary(path) -> any` | Read binary from file |
| `serial_detect_format` | `serial_detect_format(path) -> int` | Auto-detect format |

## Database (Phase 15)

| Function | Signature | Description |
|----------|-----------|-------------|
| `db_open` | `db_open(path) -> ptr` | Open SQLite connection |
| `db_close` | `db_close(db)` | Close connection |
| `db_exec` | `db_exec(db, sql)` | Execute SQL |
| `db_query` | `db_query(db, sql) -> array` | Query rows |
| `db_prepare` | `db_prepare(db, sql) -> ptr` | Prepare statement |
| `db_bind_int` | `db_bind_int(stmt, idx, val)` | Bind int |
| `db_bind_float` | `db_bind_float(stmt, idx, val)` | Bind float |
| `db_bind_text` | `db_bind_text(stmt, idx, val)` | Bind text |
| `db_bind_null` | `db_bind_null(stmt, idx)` | Bind null |
| `db_step` | `db_step(stmt) -> int` | Step statement |
| `db_column_count` | `db_column_count(stmt) -> int` | Column count |
| `db_column_type` | `db_column_type(stmt, idx) -> int` | Column type |
| `db_column_int` | `db_column_int(stmt, idx) -> int` | Column as int |
| `db_column_float` | `db_column_float(stmt, idx) -> float` | Column as float |
| `db_column_text` | `db_column_text(stmt, idx) -> string` | Column as text |
| `db_finalize` | `db_finalize(stmt)` | Finalize statement |
| `db_last_id` | `db_last_id(db) -> int` | Last insert row ID |
| `db_changes` | `db_changes(db) -> int` | Rows changed |
| `db_begin` | `db_begin(db)` | Begin transaction |
| `db_commit` | `db_commit(db)` | Commit transaction |
| `db_rollback` | `db_rollback(db)` | Rollback transaction |
| `db_escape` | `db_escape(str) -> string` | Escape SQL string |
| `db_error` | `db_error(db) -> string` | Last error message |

## Desktop Integration (Phase 18)

| Function | Signature | Description |
|----------|-----------|-------------|
| `desktop_tray_init` | `desktop_tray_init(wnd, icon, tip)` | Init system tray |
| `desktop_tray_set_icon` | `desktop_tray_set_icon(icon)` | Set tray icon |
| `desktop_tray_set_tip` | `desktop_tray_set_tip(tip)` | Set tray tooltip |
| `desktop_tray_add_menu` | `desktop_tray_add_menu(text, id)` | Add menu item |
| `desktop_tray_show_balloon` | `desktop_tray_show_balloon(title, msg, icon, timeout)` | Show balloon |
| `desktop_tray_remove` | `desktop_tray_remove()` | Remove tray |
| `desktop_notify` | `desktop_notify(title, msg)` | Show notification |
| `desktop_clipboard_get_text` | `desktop_clipboard_get_text() -> string` | Get clipboard |
| `desktop_clipboard_set_text` | `desktop_clipboard_set_text(text)` | Set clipboard |
| `desktop_dnd_enable` | `desktop_dnd_enable(wnd)` | Enable D&D |
| `desktop_assoc_set` | `desktop_assoc_set(ext, prog_id, desc, cmd)` | File association |
| `desktop_assoc_remove` | `desktop_assoc_remove(ext)` | Remove assoc |
| `desktop_startup_set` | `desktop_startup_set(name, path)` | Startup app |
| `desktop_startup_remove` | `desktop_startup_remove(name)` | Remove startup |
| `desktop_effects_acrylic` | `desktop_effects_acrylic(wnd, color)` | Acrylic effect |
| `desktop_effects_mica` | `desktop_effects_mica(wnd)` | Mica effect |
| `desktop_effects_blur` | `desktop_effects_blur(wnd)` | Blur behind |
| `desktop_effects_dark_mode` | `desktop_effects_dark_mode(wnd, enable)` | Dark title bar |
| `desktop_effects_rounded` | `desktop_effects_rounded(wnd, enable)` | Rounded corners |
| `desktop_hotkey_register` | `desktop_hotkey_register(id, mod, key) -> bool` | Register hotkey |
| `desktop_hotkey_unregister` | `desktop_hotkey_unregister(id)` | Unregister hotkey |

## Game Engine (Phase 19)

| Function | Signature | Description |
|----------|-----------|-------------|
| `light_create` | `light_create(r, g, b) -> int` | Create light |
| `light_destroy` | `light_destroy(id)` | Destroy light |
| `light_set_position` | `light_set_position(id, x, y, z)` | Set position |
| `light_set_direction` | `light_set_direction(id, x, y, z)` | Set direction |
| `light_set_color` | `light_set_color(id, r, g, b)` | Set color |
| `light_set_intensity` | `light_set_intensity(id, intensity)` | Set intensity |
| `light_set_range` | `light_set_range(id, range)` | Set range |
| `light_set_spot_angle` | `light_set_spot_angle(id, angle)` | Spot angle |
| `light_get_count` | `light_get_count() -> int` | Light count |
| `light_get` | `light_get(index) -> string` | Get light info |
| `tilemap_create` | `tilemap_create(rows, cols, layers) -> ptr` | Create tilemap |
| `tilemap_destroy` | `tilemap_destroy(tm)` | Destroy tilemap |
| `tilemap_set_tile` | `tilemap_set_tile(tm, r, c, layer, val)` | Set tile |
| `tilemap_get_tile` | `tilemap_get_tile(tm, r, c, layer) -> int` | Get tile |
| `tilemap_get_width` | `tilemap_get_width(tm) -> int` | Width |
| `tilemap_get_height` | `tilemap_get_height(tm) -> int` | Height |
| `tilemap_is_solid` | `tilemap_is_solid(tm, r, c) -> bool` | Is solid |
| `mesh_create_plane` | `mesh_create_plane() -> ptr` | Plane mesh |
| `mesh_create_sphere` | `mesh_create_sphere(lat, lon) -> ptr` | Sphere mesh |
| `mesh_create_cylinder` | `mesh_create_cylinder(seg) -> ptr` | Cylinder mesh |
| `mesh_create_capsule` | `mesh_create_capsule(seg) -> ptr` | Capsule mesh |
| `mesh_get_vertex_count` | `mesh_get_vertex_count(m) -> int` | Vertex count |
| `mesh_get_vertex_data` | `mesh_get_vertex_data(m) -> ptr` | Vertex data |
| `mesh_get_index_count` | `mesh_get_index_count(m) -> int` | Index count |
| `mesh_get_index_data` | `mesh_get_index_data(m) -> ptr` | Index data |
| `mesh_destroy` | `mesh_destroy(m)` | Destroy mesh |

## Plugin System (Phase 20)

| Function | Signature | Description |
|----------|-----------|-------------|
| `plugin_load` | `plugin_load(path) -> int` | Load plugin |
| `plugin_unload` | `plugin_unload(id)` | Unload plugin |
| `plugin_scan` | `plugin_scan(dir) -> string` | Scan directory |
| `plugin_count` | `plugin_count() -> int` | Plugin count |
| `plugin_name` | `plugin_name(id) -> string` | Get name |
| `plugin_version` | `plugin_version(id) -> string` | Get version |
| `plugin_author` | `plugin_author(id) -> string` | Get author |
| `plugin_description` | `plugin_description(id) -> string` | Get description |
| `plugin_call_init` | `plugin_call_init(id) -> int` | Call init |
| `plugin_call_shutdown` | `plugin_call_shutdown(id)` | Call shutdown |
| `plugin_abi_version` | `plugin_abi_version(id) -> int` | ABI version |

## Package Manager (Phase 21)

| Function | Signature | Description |
|----------|-----------|-------------|
| `pkg_install` | `pkg_install(name) -> int` | Install package |
| `pkg_remove` | `pkg_remove(name) -> int` | Remove package |
| `pkg_update` | `pkg_update(name) -> int` | Update package |
| `pkg_publish` | `pkg_publish(path) -> int` | Publish package |
| `pkg_search` | `pkg_search(query) -> string` | Search packages |
| `pkg_list_installed` | `pkg_list_installed() -> string` | List installed |
| `pkg_set_registry` | `pkg_set_registry(url)` | Set registry |
| `pkg_registry_url` | `pkg_registry_url() -> string` | Get registry URL |
| `pkg_login` | `pkg_login(token) -> int` | Login |
| `pkg_logout` | `pkg_logout()` | Logout |
| `pkg_lock_init` | `pkg_lock_init()` | Init lock file |
| `pkg_lock_save` | `pkg_lock_save() -> int` | Save lock |
| `pkg_lock_load` | `pkg_lock_load() -> int` | Load lock |
| `pkg_resolve` | `pkg_resolve(name) -> int` | Resolve dep |
| `pkg_resolve_count` | `pkg_resolve_count() -> int` | Dep count |
| `pkg_resolve_name` | `pkg_resolve_name(idx) -> string` | Dep name |
| `pkg_cache_list` | `pkg_cache_list() -> string` | Cache list |
| `pkg_cache_clear` | `pkg_cache_clear()` | Clear cache |

## Hot Reload (Phase 23)

| Function | Signature | Description |
|----------|-----------|-------------|
| `hr_watch` | `hr_watch(path)` | Watch path |
| `hr_unwatch` | `hr_unwatch(path)` | Stop watching |
| `hr_on_ui_change` | `hr_on_ui_change(callback)` | UI reload cb |
| `hr_on_code_change` | `hr_on_code_change(callback)` | Code reload cb |
| `hr_on_asset_change` | `hr_on_asset_change(callback)` | Asset reload cb |
| `hr_code_reload` | `hr_code_reload()` | Reload code |
| `hr_code_version` | `hr_code_version() -> int` | Code version |
| `hr_code_stale` | `hr_code_stale() -> bool` | Is stale |
| `hr_asset_reload` | `hr_asset_reload(path)` | Reload asset |
| `hr_asset_is_dirty` | `hr_asset_is_dirty(path) -> bool` | Is dirty |
| `hr_state_set` | `hr_state_set(key, val)` | Save state |
| `hr_state_get` | `hr_state_get(key) -> string` | Load state |
| `hr_console_log` | `hr_console_log(msg)` | Console log |
| `hr_console_clear` | `hr_console_clear()` | Clear console |
| `hr_console_get` | `hr_console_get(count) -> string` | Recent logs |
| `hr_console_exec` | `hr_console_exec(cmd) -> string` | Exec command |

## Testing (Phase 24)

| Function | Signature | Description |
|----------|-----------|-------------|
| `test_suite` | `test_suite(name)` | Register suite |
| `test_case` | `test_case(name)` | Register case |
| `test_run` | `test_run(results)` | Run tests |
| `assert_true` | `assert_true(cond)` | Assert true |
| `assert_false` | `assert_false(cond)` | Assert false |
| `assert_eq` | `assert_eq(a, b)` | Assert equal |
| `assert_neq` | `assert_neq(a, b)` | Assert not equal |
| `assert_gt` | `assert_gt(a, b)` | Assert greater |
| `assert_lt` | `assert_lt(a, b)` | Assert less |
| `assert_null` | `assert_null(val)` | Assert null |
| `integration_setup` | `integration_setup(cb)` | Setup callback |
| `integration_teardown` | `integration_teardown(cb)` | Teardown callback |
| `integration_test` | `integration_test(name, cb)` | Integration test |
| `widget_test` | `widget_test(name, cb)` | Widget test |
| `widget_click` | `widget_click(x, y)` | Simulate click |
| `bench_start` | `bench_start(suite)` | Start benchmark |
| `bench_end` | `bench_end(suite)` | End benchmark |
| `bench_report` | `bench_report(suite) -> string` | Report |
| `snap_shot` | `snap_shot(name, value)` | Capture snapshot |
| `snap_assert` | `snap_assert(name)` | Assert snapshot |
| `coverage_start` | `coverage_start()` | Start coverage |
| `coverage_stop` | `coverage_stop()` | Stop coverage |
| `coverage_report` | `coverage_report() -> string` | Coverage report |
| `test_pass_count` | `test_pass_count() -> int` | Pass count |
| `test_fail_count` | `test_fail_count() -> int` | Fail count |
| `test_total_count` | `test_total_count() -> int` | Total count |
| `test_results` | `test_results() -> string` | Results summary |

## Developer Tools (Phase 25)

| Function | Signature | Description |
|----------|-----------|-------------|
| `format_code` | `format_code(code) -> string` | Format code |
| `format_file` | `format_file(path) -> int` | Format file |
| `lint` | `lint(code) -> string` | Lint code |
| `lint_file` | `lint_file(path) -> string` | Lint file |
| `lsp_start` | `lsp_start(port) -> int` | Start LSP |
| `lsp_stop` | `lsp_stop() -> int` | Stop LSP |
| `complete` | `complete(code, line, col) -> string` | Completions |
| `profiler_start` | `profiler_start() -> int` | Start profiler |
| `profiler_stop` | `profiler_stop() -> int` | Stop profiler |
| `profiler_report` | `profiler_report() -> string` | Profile report |
| `inspector_tree` | `inspector_tree() -> string` | Widget tree |
| `memory_stats` | `memory_stats() -> string` | Memory stats |
| `memory_snapshot` | `memory_snapshot() -> string` | Memory snapshot |
| `memory_leak_check` | `memory_leak_check() -> int` | Leak check |
| `perf_start` | `perf_start() -> int` | Start perf monitor |
| `perf_stop` | `perf_stop() -> int` | Stop perf monitor |
| `perf_fps` | `perf_fps() -> float` | Get FPS |
| `perf_frame_time` | `perf_frame_time() -> float` | Frame time |
| `perf_report` | `perf_report() -> string` | Perf report |

---

## Security (Phase 27)

| Function | Signature | Description |
|----------|-----------|-------------|
| `sandbox_init` | `sandbox_init() -> int` | Init sandbox |
| `sandbox_allow_path` | `sandbox_allow_path(path) -> int` | Allow path in sandbox |
| `sandbox_check_path` | `sandbox_check_path(path) -> int` | Check if path is allowed |
| `sandbox_destroy` | `sandbox_destroy()` | Destroy sandbox |
| `permission_check` | `permission_check(perm) -> int` | Check permission |
| `permission_request` | `permission_request(perm) -> int` | Request permission |
| `permission_list` | `permission_list() -> string` | List permissions |
| `permission_revoke` | `permission_revoke(perm) -> int` | Revoke permission |
| `storage_open` | `storage_open(path, key, key_len) -> ptr` | Open secure store |
| `storage_set` | `storage_set(store, key, value) -> int` | Set encrypted value |
| `storage_get` | `storage_get(store, key) -> string` | Get decrypted value |
| `storage_remove` | `storage_remove(store, key) -> int` | Remove key |
| `storage_close` | `storage_close(store)` | Close store |
| `generate_key` | `generate_key(out, len) -> int` | Generate random key |
| `generate_iv` | `generate_iv(out, len) -> int` | Generate random IV |
| `encrypt` | `encrypt(key, key_len, iv, input, in_len, output, out_len) -> int` | AES-CBC encrypt |
| `decrypt` | `decrypt(key, key_len, iv, input, in_len, output, out_len) -> int` | AES-CBC decrypt |
| `pbkdf2` | `pbkdf2(password, salt, salt_len, iter, out, out_len) -> int` | PBKDF2 key derivation |
| `cert_load` | `cert_load(path) -> ptr` | Load certificate |
| `cert_info` | `cert_info(cert) -> string` | Get cert info |
| `cert_verify` | `cert_verify(cert, ca_path) -> int` | Verify cert |
| `cert_free` | `cert_free(cert)` | Free cert |
| `sha256` | `sha256(data, len) -> string` | SHA-256 hash (hex) |
| `hmac_sha256` | `hmac_sha256(key, key_len, data, data_len) -> string` | HMAC-SHA256 (hex) |
| `hash_password` | `hash_password(password) -> string` | Hash password with salt |
| `verify_password` | `verify_password(password, hash) -> int` | Verify password |
| `token_generate` | `token_generate(payload, secret) -> string` | Generate auth token |
| `token_verify` | `token_verify(token, secret) -> int` | Verify auth token |
| `basic_auth` | `basic_auth(user, pass) -> string` | Basic auth header |
| `bearer_auth` | `bearer_auth(token) -> string` | Bearer auth header |
