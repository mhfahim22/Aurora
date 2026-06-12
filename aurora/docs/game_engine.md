# Aurora Game Engine

A 2D game engine with entity-component system, physics, collision detection, audio, animation, sprite rendering, and input handling.

---

## 1. Quick Start

```aura
scene MainScene
    entity player
        sprite = "@"
        x = 400
        y = 300
        speed = 5.0

        function update(dt)
            if input.key_down("left")
                self.x -= self.speed * dt
            if input.key_down("right")
                self.x += self.speed * dt
            if input.key_down("up")
                self.y -= self.speed * dt
            if input.key_down("down")
                self.y += self.speed * dt

MainScene.init()
while true
    MainScene.update(1.0 / 60.0)
    MainScene.render()
    sleep(16)   # ~60fps
```

---

## 2. Scenes

A scene is a container for entities and game logic.

```aura
scene GameLevel
    function init()
        self.create_entity("player", Player)
        self.create_entity("enemy1", Enemy)
        self.create_entity("enemy2", Enemy)

    function update(dt)
        if self.all_enemies_defeated()
            scene.load("next_level")

    function render()
        # optional custom rendering
```

### Scene Lifecycle

| Method | Called When |
|--------|-------------|
| `init()` | Scene loaded |
| `update(dt)` | Each frame (dt = delta time in ms) |
| `render()` | After update, before display |
| `destroy()` | Scene unloaded |

---

## 3. Entities

Entities are game objects with position, velocity, sprite, and custom update logic.

```aura
entity Player
    sprite = "@"
    x = 0
    y = 0
    vx = 0
    vy = 0
    speed = 5.0
    health = 100
    mass = 1.0

    function update(dt)
        # Input handling
        if input.key_down("left")
            self.vx = -self.speed
        elif input.key_down("right")
            self.vx = self.speed
        else
            self.vx = 0

        # Position update
        self.x += self.vx * dt
        self.y += self.vy * dt

    function on_collide(other)
        if other.type == "enemy"
            self.health -= 10
            if self.health <= 0
                scene.remove(self)
```

### Entity Properties

| Property | Type | Description |
|----------|------|-------------|
| `x` | float | X position |
| `y` | float | Y position |
| `z` | float | Z position (depth) |
| `vx` | float | X velocity |
| `vy` | float | Y velocity |
| `vz` | float | Z velocity |
| `sprite` | string | Character/visual representation |
| `mass` | float | Physics mass |
| `health` | float | Hit points |
| `type` | string | Entity type tag |

---

## 4. Physics

```aura
entity Ball
    mass = 2.0
    vx = 100.0
    vy = 0.0

    function init()
        physics_set_gravity(0.0, 980.0)  # gravity in px/s^2

    function update(dt)
        physics_step(self, dt)            # apply physics

    function on_collide(other)
        if other.type == "wall"
            self.vx = -self.vx            # bounce
```

### Physics Functions

| Function | Description |
|----------|-------------|
| `physics_step(entity, dt)` | Apply gravity, velocity, position update |
| `physics_set_gravity(x, y)` | Set global gravity vector |
| `collision_aabb(e1, e2)` | AABB collision check (returns true/false) |
| `collision_distance(e1, e2)` | Distance between entities |

Physics step applies:
1. Gravity to vertical velocity
2. Velocity damping (air resistance)
3. Euler position integration
4. Ground-plane bounce

---

## 5. Sprites & Rendering

```aura
entity Wall
    sprite = "#"
    x = 100
    y = 200

entity Enemy
    sprite = "*"
    x = 300
    y = 150

entity Projectile
    sprite = "o"
    x = 0
    y = 0
```

Built-in sprite characters and their meanings:

| Character | Type | Entity |
|-----------|------|--------|
| `@` | Player | Player characters |
| `#` | Wall | Environment obstacles |
| `*` | Enemy | Hostile entities |
| `o` | Projectile | Bullets, arrows |
| `~` | Water | Liquid surfaces |
| `&` | Item | Collectible items |

---

## 6. Animation

