# Aurora Backend Framework

A lightweight web framework with HTTP routing, request/response handling, and JIT execution.

---

## 1. Quick Start

```aura
server app
    route "/"
        response("Hello, World!")

    route "/todos"
        response("[{\"id\": 1, \"title\": \"Learn Aurora\", \"done\": false}]")
```

Run with JIT:
```
aurorac myapp.aura --run
```

The server listens on `http://localhost:8080`.

---

## 2. Routing

### Basic Routes

```aura
route "/"
    response("Home")

route "/about"
    response("About us")
```

By default, routes match the `GET` method.

### HTTP Methods

Specify a method with a string prefix:

```aura
route "GET" "/"
    response("Home")

route "POST" "/todos"
    response("{\"id\": 1, \"title\": \"New\", \"done\": false}")
```

Routes registered with different methods only dispatch when the request method matches.

### Dynamic Parameters (routing only)

Path patterns with `:param` segments are matched by the router, but route parameters are **not yet accessible** from Aurora code (`request.params.name` is not implemented).

```aura
route "/user/:id"
    response("User page")
```

The route dispatches correctly for `/user/42`, but the captured `:id` value cannot yet be read inside the handler.

---

## 3. Response

```aura
response("plain text body")
response("{\"json\": \"encoded as string\"}")
```

The response keyword sets the body of the HTTP response. The body must be a string expression. Content-Type defaults to `application/json`.

---

## 4. Server

### Server Block

```aura
server my_server_name
    route "/path1"
        response("body1")
    route "POST" "/path2"
        response("body2")
```

The server initializes on port 8080 and enters the accept loop. Each `route` block registers a handler function for the given method and path.

### JIT Mode

```aura
aurorac myapp.aura --run    # compiles + runs in JIT
aurorac myapp.aura          # compiles to LLVM IR only
```

---

## 5. Limitations (not yet implemented)

- `request.params.name` / `request("param")` — path parameter access
- `request.body` / `request.method` / `request.headers` — request object fields
- `response.json(...)` / `response.status(...)` — chained response functions
- `middleware` blocks
- `api` blocks
- `database`, `cache`, `session`, `auth` keywords
- `websocket` / `sse` support
- File serving / static routes
- CORS, rate limiting, CSRF

These features are planned but not yet available in the current Aurora compiler.

---

## 6. Full Example (Todo API)

See `examples/todo_server.aura` for a working CRUD example:

```aura
outputln("Todo Server starting...")

server app
    route "GET" "/"
        response("Todo API -- use /todos")

    route "GET" "/todos"
        response("[{\"id\":1,\"title\":\"Learn Aurora\",\"done\":false},{\"id\":2,\"title\":\"Build CRUD\",\"done\":true}]")

    route "POST" "/todos"
        response("{\"id\":3,\"title\":\"New todo\",\"done\":false}")

    route "GET" "/todos/1"
        response("{\"id\":1,\"title\":\"Learn Aurora\",\"done\":false}")

    route "GET" "/todos/2"
        response("{\"id\":2,\"title\":\"Build CRUD\",\"done\":true}")
```

Test with curl:

```bash
curl http://localhost:8080/
curl http://localhost:8080/todos
curl -X POST http://localhost:8080/todos
curl http://localhost:8080/todos/1
```
