# Fado Game Engine

##### Ongoing project

"Fado Engine" is a custom 2D/3D game engine written in **C-Style C++** using **Direct3D 11** as the graphics library. The engine is
heavily **data-oriented**: state lives in flat structs and arrays, and plain functions
operate on that state, no inheritance-heavy OOP, no per-object heap allocations, no
dynamic containers in the hot path.

![Description](media/showcase.gif)

---

## Table of Contents

- [Project Layout](#project-layout)
- [Build System](#build-system)
- [Architecture Overview](#architecture-overview)
- [Win32 Platform Layer](#win32-platform-layer)
- [Renderer](#renderer)
- [Game Layer & Levels](#game-layer--levels)
- [The Shared Struct System (`FSharedStuff`)](#the-shared-struct-system-fsharedstuff)
- [Memory: The Arena System](#memory-the-arena-system)
- [Custom Asset Pipeline (`.f*` formats)](#custom-asset-pipeline-f-formats)
- [Entities, Transforms & Handles](#entities-transforms--handles)
- [Subsystems](#subsystems)
- [Conventions](#conventions)
- [Latest Features](#latest-features)

---

## Project Layout

```
FadoEngine/
├── Engine/                       # The Engine project. Builds to FadoEngine.exe
│   ├── Code/                     # Engine + renderer source (fado_*.h/.cpp)
│   ├── Shaders/                  # HLSL shaders (material, particle, UI)
│   ├── Assets/                   # Cooked, engine-ready assets (.fimage, .fmodel, .ffont, .fsound)
│   ├── AssetsSource/              # Original source assets (.png, .glb, .wav, .ttf) before cooking
│   ├── Tools/FadoConverter/       # Standalone tool that cooks source assets into .f* formats
│   └── ThirdParty/                # imgui, lz4, stb_image, stb_truetype, stb_dxt
│
├── Game/                          # The Game project. Builds to Game.dll
│   └── Code/
│       ├── fado.cpp / fado.h      # Game <-> engine connector layer, UI helpers
│       ├── fado_level.h           # Level struct, load/save/spawn logic
│       ├── fado_input.h           # Input structs (keyboard, mouse, controllers)
│       ├── fado_collision.cpp     # Collision detection implementation
│       └── Levels/                # Individual level headers (2D/3D showcases, etc.)
│
├── premake5.lua                   # Generates the Visual Studio solution
├── premake5.exe                   # Required for the .sln generation
└── fado_build.bat                 # Runs premake5 to (re)generate the .sln
```

The engine is split into **two build targets** that are compiled and linked separately,
on purpose:

| Project    | Kind          | Contains                                            |
|------------|---------------|------------------------------------------------------|
| **Engine** | `.exe`        | Win32 platform layer, DX11 renderer, asset loaders    |
| **Game**   | `.dll`        | Game update logic, levels, collision, input handling  |

---

## Build System

Fado uses **premake5** rather than a hand-written Visual Studio solution.

```
fado_build.bat -> runs premake5.exe vs2026 -> generates FadoEngine.sln
```

`premake5.lua` defines both projects:
- **Engine** (`WindowedApp`): compiles everything under `Engine/Code`, the shader
  sources (marked `buildaction "None"` since they're compiled at runtime via
  `D3DCompileFromFile`, not by MSVC), and all ThirdParty code. A post-build step copies
  `Assets/` and `Shaders/` next to the built `.exe` so it's runnable standalone.
- **Game** (`SharedLib`): compiles `Game/Code` into `Game.dll`, defining `GAME_DLL` so
  shared headers (like the logger) know which build they're in.

Both projects share the same `Debug`/`Release` configuration split, and both are built
into the same output directory so the `.exe` can find and hot-reload the `.dll` at runtime.

---

## Architecture Overview

The engine is organized into three conceptual layers, matching the project's own
description of itself:

```
┌─────────────────────────────────────────────────────────┐
│  Win32 Platform Layer  (Engine/Code/fado_win32.*)       │
│  - Owns the window, the message loop, input polling     │
│  - Owns XAudio2 sound output                            │
│  - Loads/hot-reloads Game.dll                           │
│  - Drives the main loop: Update DLL → Render            │
└───────────────┬───────────────────────────────┬─────────┘
                │                                 │
                ▼                                 ▼
┌────────────────────────────┐      ┌──────────────────────────────┐
│  Renderer  (fado_d3d.*)    │      │  Game  (Game.dll)            │
│  - DX11 device/swapchain   │◄────►│  - Level system              │
│  - Shaders, buckets, draws │      │  - Entity spawning           │
│  - Particle system         │      │  - Collision, input handling │
│  - Frustum culling         │      │  - UI logic                  │
└────────────────────────────┘      └──────────────────────────────┘
                │                                 │
                └───────────────┬─────────────────┘
                                ▼
                    FSharedStuff (fado_shared.h)
              The one struct both sides read/write
```

Data flows through **one preallocated memory block** (the arena system, see below),
and both the exe and the dll operate on the *same* `FSharedStuff` instance, the exe
just hands the dll a pointer to it every frame.

---

## Win32 Platform Layer

**Files:** `fado_win32.h`, `fado_win32.cpp`

This is where `main` (`wWinMain`) lives, and the only part of the engine that is
Windows-specific. Its responsibilities:

- **Window creation & the message loop.** Creates the `HWND`, registers the window
  class, and pumps Windows messages every frame (`Win32HandleWindowsMessageLoop`).
- **Game code hot reloading.** `Win32LoadGameCode`/`Win32UnloadGameCode` load
  `Game.dll` (via a temp-file copy trick so the original `.dll` isn't locked while the
  build system rewrites it), and the main loop checks the DLL's last-write time every
  frame to reload it automatically when you rebuild.
- **Input.** Polls keyboard/mouse via Win32 messages and controllers via XInput
  (`Win32HandleControllerInput`), filling in `FGameInput` (defined in `Game/Code/fado_input.h`)
  which is then handed to the game DLL.
- **Sound output.** Owns the XAudio2 device (`Win32SoundState`), a small pool of 3D
  voices (`Win32VoiceSlot3D[WIN32_MAX_3D_VOICES]`) for spatial sounds, and streams the
  mixed output buffer built by the sound system each frame.
- **Fullscreen.** Implemented as **manual borderless fullscreen** (`ToggleFullscreen`
  strips `WS_OVERLAPPEDWINDOW` and resizes the window to the monitor bounds) — the swap
  chain itself is always created windowed; DXGI's own exclusive-fullscreen mode is
  intentionally not used, to avoid two competing fullscreen mechanisms fighting each other.
- **The main loop**, in order, every frame:
  1. Compute `deltaTime` via `QueryPerformanceCounter` (clamped to avoid huge spikes
     after a blocking resize/move).
  2. Check for/apply DLL hot reload.
  3. Pump ImGui's new-frame (debug builds only).
  4. Handle input.
  5. Call `gameCode.gameUpdate(gameState, input)`, the *only* function pointer the
     exe calls into the dll.
  6. Call `Render`.
  7. Mix and submit audio.
  8. Snapshot input button states for next frame's "was down" comparisons.

The platform layer allocates **all** engine memory once via `VirtualAlloc` at startup
and never allocates again during the session (DX11's own internal driver allocations
are the one unavoidable exception).

---

## Renderer

**Files:** `fado_d3d.h`, `fado_d3d.cpp`, `Shaders/*.hlsl`

The renderer is a self-contained DX11 layer. Only `InitializeFD3D` and `Render` (plus
the asset loaders, a couple of mesh builders and a resize window function) are exposed outside this file, every
DX11 type (`ID3D11Device`, `ID3D11DeviceContext`, shader/buffer objects) is contianed in
`fado_d3d.*`.

**Core pieces:**

| Concept | Struct | Purpose |
|---|---|---|
| Device/swapchain/depth buffer | `FD3D` | The raw DX11 objects every renderer needs |
| Main shader | `FMaterialShader` | One unified shader for lit/unlit, textured/colored, transparent entities |
| Particle shader | `FParticleShader` | GPU-instanced billboard rendering for particles |
| UI shader | `FUIShader` | Screen-space quads with vertex color, used by the UI bucket |
| Draw call | `FDrawCall` / `FRenderBucket` | Draw calls are pushed into opaque/transparent buckets and flushed once per frame |
| Meshes | `FMeshBuffer` | Raw vertex/index buffer pool shared by all loaded models |
| Frustum culling | `FFrustum` / `FFrustumPlane` | Extracted from the view-projection matrix; used to skip drawing (and shadow-blobbing) off-screen entities |

**Per-frame flow (`Render`):**
1. `BeginScene`: clear back buffer + depth.
2. `RenderCamera`: rebuild the view matrix from the camera entity's transform.
3. Frustum-cull and push every visible entity into the opaque or transparent bucket
   (based on `Material_Transparent`/alpha), plus a **blob shadow** draw call for any
   entity flagged `Material_CastShadow`.
4. `FlushBuckets`: draws opaque, then transparent, then all active particle emitters,
   then the UI bucket, each with the appropriate blend/depth/rasterizer state set once
   per bucket (not per draw call).
5. `EndScene`: presents the back buffer (and renders ImGui, in debug builds).

**Shadows** are handled as simple **blob shadows**: a soft, alpha-faded quad decal
projected onto the ground beneath a shadow-casting entity, rather than full shadow
mapping. There are two variants: `PushBlobShadow` (a ground-plane quad for 3D entities)
and `PushBlobShadow2D` (a camera-facing squashed ellipse for sprites).

**Particles** are **CPU-simulated, GPU-instanced**: particle position/age/color/size are
updated on the CPU each frame (`fado_particles.h`), compacted into a small
`FParticleInstance` array, uploaded to a dynamic instance buffer, and drawn with a
single `DrawIndexedInstanced` call per emitter, no per-particle draw calls, no compute
shaders.

---

## Game Layer & Levels

**Files:** `Game/Code/fado.h`, `fado.cpp`, `fado_level.h`, `Levels/*.h`

The Game project compiles to `Game.dll` and exposes exactly one function to the engine:

```cpp
#define GAME_UPDATE(name) void name(FGameState* gameState, struct FGameInput* input)
typedef GAME_UPDATE(FGameUpdate);
```

`FGameState` is the game's own top-level struct; it holds a pointer to the shared
data (`FSharedStuff* shared`), the sound manager, the current level, the loaded UI font,
and a couple of small pieces of state (camera yaw/pitch, pause flag).

**Levels** are just a name plus three function pointers:

```cpp
struct FLevel
{
    void (*Init)(FGameState*);
    void (*Begin)(FGameState*);
    void (*Update)(FGameState*, f32 dt);
    cc8* name;
};
```

To add a new level: create a header with a struct that fills in these three functions
and a `Make...()` helper that returns a populated `FLevel` (see `level_2d_showcase.h` /
`level_3d_showcase.h` for the existing pattern). `Init` always runs to set up an
entity's default state; if a saved `.flevel` file exists, its entities are then
overridden **in the same spawn order** (order matters; a saved handle must still point
at the same conceptual entity when reloaded).

Levels save/load via `SaveCurrentLevel`/`LoadLevel`, which serialize each entity's
`FEntityDesc` (position, rotation, scale, entity data) to a custom `.flevel` file using
the same asset-header format the rest of the engine's asset pipeline uses.

---

## The Shared Struct System (`FSharedStuff`)

**File:** `fado_shared.h`

Because the renderer (exe) and the game (dll) are two separately compiled binaries,
they need one common block of data both can read and write without either owning the
other. That's `FSharedStuff`:

```cpp
struct FSharedStuff
{
    FCamera camera;
    FViewPort viewport;
    FDirectionalLight dirLight;

    FAssetsHandles assets;

    FEntityTable entityTable;
    FTransforms transforms;
    FCollisionWorld collisionWorld;
    FSpriteSheetTable spriteSheetTable;
    FParticleEmitterTable particles;
    FUICommandsBucket uiBucket;

    FEngineMemory* arena;

#if FADO_DEBUG
    HEntity selectedEntity;
    b32 canSelect;
#endif  // FADO_DEBUG
};
```

Every major subsystem's runtime state (entities, transforms, collision world, particle
pool, UI command bucket, sprite sheets, asset handles) is embedded **by value**, not by
pointer; a deliberate choice for cache locality and to avoid a scattering of separate
arena allocations for each subsystem. This does mean every subsystem header it embeds
must be fully defined (not just forward-declared) before `FSharedStuff` itself, which
is why `fado_shared.h` exists, it includes the required files and then defines `FSharedStuff` on top of all of them.

The renderer's own `FRenderWorld` holds a `FSharedStuff*` pointer (not its own copy),
and `FGameState` does the same, both sides are always looking at the one instance
allocated once in `wWinMain`.

---

## Memory: The Arena System

**File:** `fado_types.h`

Fado does **no dynamic (heap) allocation** during gameplay. Everything lives in one
`VirtualAlloc`'d block, split into three arenas with different lifetimes:

```cpp
struct FEngineMemory
{
    FMemoryArena permanent; // lives for the entire session
    FMemoryArena scratch;   // transient — MUST be reset manually after use
    FMemoryArena level;     // reused every time a level (re)loads
};
```

| Arena | Size | Lifetime | Used for |
|---|---|---|---|
| `permanent` | 64 MB | Whole process | `FGameState`, `FSharedStuff`, the render world, all loaded shaders, anything created once at startup |
| `scratch` | 15 MB | One operation | Temporary buffers during asset loading/decompression. Always `ArenaReset()` immediately after use |
| `level` | 1 MB | Per level | The current `FLevel`'s own data, zeroed on unload |

A `FMemoryArena` is nothing more than a base pointer, a used offset, and a size; a
linear "bump" allocator:

```cpp
internal void* AreaPushSize_(FMemoryArena* arena, u32 size)
{
    Assert((arena->used + size) <= arena->size);
    void* result = arena->base + arena->used;
    arena->used += size;
    return result;
}
```

Three convenience macros wrap it:

```cpp
ArenaPushType(arena, FGameState)          // push one instance of a type
ArenaPushSize(arena, u8, sizeInBytes)     // push a raw byte block
ArenaPushArray(arena, FEntity, count)     // push a fixed-count array of a type
```

There's no per-allocation `Free`; arenas are only ever reset wholesale
(`ArenaReset`, which just zeroes `used`), matching each arena's own lifetime: scratch
resets after every load, level resets on level unload, and permanent never resets at all.

---

## Custom Asset Pipeline (`.f*` formats)

**Files:** `fado_asset_format.h`, `Engine/Tools/FadoConverter/`

Rather than loading `.png`/`.wav`/`.glb`/`.ttf` directly at runtime, Fado **cooks**
source assets ahead of time into its own compact, LZ4-compressed formats:

```
AssetsSource/           Assets/ (cooked, what the engine actually loads)
├── Textures/*.png   ->  ├── Textures/*.fimage
├── Models/*.glb     ->  ├── Models/*.fmodel
├── Fonts/*.ttf      ->  ├── Fonts/*.ffont
└── Audio/*.wav      ->  └── Audio/*.fsound
```

`FadoConverter` (a separate standalone tool project, excluded from the main Engine
build) reads the source formats and writes cooked `.f*` files. Every cooked file shares
one common header:

```cpp
struct FAssetHeader
{
    u32 magic;      // "FAS1"
    u32 assetType;  // FASSET_TYPE_IMAGE / MODEL / FONT / SOUND / LEVEL
    u32 version;
    u32 reserved;
};
```

followed by a type-specific header (`FImageHeader`, `FModelHeader`, `FFontHeader`,
`FSoundHeader`, or `FLevelHeader`) and then the payload; LZ4-compressed pixel data,
vertex/index blobs, PCM samples, or serialized entity descriptors, depending on type.
Textures additionally support **BC1/BC3 block-compressed** formats with mip chains
alongside plain RGBA8.

At runtime, the engine's loaders (`LoadFImage`, `LoadFModel`, `LoadFFont`, `LoadFSound`,
`LoadLevel`) all follow the same shape: read the header, `LZ4_decompress_safe` the
payload into the scratch arena, upload to the appropriate GPU/engine resource, then
reset scratch.

---

## Entities, Transforms & Handles

**File:** `fado_types.h`

Fado uses a lightweight, array-based Entity Component pattern rather than a general
ECS framework:

- **Handles** (e.g. `HEntity`, `HMesh`, are all just `u32` indices into a fixed-size array, `INVALID_HANDLE` (`0xFFFFFFFF`)
  marks "no value."
- **`FEntityTable`** is a flat array of `FEntity` (mesh handle, material, sprite rect,
  animation state), capped at `FMAX_ENTITIES`.
- **`FTransforms`** is a **parallel array** indexed by the *same* handle as its entity;
  `positions[i]`, `scales[i]`, `rotations[i]` for entity `i`. There's no per-entity
  transform struct; the three arrays are kept separate for cache-friendly bulk access.
- **`FMaterial`** drives how an entity renders: a base color/tint, an optional texture
  handle, and a small flags byte (`Material_Lit`, `Material_Transparent`,
  `Material_CastShadow`) that the renderer reads to decide lighting, blending, and
  whether to emit a blob shadow.

Spawning an entity (`SpawnEntity` in `fado_level.h`) is nothing more than writing into
the next free slot of these arrays, no allocation, no indirection beyond the handle.

---

## Subsystems

A few other systems round out the engine, each self-contained in its own header:

- **`fado_particles.h`**: CPU-simulated particle emitters. Each emitter defines
  lifetime, spawn rate, and four animatable properties (position, size, color, speed) as
  **ranges** (`FRangeF32`/`FRangeV3`/`FRangeV4`) rather than fixed values, every
  particle rolls its own concrete start/end value from those ranges at spawn, giving
  natural per-particle variation without per-particle authoring.
- **`fado_collision.h`** (declarations, Game project) / `fado_collision.cpp`
  (implementation): AABB and OBB collision detection, with static/kinematic/dynamic/
  physics/trigger collider flags.
- **`fado_sound.h`**: 2D and 3D sound mixing on top of XAudio2; 3D sounds are
  positioned by simple distance/direction-to-listener math rather than a full audio
  engine.
- **`fado_sprite_anim.h`**: sprite sheet registration and frame-based 2D animation
  state, driving `FEntity::spriteRect` over time.
- **`fado_ui.h`**: an immediate-ish UI command bucket (`FUICommandsBucket`): game code
  pushes rects/text for the frame, the renderer draws them all in one pass, separate
  from ImGui (which is debug-only and used for the entity inspector).
- **`fado_math.h`**: the engine's own vector/quaternion/matrix math on top of the raw
  `v2`/`v3`/`v4`/`mat3`/`mat4` types defined in `fado_types.h`.
- **`fado_log.h`**: a minimal file logger (`FLOG(level, fmt, ...)`), debug-build only,
  writing to `EditorLog.txt` (exe) or `GameLog.txt` (dll) separately since they're
  different processes/modules.

---

## Conventions

- All engine files are prefixed `fado_` (e.g. `fado_types.h`).
- All custom types are prefixed `F` (Fado), e.g. `FEntity`, `FCamera`.
- Any type named `HSomething` is a `u32` handle into a fixed array, not a pointer.
- `fado_types.h` is included by (almost) every engine file, it's the one header with
  no engine-specific dependencies.
- No dynamic (`new`/`malloc`) allocation happens in gameplay code, everything comes
  from one of the three arenas.
- `FADO_DEBUG` gates everything development-only: `Assert`, ImGui, the debug line/AABB
  drawing, `FLOG`.

---