```aura
entity AnimatedSprite
    function init()
        anim = animation_create("walk", 0.0, 1.0, 500)
        animation_play(anim)

    function update(dt)
        # animation plays automatically at ~60fps
        # frame callback interpolates t from 0.0 to 1.0
```

### Animation Functions

| Function | Description |
|----------|-------------|
| `animation_create(name, from, to, duration_ms)` | Create animation |
| `animation_play(anim)` | Start playing |
| `animation_stop(anim)` | Stop animation |
| `animation_set(anim, key, value)` | Set animation property |

---

## 7. Audio

```aura
function play_sound()
    audio_play("explosion.wav")
    audio_play_tone(440, 200)   # A4 note for 200ms
```

### Audio Functions

| Function | Description |
|----------|-------------|
| `audio_play(filename)` | Play audio file |
| `audio_play_tone(freq, duration_ms)` | Play tone (beep) |

- **Windows**: Win32 `Beep()` API
- **Linux**: Terminal bell (`\a`)
- **macOS**: Terminal bell (`\a`)

---

## 8. Input Handling

```aura
function handle_input()
    poll_input()

    if input_key_down("left")
        player.x -= 5

    if input_key_down("space")
        shoot()

    if input_key_pressed("enter")
        pause_game()
```

### Input Functions

| Function | Description |
|----------|-------------|
| `poll_input()` | Poll all keyboard state |
| `input.key_down(key)` | Is key currently held? |
| `input.key_pressed(key)` | Was key just pressed this frame? |
| `input.key_released(key)` | Was key just released? |

### Key Names

`left`, `right`, `up`, `down`, `space`, `enter`, `escape`, `shift`, `ctrl`, `alt`, `a`-`z`, `0`-`9`, `f1`-`f12`

---

## 9. Camera

```aura
entity Player
    function update(dt)
        # Camera follows player
        camera.x = self.x - 400  # center on screen
        camera.y = self.y - 300
```

### Camera Properties

| Property | Description |
|----------|-------------|
| `camera.x` | Camera X offset |
| `camera.y` | Camera Y offset |
| `camera.zoom` | Zoom level (default: 1.0) |

---

## 10. Game Loop

The standard game loop structure:

```aura
scene MyGame
    function init()
        # Create entities
        entity player

    function update(dt)
        # Poll input
        poll_input()

        # Update entities
        for entity in self.entities
            entity.update(dt)

        # Physics
        physics_set_gravity(0, 980)
        for entity in self.entities
            physics_step(entity, dt)

        # Collision detection
        for a in self.entities
            for b in self.entities
                if a != b and collision_aabb(a, b)
                    a.on_collide(b)
                    b.on_collide(a)

        # Check win/lose conditions
        if player.health <= 0
            scene.load("game_over")

    function render()
        # Print all entities at their positions
        # Camera offset is applied automatically
```

### Tick Callback

```aura
tick
    # Called every fixed timestep (independent of frame rate)
    update_physics()
```

---

## 11. Full Example: Space Shooter

```aura
scene SpaceShooter
    function init()
        entity player
            sprite = "@"
            x = 400
            y = 500
            speed = 5.0
            health = 100

        for i in 5
            entity enemy
                sprite = "*"
                x = 50 + i * 150
                y = 50
                speed = 1.0

    function update(dt)
        poll_input()

        # Player movement
        if input.key_down("left")  and player.x > 0
            player.x -= player.speed * dt
        if input.key_down("right") and player.x < 800
            player.x += player.speed * dt
        if input.key_down("space")
            entity bullet
                sprite = "o"
                x = player.x
                y = player.y - 10
                vy = -10.0

        # Move enemies
        for entity in self.entities
            if entity.type == "enemy"
                entity.y += entity.speed * dt
                if entity.y > 600
                    scene.load("game_over")

        # Collision detection
        for a in self.entities
            for b in self.entities
                if a.type == "bullet" and b.type == "enemy"
                    if collision_aabb(a, b)
                        scene.remove(a)
                        scene.remove(b)
                        audio_play_tone(880, 50)

    function render()
        for entity in self.entities
            output(entity.sprite + " at " + entity.x + "," + entity.y)

# Run
space.init()
while true
    space.update(16)
    space.render()
    sleep(16)
```
