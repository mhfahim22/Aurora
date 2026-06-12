# Aurora Backend Framework

A full-featured web framework with routing, middleware, sessions, caching, authentication, database, WebSocket, and clustering.

---

## 1. Quick Start

```aura
server app
    route "/"
        response("Hello, World!")

    route "/api/hello"
        response.json({"greeting": "Hello from Aurora"})

    route "/user/:name"
        response("Hello, " + request.params.name)

app.start(8080)
```

---

## 2. Routing

### Basic Routes

```aura
route "/"
    response("Home")

route "/about"
    response("About us")
```

### Dynamic Parameters

```aura
route "/user/:id"
    response("User ID: " + request.params.id)

route "/posts/:year/:month/:slug"
    response("Post: " + request.params.slug)
```

### Route Groups

```aura
route_group "/api/v1"

    route "/users"
        response.json(get_all_users())

    route "/users/:id"
        response.json(get_user(request.params.id))

end
```

### HTTP Methods

```aura
route "/data"
    if request.method == "GET"
        response.json(fetch_data())
    elseif request.method == "POST"
        data = request.body
        save_data(data)
        response.status(201).json({"ok": true})
```

---

## 3. Middleware

```aura
# Logging middleware
middleware logger
    log("Request: " + request.method + " " + request.path)
    next()
    log("Response sent")

# Auth middleware
middleware require_auth
    token = request.headers["Authorization"]
    if not token or not verify(token)
        response.status(401).json({"error": "Unauthorized"})
        return
    request.user = decode_token(token)
    next()

# Apply middleware
server app
    middleware logger
    middleware require_auth

    route "/protected"
        response("Hello, " + request.user.name)
```

### Built-in Middleware

```aura
rate_limit(100, 60)          # 100 requests per 60 seconds
cors("*")                    # CORS headers
csrf()                       # CSRF protection
```

---

## 4. Request & Response

### Request Object

| Field | Description |
|-------|-------------|
| `request.method` | HTTP method (GET, POST, etc.) |
| `request.path` | Request path |
| `request.params` | Route parameters |
| `request.query` | Query string parameters |
| `request.headers` | Request headers |
| `request.body` | Request body (string) |
| `request.ip` | Client IP address |
| `request.user` | Authenticated user (after auth middleware) |

### Response Functions

```aura
response("text")                           # plain text
response.json({"key": "value"})            # JSON
response.status(404)                       # custom status
response.status(201).json({"created": 1})  # chained
response.file("./download.zip")            # file download
response.redirect("/login")                # redirect
```

### Streaming

```aura
stream("data.txt")                 # stream a file
stream_file("/path/to/large.mp4")  # stream with headers
sse("/events")                     # Server-Sent Events
    while true
        send("message", "hello")
        sleep(1)
```

---

## 5. Sessions & Cookies

```aura
# Session
session_set("user_id", 42)
user = session_get("user_id")
session_delete("user_id")

# Cookies
cookie_set("theme", "dark", max_age=3600)
theme = cookie_get("theme")
cookie_delete("theme")
```

---

## 6. Authentication

```aura
# Token generation
token = sign(user_payload, secret_key)
valid = verify(token, secret_key)

# Hash passwords
hash = auth_hash_password("mypassword")
ok = auth_verify_password("mypassword", hash)
```

---

## 7. Caching

```aura
cache_set("user_42", user_data, ttl=300)   # 5 min TTL
data = cache_get("user_42")
if cache_has("user_42")
    output("cache hit")
cache_delete("user_42")
cache_clear()
```

---

## 8. Database

```aura
# SQL queries
query("CREATE TABLE users (id INT, name TEXT, email TEXT)")
query("INSERT INTO users VALUES (1, 'Alice', 'alice@example.com')")
results = query("SELECT * FROM users WHERE id = 1")
```

### Migrations

```aura
migrate("001_create_users")
    execute("CREATE TABLE users (id INT, name TEXT, email TEXT)")
    execute("CREATE INDEX idx_users_email ON users(email)")

migrate("002_add_age")
    execute("ALTER TABLE users ADD COLUMN age INT DEFAULT 0")
```

### Models & Schemas

```aura
model User
    schema
        id int primary_key
        name string
        email string unique
        age int default 0
    validate
        if len(self.name) == 0
            error("Name is required")
        if not self.email contains "@"
            error("Invalid email")
```

---

## 9. WebSockets & Real-Time

```aura
# WebSocket handler
route "/ws"
    ws = upgrade(request)
    ws.on("message")
        broadcast(ws.message)
    ws.on("close")
        log("client disconnected")

# Event bus
event("user.login")
    log("User logged in: " + data.user)
    send_email(data.user.email, "Welcome back!")

emit("user.login", {"user": user_data})
publish("notifications", "New update available")
```

---

## 10. Error Handling

```aura
try
    result = query("SELECT * FROM users")
    response.json(result)
catch error
    response.status(500).json({"error": error})
finally
    log("request complete")
```

---

## 11. Clustering & Scaling

```aura
# Cluster configuration
cluster
    node_id("web-1")
    discover("consul://localhost:8500")
    shard("users", 3)
    replica(2)

# RPC between nodes
rpc("user-service")
    user = call("get_user", 42)
    output(user.name)
```

---

## 12. Monitoring & Observability

```aura
# Request tracing
trace_id("req-" + stamp())
request_id = request_id()

# Metrics
metrics
    counter("requests_total", 1)
    gauge("active_connections", connections)
    histogram("response_time_ms", elapsed)

# Health checks
health "/health"
    check("database", db.ping())
    check("cache", cache.ping())

# Audit logging
audit("user.login", {"user_id": user.id, "ip": request.ip})
```

---

## 13. Rate Limiting & Throttling

```aura
# Rate limiting
rate_limit(100, 60)        # 100 requests per 60s window

# Throttle/debounce
throttle("email_notification", 5000)    # max once per 5s
debounce("search", 300)                  # wait 300ms after last call
```

---

## 14. Security

```aura
# CORS
cors("https://myapp.com")
cors("*")                   # allow all origins

# Input sanitization
clean_input = sanitize(user_input)

# GeoIP blocking
country = geoip(request.ip)
if country == "XX"
    response.status(403).json({"error": "blocked"})
```

---

## 15. Full Example

```aura
server my_api
    # Middleware
    middleware logger
        log(request.method + " " + request.path)
        next()

    middleware require_auth
        if not request.headers["Authorization"]
            response.status(401).json({"error": "no auth"})
            return
        next()

    # Routes
    route "/"
        response.json({"status": "ok", "version": "1.0"})

    route "/users"
        if request.method == "GET"
            response.json(query("SELECT * FROM users"))
        elseif request.method == "POST"
            data = request.body
            query("INSERT INTO users VALUES (...)")

    route "/users/:id"
        user = query("SELECT * FROM users WHERE id = " + request.params.id)
        if user == null
            response.status(404).json({"error": "not found"})
        else
            response.json(user)

    # Rate limit login endpoint
    route_group "/auth"
        rate_limit(5, 60)   # 5 attempts per minute

        route "/login"
            # validate credentials, create session, return token

        route "/logout"
            session_delete(request.user.session)
            response.json({"ok": true})
        end

    # Health check
    route "/health"
        health("db", db.ping())
        health("cache", cache.ping())

    # WebSocket
    route "/ws"
        ws = upgrade(request)
        ws.on("message")
            broadcast(ws.message)

my_api.start(8080)
```
