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
