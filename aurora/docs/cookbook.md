# Aurora Cookbook

Practical recipe-style solutions for common tasks.

## Strings

### Join a list of strings
```aura
items = ["apple", "banana", "cherry"]
result = join(items, ", ")
output(result)  # "apple, banana, cherry"
```

### Check if string contains a substring
```aura
text = "Hello, World!"
if has(text, "World")
    output("Found it!")
```

### Extract between delimiters
```aura
data = "<title>Hello</title>"
start = index(data, ">") + 1
end_ = index(data, "<")
title = substr(data, start, end_ - start)
output(title)  # "Hello"
```

## Math

### Clamp a value to a range
```aura
val = clamp(150, 0, 100)
output(val)  # 100
```

### Random dice roll (1–6)
```aura
roll = rand(1, 6)
output("You rolled: " + str(roll))
```

### Check if number is even
```aura
n = 42
if n % 2 == 0
    output("even")
```

## File I/O

### Read a file
```aura
contents = read("data.txt")
output(contents)
```

### Write to a file
```aura
write("output.txt", "Hello, file!")
```

### Check if file exists before reading
```aura
path = "config.json"
if exists(path)
    data = read(path)
else
    output("File not found")
```

## JSON

### Parse JSON and access fields
```aura
data = decode('{"user": "Alice", "age": 30}')
output(data.user)
output(data.age)
```

### Encode to JSON
```aura
obj = {"name": "Bob", "scores": [90, 85, 92]}
json_str = encode(obj)
output(json_str)
```

## Database (SQLite)

### Open, query, close
```aura
db = db_open("test.db")
db_exec(db, "CREATE TABLE IF NOT EXISTS users (id INT, name TEXT)")
db_exec(db, "INSERT INTO users VALUES (1, 'Alice')")
rows = db_query(db, "SELECT * FROM users")
db_close(db)
```

### Use prepared statements
```aura
db = db_open("test.db")
stmt = db_prepare(db, "INSERT INTO users VALUES (?, ?)")
db_bind_int(stmt, 1, 42)
db_bind_text(stmt, 2, "Bob")
db_step(stmt)
db_finalize(stmt)
db_close(db)
```

### Transaction
```aura
db = db_open("test.db")
db_begin(db)
db_exec(db, "UPDATE accounts SET balance = balance - 100 WHERE id = 1")
db_exec(db, "UPDATE accounts SET balance = balance + 100 WHERE id = 2")
db_commit(db)
db_close(db)
```

## Serialization

### Write and read JSON file
```aura
data = {"name": "Widget", "count": 5}
serial_write_json("data.json", data)
loaded = serial_read_json("data.json")
output(loaded.name)
```

### Binary serialization (compact)
```aura
data = {"x": 1.0, "y": 2.0, "z": 3.0}
serial_write_binary("vec.bin", data)
restored = serial_read_binary("vec.bin")
```

## Desktop (Windows)

### System tray notification
```aura
desktop_tray_init(wnd, "app.ico", "My App")
desktop_tray_show_balloon("Hello", "Task complete!", 1, 3000)
```

### Clipboard
```aura
desktop_clipboard_set_text("Copied text!")
text = desktop_clipboard_get_text()
output(text)
```

### Global hotkey (Ctrl+Shift+F1)
```aura
id = desktop_hotkey_register(1, 0x0008 | 0x0004, 0x70)  # MOD_CTRL | MOD_SHIFT, VK_F1
```

## Game Engine

### Create a directional light
```aura
light = light_create(1.0, 1.0, 1.0)
light_set_direction(light, 0.0, -1.0, 0.0)
light_set_intensity(light, 0.8)
```

### Create and query a tilemap
```aura
tm = tilemap_create(10, 10, 2)
tilemap_set_tile(tm, 3, 4, 0, 1)
val = tilemap_get_tile(tm, 3, 4, 0)
solid = tilemap_is_solid(tm, 3, 4)
```

### Create mesh primitives
```aura
plane = mesh_create_plane()
sphere = mesh_create_sphere(16, 16)
verts = mesh_get_vertex_count(plane)
indices = mesh_get_index_count(plane)
```

## Plugins

### Load and query a plugin
```aura
id = plugin_load("my_plugin.dll")
if id >= 0
    output("Loaded: " + plugin_name(id))
    output("Version: " + plugin_version(id))
    plugin_call_init(id)
```

## Package Manager

### Search and install
```aura
results = pkg_search("http")
output(results)
pkg_install("aurora-http")
```

## Hot Reload

### Watch files for changes
```aura
hr_watch("src/")
hr_on_code_change(lambda()
    output("Code changed, reloading...")
    hr_code_reload()
)
```

## Testing

### Basic unit test
```aura
test_suite("Math")
test_case("addition")
    assert_eq(2 + 2, 4)
    assert_gt(10, 5)
    assert_lt(-1, 0)
```

### Snapshot test
```aura
snap_shot("greeting", "Hello, World!")
snap_assert("greeting")
```

### Benchmark
```aura
bench_start("sorting")
    result = sort([3, 1, 4, 1, 5])
bench_end("sorting")
output(bench_report("sorting"))
```

## Security

### Hash a password
```aura
hashed = hash_password("my_secret_password")
ok = verify_password("my_secret_password", hashed)
output(ok)  # 1 (true)
```

### Generate and verify an auth token
```aura
token = token_generate("user_id=42", "my_secret")
ok = token_verify(token, "my_secret")
output(ok)  # 1 (valid)
```

### Basic and Bearer auth headers
```aura
basic = basic_auth("admin", "password123")
output(basic)  # "Basic YWRtaW46cGFzc3dvcmQxMjM="

bearer = bearer_auth("my_jwt_token")
output(bearer)  # "Bearer my_jwt_token"
```

### SHA-256 hash
```aura
hash = sha256("hello", 5)
output(hash)  # hex-encoded SHA-256
```

### Encrypt and decrypt data
```aura
key = malloc(32)
generate_key(key, 32)
iv = malloc(16)
generate_iv(iv, 16)

plaintext = "Hello, World!"
in_len = 13
out_len_ptr = malloc(4)
encrypt(key, 32, iv, plaintext, in_len, ciphertext, out_len_ptr)

decrypted = malloc(32)
dec_len_ptr = malloc(4)
decrypt(key, 32, iv, ciphertext, deref(out_len_ptr), decrypted, dec_len_ptr)
```

## Developer Tools

### Format code
```aura
formatted = format_code("fn add(a,b) { return a+b }")
output(formatted)  # "fn add(a, b) { return a + b }"
```

### Profile performance
```aura
profiler_start()
# ... intensive work ...
profiler_stop()
report = profiler_report()
output(report)
