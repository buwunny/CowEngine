# Cow Engine - Help

## Getting Started

Cow Engine is a C++ game engine editor. Use the **Scene** tab to build your scene, the **Code** tab to write scripts, and this **Help** tab as a reference.

To attach a script to an object, select it in the Scene Hierarchy, enter a `.cow` file path in the Inspector's Script field, then click **Edit** to open it in the Code tab. Hit **Apply (recompile)** to compile and run it.

---

# CowScript Reference

CowScript is Cow Engine's built-in scripting language. Scripts are plain text files with the `.cow` extension. Each object in the scene can have one script attached to it.

---

## Language Basics

### Comments

```
// This is a comment
```

Lines beginning with `//` are ignored by the interpreter.

### Variables

```
let x = 10
let name = "player"
let active = true
let nothing = null
```

Declare variables with `let`. Variables are dynamically typed — they can hold numbers, strings, booleans, or null.

### Data Types

| Type   | Examples                  |
|--------|---------------------------|
| Number | `0`, `3.14`, `-1`, `1e3`  |
| Bool   | `true`, `false`           |
| Str    | `"hello"`, `""`           |
| Null   | `null`                    |
| Handle | returned by engine calls  |

### Operators

**Arithmetic:** `+`, `-`, `*`, `/`, `%`

**Comparison:** `==`, `!=`, `<`, `<=`, `>`, `>=`

**Logical:** `and`, `or`, `not`

**Assignment:** `=`

**Property access:** `.` (dot notation on handles)

---

## Control Flow

### If / Else

```
if (x > 0) {
    print("positive")
} else {
    print("non-positive")
}
```

The `else` branch is optional.

### While Loop

```
let i = 0
while (i < 10) {
    print(i)
    i = i + 1
}
```

Loops have a built-in safety limit of 1,000,000 iterations to prevent infinite loops from freezing the engine.

---

## Functions

Define reusable functions with `fn`:

```
fn add(a, b) {
    return a + b
}

let result = add(3, 4)
print(result)  // prints 7
```

Functions can return values with `return`. Calling a function without enough arguments is safe — missing parameters default to `null`.

---

## Events

Events are special functions the engine calls automatically. Define them with `on`:

```
on start() {
    // runs once when testing mode begins
}

on update(dt) {
    // runs every frame while testing
    // dt = delta time in seconds since last frame
}
```

Only `start` and `update` are currently supported. `update` receives `dt` (delta time) as its first argument — use it to make movement frame-rate independent.

---

## Built-in Functions

### Utility

| Function | Description |
|----------|-------------|
| `print(...)` | Logs values to the Debug Console. Accepts any number of arguments. |
| `time()` | Returns elapsed time in seconds since testing began. |
| `dt()` | Returns last frame's delta time in seconds. |
| `key(name)` | Returns `true` if the named key is currently held. |

**Key names for `key()`:**

Letters `a`–`z`, and: `space`, `enter`, `shift`, `ctrl`, `alt`, `up`, `down`, `left`, `right`, `escape`

```
if (key("space")) {
    self_apply_impulse(0, 5, 0)
}
```

### Math

| Function | Description |
|----------|-------------|
| `sin(x)` | Sine of x (radians) |
| `cos(x)` | Cosine of x (radians) |
| `tan(x)` | Tangent of x (radians) |
| `sqrt(x)` | Square root |
| `abs(x)` | Absolute value |
| `floor(x)` | Round down to nearest integer |
| `ceil(x)` | Round up to nearest integer |
| `random()` | Random float in [0, 1) |

---

## Self — The Attached Object

These functions read and write the transform of the object the script is attached to.

### Reading Position / Rotation / Scale

| Function | Returns |
|----------|---------|
| `self_x()` | World X position |
| `self_y()` | World Y position |
| `self_z()` | World Z position |
| `self_rx()` | X rotation (degrees) |
| `self_ry()` | Y rotation (degrees) |
| `self_rz()` | Z rotation (degrees) |
| `self_sx()` | X scale |
| `self_sy()` | Y scale |
| `self_sz()` | Z scale |

### Writing Position / Rotation / Scale / Color

| Function | Description |
|----------|-------------|
| `self_set_pos(x, y, z)` | Set world position |
| `self_set_rot(rx, ry, rz)` | Set rotation in degrees |
| `self_set_scale(sx, sy, sz)` | Set scale |
| `self_set_color(r, g, b, a)` | Set RGBA color, each component 0–1 |

### Physics

These require the object to have a Rigidbody (mass > 0).

| Function | Description |
|----------|-------------|
| `self_apply_impulse(x, y, z)` | Apply an instantaneous impulse |
| `self_apply_force(x, y, z)` | Apply a continuous force (per frame) |
| `self_set_velocity(x, y, z)` | Set linear velocity directly |

```
on update(dt) {
    if (key("space")) {
        self_apply_impulse(0, 5, 0)
    }
}
```

---

## Handles and Components

