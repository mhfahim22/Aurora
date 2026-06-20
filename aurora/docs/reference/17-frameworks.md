# Domain-Specific Frameworks

Aurora includes built-in support for UI, Backend, Game, and AI/ML frameworks via domain-specific keywords.

## UI Framework

| Keyword       | Description                    |
|---------------|--------------------------------|
| `component`   | Declare a UI component         |
| `state`       | Component local state          |
| `properties`  | Component input properties     |
| `render`      | Define render function         |
| `style`       | Declare style rules            |
| `theme`       | Declare theme                  |
| `route`       | Register a route               |
| `page`        | Declare a page                 |
| `layout`      | Declare a layout               |
| `animate`     | Declare an animation           |
| `transition`  | Declare a transition           |

```aura
component Button
    properties
        label = "Click"
        onClick = null

    function render()
        output("<button>" + self.label + "</button>")
```

## Backend Framework

| Keyword        | Description                    |
|----------------|--------------------------------|
| `server`       | HTTP server                    |
| `request`      | HTTP request                   |
| `response`     | HTTP response                  |
| `api`          | API endpoint                   |
| `middleware`   | Middleware handler             |
| `database`     | Database connection            |
| `query`        | Database query                 |
| `model`        | Data model                     |
| `cache`        | Cache entry                    |
| `session`      | User session                   |
| `token`        | Auth token                     |
| `auth`         | Authentication                 |

```aura
api "/users" method="GET"
    response(200, users)
```

## Game Engine

| Keyword      | Description                    |
|--------------|--------------------------------|
| `scene`      | Game scene                     |
| `entity`     | Game entity                    |
| `object`     | Game object                    |
| `sprite`     | Sprite definition              |
| `camera`     | Camera                         |
| `physics`    | Physics body                   |
| `collision`  | Collision event                |
| `audio`      | Audio source                   |
| `animation`  | Animation clip                 |
| `input`      | Input handler                  |
| `update`     | Update callback                |
| `tick`       | Tick callback                  |

```aura
scene MainMenu
    function update(dt)
        if input.key_pressed("space")
            start_game()
```

## AI/ML Framework

| Keyword    | Description                    |
|------------|--------------------------------|
| `ai`       | AI context                     |
| `tensor`   | Tensor literal                 |
| `train`    | Train model                    |
| `predict`  | Make prediction                |
| `neural`   | Neural network                 |

```aura
neural network:
    dense(128, relu)
    dropout(0.5)
    dense(10, softmax)
```

## Cross-reference

| Framework   | Backend Module     | Low-level FFI      |
|-------------|---------------------|---------------------|
| UI          | `libc:ui`           | `user32.auf`        |
| Backend     | `libc:backend`      | `libc:database`     |
| Game        | `libc:game`         | `libc:sprite2d`     |
| AI/ML       | `libc:ai`           | `libtorch.auf`      |
| 3D Graphics | `libc:gl`           | `opengl.auf`        |
| Audio       | `libc:audio`        | SDL                 |
| Images      | `libc:image`        | stb_image           |

---

**Next:** [Built-in Functions](15-builtins.md)
