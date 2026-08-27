
---

# Lua ARchive (lar)

**Lua ARchive (lar)** is a runtime and packaging format for Lua projects. It executes applications stored inside `.lar` archives (ZIP-based containers) using a custom loader, module system, and virtual filesystem.

A `.lar` file contains Lua bytecode, assets, and a manifest that defines how the application is loaded and executed.

---

# Features

* ZIP-based Lua application packaging (`.lar`)
* Custom module loader integrated into Lua’s `package.searchers`
* Dependency system using nested `.lar` archives
* Virtual filesystem for assets inside archives
* Lua bytecode compilation for modules (`.luac`)
* Streaming asset access (read/seek API)
* Optional sandboxing via `disableExternalLua`

---

# Running

```bash
lar game.lar
```

Executes a `.lar` archive using the runtime loader.

---

# Building

```bash
lar --build gameproject -o game.lar
```

Builds a `.lar` archive from a directory.

---

# Compatibility

* Lua 5.3+
* C++17+

---

# Dependencies

* [libzip](https://libzip.org/download)
* [zlib](https://zlib.net/)
* [luapp](https://github.com/anatinesquire40/Lua-CPP-API)

---

# Archive Structure

A `.lar` archive typically contains:

```
manifest.luac
assets/ (optional)
com/example/game/main.luac
```

* `manifest.luac` defines runtime configuration
* `assets/` contains binary resources
* module files follow Lua package path conventions

---

# Manifest

The manifest is a Lua table compiled to bytecode (`manifest.luac`) that defines how the archive is executed.

## Fields

* `name` *(string, required)*
  Project name.

* `version` *(string, required)*
  Project version.

* `main` *(string, required for root archives)*
  Entry module of the application.
  Example: `"com.example.game.main"` → `com/example/game/main.luac`

* `dependencies` *(table, optional)*
  List of other `.lar` archives required at runtime.

* `disableExternalLua` *(boolean, optional, default: false)*
  If `true`, disables loading modules from outside the archive system.

---

# Manifest Example

```lua
return {
    name = "game",
    version = "1.0",
    main = "com.example.game.main",

    dependencies = {
        "vulkan.lar"
    },

    disableExternalLua = true,
}
```

---

# Module System

The runtime overrides Lua’s module resolution system:

* Replaces `package.searchers`
* Adds support for `.luac` modules inside `.lar` archives
* Uses custom `package.zpath`:

  ```
  ?.luac;?/init.luac
  ```

Modules are resolved inside:

* the main archive
* dependency archives
* optional external filesystem (if enabled)

---

# Assets System

The runtime provides a virtual filesystem for accessing files inside `.lar` archives.

Example usage from Lua:

```lua
local LarAssets = require("LarAssets")
local file, err = LarAssets.open("image.png")
```

### Supported operations

* `read(n)` → read bytes
* `read("*l")` → read line
* `read("*a")` → read all remaining data
* `seek(mode, offset)` → move cursor (`set`, `cur`, `end`)
* `size()` → file size
* `lines()` → iterator over lines
* `close()` → close asset handle

---

### Implementation notes

* Assets are streamed directly from ZIP archives
* File handles are C++-managed (not Lua GC objects)
* Seeking reopens the ZIP entry (no persistent stream guarantee)

---

# Notes

* The system compiles `.lua` files into Lua bytecode (`.luac`) during build
* Bytecode is version-dependent and may not be portable across Lua versions
* Dependencies are loaded as nested runtime archives within the same Lua state
* The runtime uses a custom module loader integrated into Lua’s search system

---