Engine objects are accessed through **handles** — opaque references returned by built-in functions. Use dot notation to read and write properties on handles.

### Getting Handles

| Function | Returns |
|----------|---------|
| `self()` | Handle to the attached object |
| `transform()` | Transform handle for attached object |
| `transform_of(obj)` | Transform handle for any object handle |
| `rigidbody()` | Rigidbody handle for attached object |
| `rigidbody_of(obj)` | Rigidbody handle for any object handle |
| `camera()` | Handle to the active player camera |

### Spawning Objects

Spawn functions return an **object handle** and accept an optional `(x, y, z)` position (defaults to `(0, 5, 0)`).

| Function | Description |
|----------|-------------|
| `spawn_cube(x, y, z)` | Spawns a cube, returns object handle |
| `spawn_cow(x, y, z)` | Spawns a cow mesh, returns object handle |
| `spawn_plane(x, y, z)` | Spawns a plane, returns object handle |

Spawned objects get a random color and are added to the live scene.

---

## Handle Properties

Once you have a handle, read and write properties with `.`:

```
let t = transform()
t.x = 0        // write
let y = t.y    // read
```

### Object Handle (`spawn_*`, `self()`)

| Property | Type | Description |
|----------|------|-------------|
| `.x` `.y` `.z` | number (r/w) | World position shortcut |
| `.rx` `.ry` `.rz` | number (r/w) | Rotation shortcut |
| `.sx` `.sy` `.sz` | number (r/w) | Scale shortcut |
| `.transform` | handle (r) | Gets the transform component handle |
| `.rigidbody` | handle (r) | Gets the rigidbody component handle |
| `.name` | string (r) | Object's name |

### Transform Handle (`transform()`, `transform_of()`)

| Property | Type | Description |
|----------|------|-------------|
| `.x` `.y` `.z` | number (r/w) | World position |
| `.rx` `.ry` `.rz` | number (r/w) | Rotation in degrees |
| `.sx` `.sy` `.sz` | number (r/w) | Scale |

### Rigidbody Handle (`rigidbody()`, `rigidbody_of()`)

| Property | Type | Description |
|----------|------|-------------|
| `.vx` `.vy` `.vz` | number (r/w) | Linear velocity |
| `.mass` | number (r) | Object mass |

### Camera Handle (`camera()`)

| Property | Type | Description |
|----------|------|-------------|
| `.x` `.y` `.z` | number (r/w) | Camera world position |
| `.fx` `.fy` `.fz` | number (r) | Front (forward) direction vector |
| `.ux` `.uy` `.uz` | number (r) | Up direction vector |
| `.yaw` | number (r) | Horizontal rotation in degrees |
| `.pitch` | number (r) | Vertical rotation in degrees |

---

## Examples

### Spin and Bob

Rotates the object on Y and bobs it up and down using `sin`:

```
let base_y = 0
let speed = 60

on start() {
    base_y = self_y()
}

on update(dt) {
    let yaw = self_ry() + speed * dt
    self_set_rot(self_rx(), yaw, self_rz())
    let y = base_y + sin(time() * 2) * 1.5
    self_set_pos(self_x(), y, self_z())
}
```

### Jump on Space

Applies an upward impulse and changes color when Space is pressed:

```
let was_down = false

on update(dt) {
    let down = key("space")
    if (down and not was_down) {
        self_apply_impulse(0, 5, 0)
        self_set_color(random(), random(), random(), 1)
    }
    was_down = down
}
```

### Shoot Cows from the Camera

Spawns a cow in the camera's forward direction and launches it:

```
let was_down = false
let speed = 100

on update(dt) {
    let down = key("c")
    if (down and not was_down) {
        let cam = camera()
        let ox = cam.x + cam.fx * 2
        let oy = cam.y + cam.fy * 2
        let oz = cam.z + cam.fz * 2
        let cow = spawn_cow(ox, oy, oz)
        cow.sx = 0.1
        cow.sy = 0.1
        cow.sz = 0.1
        let rb = cow.rigidbody
        rb.vx = cam.fx * speed
        rb.vy = cam.fy * speed
        rb.vz = cam.fz * speed
    }
    was_down = down
}
```

### Orbit Around Origin

Makes the object orbit the origin at a fixed radius using trigonometry:

```
let radius = 5
let angle = 0
let orbit_speed = 90

on update(dt) {
    angle = angle + orbit_speed * dt
    let rad = angle * 3.14159 / 180
    self_set_pos(cos(rad) * radius, self_y(), sin(rad) * radius)
}
```

---

## Tips

- Use `dt` (delta time) from `on update(dt)` for all movement to stay frame-rate independent.
- The `was_down` pattern (store previous key state, act only on the rising edge) prevents holding a key from firing every frame.
- `spawn_*` functions return handles — store them in a variable if you need to access the spawned object later.
- `self_apply_impulse` requires the object to have a rigidbody (set Mass > 0 in the Inspector).
- Use `print(...)` in the Debug Console to inspect values while testing.
