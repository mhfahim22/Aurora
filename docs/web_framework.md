# Aurora Web Framework

## Overview
Aurora's web framework provides a complete DSL for building HTTP servers, WebSocket endpoints, and full-stack web applications.

## Quick Start
```aura
import "server"

server app
    route "GET" "/"
        response.html("<h1>Hello World</h1>")
```

Run with: `aurorac --run app.aura`

## Routing
```aura
route "GET" "/users"           # list users
route "GET" "/users/:id"       # get user by ID
route "POST" "/users"          # create user
route "PUT" "/users/:id"       # update user
route "DELETE" "/users/:id"    # delete user
route "OPTIONS" "/path"        # preflight
```

## Request Accessors
- `request.params.X` — Path parameters
- `request.query.X` — Query string parameters
- `request.form.X` — Form body parameters
- `request.cookie.X` — Cookie values
- `request.header("Name")` — Request header

## Response Builders
- `response.json(data)` — JSON response
- `response.html(data)` — HTML response
- `response.status(code)` — HTTP status
- `response.redirect(url, code)` — Redirect
- `response.cookie(name, value, ttl)` — Set cookie
- `response.header(name, value)` — Set header

## CORS
```aura
server app
    cors                              # allow all origins
    route "GET" "/api"
        response.json("{\"ok\": true}")
```

## Sessions
```aura
server app
    route "GET" "/login"
        session = aurora_session_begin()
        response.json("{\"session\": \"created\"}")
```

## Authentication
```aura
extern function aurora_jwt_sign(payload: str, secret: str): str
extern function aurora_jwt_verify(token: str, secret: str): str
```

## Middleware
Middleware functions intercept requests before route handlers:
```aura
extern function aurora_middleware_set_context(key: str, value: str)
extern function aurora_middleware_get_context(key: str): str
```

## Templates
```aura
server app
    template "layout" "<html><body>{{content}}</body></html>"

    route "GET" "/page"
        html = render("layout", "{\"content\": \"Hello\"}")
        response.html(html)
```

## WebSocket
```aura
server app
    websocket "/chat"
        on_open    output("connected")
        on_message output("received: " + ws.message())
        on_close   output("disconnected")
```

## Server-Sent Events
```aura
server app
    sse "/events"
        output("SSE client connected")
```

## Full Example
See `examples/app/phase31_demo.aura` and `Workflow/tests/test_phase31.aura`.
