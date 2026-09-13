# Voxel World Module — Documentation

> **Engine**: Flow (Godot 4 fork)  
> **Module path**: `modules/voxel_world/`  
> **Language**: C++ (Godot module API)

---

## Table of Contents

1. [Overview](#1-overview)
2. [Architecture](#2-architecture)
3. [Block System](#3-block-system)
   - [VoxelBlockType Enum](#voxelblocktype-enum)
   - [VoxelBlockData (Thread-safe Cache)](#voxelblockdata-thread-safe-cache)
   - [VoxelBlockRegistry (Source of Truth)](#voxelblockregistry-source-of-truth)
4. [Biome System](#4-biome-system)
   - [VoxelBiomeRegistry (Code-only API)](#voxelbiomeregistry-code-only-api)
   - [Biome Properties](#biome-properties)
   - [Feature System](#feature-system)
   - [Biome Blending](#biome-blending)
5. [Terrain Generation](#5-terrain-generation)
   - [VoxelTerrainGenerator](#voxelterraingenerator)
   - [Noise Layers](#noise-layers)
6. [Chunk Management](#6-chunk-management)
   - [VoxelChunk](#voxelchunk)
   - [Loading & Unloading](#loading--unloading)
7. [Lighting System](#7-lighting-system)
   - [Sunlight BFS](#sunlight-bfs)
   - [Block Light (OmniLight3D)](#block-light-omnilight3d)
   - [VoxelLightMap](#voxellightmap)
8. [Meshing](#8-meshing)
   - [VoxelMesher](#voxelmesher)
   - [Vertex Format & Shader](#vertex-format--shader)
9. [VoxelWorld Node (Main API)](#9-voxelworld-node-main-api)
   - [Inspector Properties](#inspector-properties)
   - [GDScript API](#gdscript-api)
   - [Day/Night Cycle](#daynight-cycle)
   - [Fog System](#fog-system)
10. [Editor Plugin](#10-editor-plugin)
11. [Practical Guide](#11-practical-guide)
    - [Minimal Scene Setup](#minimal-scene-setup)
    - [Configuring Biomes from GDScript](#configuring-biomes-from-gdscript)
    - [Adding a Custom Block Type](#adding-a-custom-block-type)
    - [GDScript Examples](#gdscript-examples)

---

## 1. Overview

The **voxel_world** module adds a complete voxel engine to Flow. It provides:

- Procedural infinite terrain with biome blending
- **Data-driven biome system** — the engine handles math (noise, blending, caves, feature placement), while the game configures parameters via GDScript API
- **Declarative feature system** for trees (sphere, cone, bush canopy shapes) and scatter blocks per biome
- Multi-threaded chunk generation & meshing via `WorkerThreadPool`
- BFS-based sunlight propagation with per-vertex ambient occlusion
- Dynamic block light via Godot's `OmniLight3D` (with shadows)
- Per-block textures, shaders, and customizable physical properties
- Day/night cycle with sun, moon, and environment control
- Voxel-space raycasting and AABB-based collision (no `PhysicsServer` needed)
- Editor plugin for graphical block registry editing

### Design Philosophy

The module follows a clear separation of concerns:

- **Engine** (C++) — noise computation, biome weight blending, cave carving, feature placement algorithms, meshing, lighting
- **Game** (GDScript) — biome definitions (terrain shape, block types, temperature/humidity, tree styles), block registry configuration

The engine ships with a single default "Meadow" biome. Games add their own biomes (desert, forest, mountains, etc.) via the `VoxelBiomeRegistry` API.

---

## 2. Architecture

```
┌──────────────────────────────────────────────────────────┐
│                      VoxelWorld (Node3D)                  │
│  - Owns chunks, camera tracking, day/night cycle         │
│  - Dispatches generation tasks to WorkerThreadPool       │
│  - Integrates finished chunks on main thread             │
├───────────────┬───────────────┬───────────────────────────┤
│ VoxelTerrain  │ VoxelBlock    │ VoxelBiome                │
│ Generator     │ Registry      │ Registry                  │
│  - Noise      │  - Textures   │  - Code-only API          │
│  - Biome      │  - Physics    │  - Biome params           │
│    blending   │  - Lighting   │  - Climate position       │
│  - Caves      │  - Shaders    │  - Features (trees,       │
│  - Features   │               │    scatter)               │
├───────────────┴───────────────┴───────────────────────────┤
│                    Per Chunk Pipeline                      │
│  generate_chunk_data → propagate_sunlight → build_mesh    │
│                                                           │
│  VoxelChunk ←→ VoxelLightMap ←→ VoxelMesher              │
├───────────────────────────────────────────────────────────┤
│              VoxelBlockData (Thread-safe Cache)            │
│  - Static arrays read by mesher/light on worker threads   │
│  - Populated from VoxelBlockRegistry at world init        │
└──────────────────────────────────────────────────────────┘
```

**Data flow for a new chunk:**
1. Camera moves → `_update_chunks()` finds missing chunk positions
2. `_request_chunk()` dispatches `_chunk_generation_task()` to `WorkerThreadPool`
3. Worker thread: `generate_chunk_data()` → `propagate_sunlight()` → `build_chunk_mesh()`
4. Result pushed to `finished_chunks` vector (mutex-protected)
5. Main thread: `_integrate_finished_chunks()` applies mesh, scans for emissive blocks

---

## 3. Block System

### VoxelBlockType Enum

Defined in `voxel_block_data.h`. Integer IDs 0–10 are built-in:

| ID | Name     | Solid | Transparent | Emit | Light Opacity |
|----|----------|-------|-------------|------|---------------|
| 0  | Air      | No    | Yes         | 0    | 0             |
| 1  | Grass    | Yes   | No          | 0    | 15            |
| 2  | Dirt     | Yes   | No          | 0    | 15            |
| 3  | Stone    | Yes   | No          | 0    | 15            |
| 4  | Sand     | Yes   | No          | 0    | 15            |
| 5  | Water    | No    | Yes         | 0    | 2             |
| 6  | Snow     | Yes   | No          | 0    | 15            |
| 7  | Wood     | Yes   | No          | 0    | 15            |
| 8  | Leaves   | Yes   | No          | 0    | 1             |
| 9  | Bedrock  | Yes   | No          | 0    | 15            |
| 10 | Torch    | No    | Yes         | 14   | 0             |

`VOXEL_BLOCK_TYPE_MAX = 11`

### VoxelBlockData (Thread-safe Cache)

Static class with mutable arrays. **Do not modify directly** — these are populated
by `load_from_registry()` at world initialization.

```cpp
// Arrays (indexed by VoxelBlockType):
Color    block_colors[TYPE_MAX];
char*    block_names[TYPE_MAX];       // const, not overwritten
bool     block_solid[TYPE_MAX];
bool     block_transparent[TYPE_MAX];
uint8_t  block_emission[TYPE_MAX];
uint8_t  block_light_opacity[TYPE_MAX];
Color    block_light_color[TYPE_MAX];

// Query methods (all inline, safe from any thread):
bool    is_solid(VoxelBlockType)
bool    is_transparent(VoxelBlockType)
String  get_block_name(VoxelBlockType)
uint8_t get_block_emission(VoxelBlockType)
uint8_t get_block_light_opacity(VoxelBlockType)
Color   get_block_light_color(VoxelBlockType)
```

### VoxelBlockRegistry (Source of Truth)

Godot `Resource` subclass saved as `.tres`. This is the **single source of truth** for
all block definitions. Each block is a `BlockEntry`:

| Field              | Type             | Default        | Description                            |
|--------------------|------------------|----------------|----------------------------------------|
| `texture_top`      | `Texture2D`      | null           | Texture for top (+Y) face              |
| `texture_side`     | `Texture2D`      | null           | Texture for side faces                 |
| `texture_bottom`   | `Texture2D`      | null           | Texture for bottom (-Y) face           |
| `shader_material`  | `ShaderMaterial`  | null           | Custom per-block shader                |
| `mesh_height`      | `float`          | 1.0            | Visual height fraction (0.0–1.0)       |
| `collision_height` | `float`          | 1.0            | Physics AABB height fraction (0.0–1.0) |
| `color`            | `Color`          | White          | Vertex color when no texture           |
| `solid`            | `bool`           | true           | Blocks movement, hides adjacent faces  |
| `transparent`      | `bool`           | false          | Lets light through                     |
| `light_opacity`    | `int`            | 15             | Light absorbed per step (0–15)         |
| `emission`         | `int`            | 0              | Block light emitted (0–15)             |
| `light_color`      | `Color`          | White          | OmniLight3D color for emissive blocks  |

**Persistence mechanism**: Uses Godot's `_set`/`_get`/`_get_property_list` pattern (same as `MeshLibrary`). Properties are serialized as `block/<id>/<field>`.

**Engine defaults**: When `create_block(id)` is called for a built-in type, correct defaults (AIR=non-solid, WATER=transparent, TORCH=emissive, etc.) are applied automatically. When loading an **old** `.tres` that lacks new fields, the engine defaults fill in. When loading a **new** `.tres`, saved values overwrite the defaults.

**Key methods** (all bound to GDScript via ClassDB):

```gdscript
# Block CRUD
registry.create_block(id)
registry.remove_block(id)
registry.has_block(id) → bool
registry.get_block_list() → PackedInt32Array
registry.get_block_count() → int

# Textures
registry.set_block_texture_top(id, texture)
registry.get_block_texture_top(id) → Texture2D
# ... same for _side, _bottom
registry.get_block_texture_for_face(id, normal) → Texture2D  # auto-selects face
registry.block_has_texture(id) → bool

# Shader
registry.set_block_shader_material(id, material)
registry.get_block_shader_material(id) → ShaderMaterial
registry.block_has_shader(id) → bool

# Geometry
registry.set_block_mesh_height(id, 0.875)
registry.set_block_collision_height(id, 0.875)

# Physics & identity
registry.set_block_color(id, Color(...))
registry.set_block_solid(id, false)
registry.set_block_transparent(id, true)

# Lighting
registry.set_block_light_opacity(id, 0)
registry.set_block_emission(id, 14)
registry.set_block_light_color(id, Color(1.0, 0.6, 0.2))

# Initialize all built-in types
registry.setup_defaults()
```

---

## 4. Biome System

### VoxelBiomeRegistry (Code-only API)

`VoxelBiomeRegistry` is a `RefCounted` class (GDCLASS) that stores biome definitions. It is **not** a serializable resource — biomes are configured purely from code (GDScript or C++).

**Key design decisions:**
- The engine provides a single default biome ("Meadow") via `setup_defaults()`
- Games add their own biomes by calling `add_biome()` with a Dictionary of properties
- The registry is assigned to `VoxelWorld.biome_registry` before the world initializes
- If no registry is set, the engine uses its built-in single-biome default

**Lifecycle:**
1. Game creates `VoxelBiomeRegistry.new()` in `_init()` of a script extending `VoxelWorld`
2. Calls `setup_defaults()` to add the built-in Meadow biome
3. Calls `add_biome()` for each additional biome (desert, forest, mountains, etc.)
4. Assigns the registry: `biome_registry = reg`
5. On `_ready()`, `VoxelWorld` converts biome entries to `RuntimeBiomeData` and passes them to the terrain generator

**GDScript API:**

```gdscript
var reg = VoxelBiomeRegistry.new()

# Built-in default (Meadow with trees).
reg.setup_defaults()

# Add a biome.
var id: int = reg.add_biome("desert", {
    "height_base": 0.0,
    "height_scale": 8.0,
    "detail_scale": 1.0,
    "density_3d_weight": 0.3,
    "temperature": 0.8,
    "humidity": -0.6,
    "surface_block": VoxelWorld.VOXEL_BLOCK_SAND,
    "subsurface_block": VoxelWorld.VOXEL_BLOCK_STONE,
    "shore_block": VoxelWorld.VOXEL_BLOCK_SAND,
    "snow_block": VoxelWorld.VOXEL_BLOCK_SNOW,
    "snow_line": 999,
    "features": [
        {
            "type": "tree",
            "trunk_block": VoxelWorld.VOXEL_BLOCK_WOOD,
            "canopy_block": VoxelWorld.VOXEL_BLOCK_LEAVES,
            "density": 700,
            "canopy_shape": "cone",
        }
    ],
})

# Query.
reg.get_biome_count()           # → int
reg.get_biome_id("desert")      # → int (or -1)
reg.get_biome_name(0)           # → String
reg.has_biome(0)                # → bool

# Per-biome property modification after creation.
reg.set_biome_height_scale(id, 12.0)
reg.set_biome_temperature(id, 0.5)
reg.set_biome_surface_block(id, VoxelWorld.VOXEL_BLOCK_GRASS)

# Feature management.
reg.add_biome_feature(id, {"type": "scatter", "block": 10, "density": 200})
reg.get_biome_feature_count(id)   # → int
reg.get_biome_feature(id, 0)      # → Dictionary
reg.clear_biome_features(id)

# Remove biome (shifts IDs!).
reg.remove_biome(id)
reg.clear()
```

### Biome Properties

Each biome has the following configurable properties:

| Property            | Type   | Default | Description                                          |
|---------------------|--------|---------|------------------------------------------------------|
| `height_base`       | float  | 0.0     | Base height offset added to terrain                  |
| `height_scale`      | float  | 15.0    | Amplitude of the continentalness noise               |
| `detail_scale`      | float  | 3.0     | Amplitude of the detail/erosion noise                |
| `density_3d_weight` | float  | 0.5     | How much 3D noise affects terrain (caves, overhangs) |
| `temperature`       | float  | 0.0     | Position on temperature axis [-1, 1]                 |
| `humidity`          | float  | 0.0     | Position on humidity axis [-1, 1]                    |
| `surface_block`     | int    | 1       | Block ID for the top layer (e.g. grass)              |
| `subsurface_block`  | int    | 2       | Block ID for layers below surface (e.g. dirt)        |
| `shore_block`       | int    | 4       | Block ID for beach/shore areas (e.g. sand)           |
| `snow_block`        | int    | 6       | Block ID placed above snow_line (e.g. snow)          |
| `snow_line`         | int    | 165     | Y-level above which surface is replaced by snow      |

**Climate space**: Biomes are placed in a 2D (temperature, humidity) space. The generator samples noise to get a climate point, then blends nearby biomes using Gaussian weighting (distance-based).

### Feature System

Each biome can have zero or more **features** — declarative descriptions of what to place on the terrain surface. Features are specified as an array of Dictionaries in the `"features"` key when calling `add_biome()`, or added later with `add_biome_feature()`.

#### Tree Features

Trees consist of a trunk column and a canopy (leaf volume) on top.

| Key            | Type   | Default        | Description                              |
|----------------|--------|----------------|------------------------------------------|
| `type`         | String | —              | Must be `"tree"`                         |
| `trunk_block`  | int    | 7 (Wood)       | Block ID for the trunk                   |
| `canopy_block` | int    | 8 (Leaves)     | Block ID for the canopy/leaves           |
| `density`      | int    | 0              | Hash modulus — lower = denser (0 = none) |
| `min_trunk`    | int    | 4              | Minimum trunk height                     |
| `max_trunk`    | int    | 6              | Maximum trunk height                     |
| `canopy_radius`| int    | 2              | Radius of the canopy volume              |
| `canopy_shape` | String | `"sphere"`     | One of: `"sphere"`, `"cone"`, `"bush"`   |

**Canopy shapes:**

- **`sphere`** — Classic round canopy. A sphere with clipped corners around the trunk top. Good for oak/deciduous trees.
- **`cone`** — Spruce/pine shape. Tapered cone, wider at bottom and narrowing to a point. Each vertical layer reduces the radius.
- **`bush`** — Wide, low canopy. Large horizontal spread with minimal height. Good for savanna/acacia-style trees.

**Density**: The `density` value is a hash modulus. The engine hashes the world (x, z) coordinates and checks `hash % density == 0`. Lower values = more trees. Typical values: 15 (dense forest), 400 (sparse meadow), 700 (very sparse mountains). A value of 0 means the feature is disabled.

#### Scatter Features

Scatter places single blocks on the terrain surface (e.g. flowers, mushrooms, rocks).

| Key            | Type   | Default | Description                                    |
|----------------|--------|---------|------------------------------------------------|
| `type`         | String | —       | Must be `"scatter"`                            |
| `block`        | int    | 0       | Block ID to place                              |
| `density`      | int    | 0       | Hash modulus — lower = denser (0 = none)       |
| `min_y`        | int    | 0       | Minimum Y level for placement                  |
| `max_y`        | int    | 192     | Maximum Y level for placement                  |
| `surface_only` | bool   | true    | Only place on the terrain surface              |

**Feature salt**: Each feature in a biome's feature array uses a unique hash salt (based on its index), ensuring different features don't always coincide at the same positions.

### Biome Blending

The terrain generator uses temperature and humidity noise to determine the climate at each column. Each biome has a center point in (temperature, humidity) space. Biome weights are computed as:

```
weight[i] = exp(-4.0 * distance_squared(climate_point, biome_center[i]))
```

Weights are normalized so they sum to 1.0. Terrain parameters (height, detail, density) are then **linearly interpolated** across all biomes using these weights. This creates smooth transitions.

**Non-blendable parameters** (blocks, features, snow_line) come from the **dominant biome** — the one with the highest weight at that column.

---

## 5. Terrain Generation

### VoxelTerrainGenerator

Procedural terrain with 3D density-based generation.

**Chunk dimensions**: 16×192×16 blocks (`CHUNK_SIZE_X/Y/Z`)

### Noise Layers

- **Continentalness**: base terrain height variation (2D, low frequency)
- **Erosion**: detail-scale noise layered on top (2D)
- **3D density**: caves, overhangs, floating islands (3D)
- **Cave tunnels**: `abs(noise) < CAVE_THRESHOLD` → carved tunnel
- **Cheese caves**: `noise > CAVE_CHEESE_THRESHOLD` → large chambers
- **River noise**: creates river channels with banks
- **Lake noise**: creates lake basins at quantized water levels

**Key constants**:

| Constant                | Value | Description                            |
|-------------------------|-------|----------------------------------------|
| `BASE_HEIGHT`           | 64.0  | Default terrain height                 |
| `CAVE_THRESHOLD`        | 0.16  | Tunnel carving threshold               |
| `CAVE_CHEESE_THRESHOLD` | 0.26  | Chamber carving threshold              |
| `RIVER_WIDTH`           | 0.07  | River channel width                    |
| `LAKE_THRESHOLD`        | 0.35  | Lake formation threshold               |
| `OCEAN_THRESHOLD`       | -0.2  | Ocean depth threshold                  |

**Generation passes** (per chunk):

1. **Pass 1**: Compute surface height for each (x, z) column using blended biome parameters
2. **Pass 2**: Fill blocks — bedrock, subsurface, surface, snow, shore. Carve caves and fill water
3. **Pass 3**: Sunlight propagation (BFS from top)
4. **Pass 4**: Feature placement — iterate biome features for each column, place trees and scatter blocks

---

## 6. Chunk Management

### VoxelChunk

Stores blocks and light data for one 16×192×16 column.

| Property       | Type              | Description                                |
|----------------|-------------------|--------------------------------------------|
| `blocks`       | `Vector<uint8_t>` | Flat array, size = 16×192×16 = 49152       |
| `light_data`   | `Vector<uint8_t>` | Same size, nibble-packed: high=sun, low=block |
| `chunk_pos`    | `Vector2i`        | Chunk coordinates (not block coords)       |
| `mesh_instance`| `MeshInstance3D*`  | Owned mesh node in the scene tree          |

**Block indexing**: `index = x + z * SIZE_X + y * SIZE_X * SIZE_Z`

**Light encoding** (per byte):
- **High nibble** (bits 4–7): Sunlight level 0–15
- **Low nibble** (bits 0–3): Block light level 0–15

### Loading & Unloading

- Chunks in a square radius (`chunk_load_radius`) around the camera are loaded
- `chunks_per_frame` limits how many finished chunks are integrated per frame (default: 4)
- Unloaded chunks have their emissive block lights removed and mesh freed
- Worker thread pool handles generation and remeshing asynchronously

---

## 7. Lighting System

### Sunlight BFS

Sunlight starts at level 15 at the top of the chunk (y = SIZE_Y - 1) and propagates downward without attenuation through air. Horizontal spreading reduces the level by 1 per step. Each block's `light_opacity` is subtracted during propagation.

Called by `VoxelLightMap::propagate_sunlight()`. Runs on worker threads during chunk generation.

### Block Light (OmniLight3D)

Blocks with `emission > 0` spawn a Godot `OmniLight3D` node:
- `set_shadow(true)` — GPU-based shadow casting through the voxel world
- Energy = 10.0
- Range = `emission * block_size`
- Color = `block_light_color` from registry
- Tracked in `HashMap<Vector3i, OmniLight3D*> block_lights`

Managed by:
- `_spawn_block_light()` — creates the light node
- `_remove_block_light()` — destroys the light node
- `_scan_chunk_for_lights()` — scans a new chunk for emissive blocks

### VoxelLightMap

Static utility class. Key methods:

```cpp
// Full light propagation (sun + block BFS)
VoxelLightMap::propagate_all(light_data, blocks, neighbors);

// Sunlight only (used for chunk generation)
VoxelLightMap::propagate_sunlight(light_data, blocks, neighbors);

// After a block is placed/removed:
VoxelLightMap::remove_and_repropagate_sunlight(light_data, blocks, neighbors);
```

---

## 8. Meshing

### VoxelMesher

Static utility that builds mesh surfaces from chunk block data.

**Face culling**: A face is generated only when the adjacent block is transparent (or out of bounds). Neighbor chunk data is provided via `NeighborBlocks` for seamless cross-chunk meshing.

**Surface splitting**: Blocks are grouped by texture/shader. Each unique material variant becomes a separate `MeshSurface`. Untextured blocks are batched into a single vertex-colored surface.

**Per-vertex data**:

| Channel  | Format     | Content                                  |
|----------|------------|------------------------------------------|
| `VERTEX` | Vector3    | World-space position                     |
| `NORMAL` | Vector3    | Face normal                              |
| `COLOR`  | Color      | Block vertex color × texture tinting     |
| `UV`     | Vector2    | Texture coordinates (if textured)        |
| `CUSTOM0`| RGBA8_UNORM | R=sunlight, G=block_light, B=AO, A=255 |

**Ambient Occlusion**: Computed per-vertex using the 3-sample method (2 edge + 1 corner neighbor). Range: 0.34 (fully occluded) to 1.0 (open).

### Vertex Format & Shader

The built-in voxel shader:

```glsl
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;
uniform sampler2D texture_albedo : source_color, filter_nearest, repeat_enable;
global uniform float voxel_sun_intensity;
uniform bool use_texture = false;
varying vec4 voxel_light;

void vertex() {
    voxel_light = CUSTOM0;
}

void fragment() {
    vec4 base = use_texture ? texture(texture_albedo, UV) : vec4(1.0);
    ALBEDO = base.rgb * COLOR.rgb;
    float sun = voxel_light.r * voxel_sun_intensity;
    float ao = voxel_light.b;
    float brightness = sun * ao;
    brightness = max(brightness, 0.03);  // Minimum ambient
    ALBEDO *= brightness;
    if (use_texture) {
        ALPHA = base.a * COLOR.a;
        ALPHA_SCISSOR_THRESHOLD = 0.5;
    }
}
```

`voxel_sun_intensity` is updated as a global shader parameter by the day/night cycle system.

---

## 9. VoxelWorld Node (Main API)

### Inspector Properties

| Category            | Property              | Type                  | Default  |
|---------------------|-----------------------|-----------------------|----------|
| **World**           | `seed`                | int                   | -1 (random) |
|                     | `chunk_load_radius`   | int                   | 8        |
|                     | `block_size`          | float                 | 1.0      |
|                     | `sea_level`           | int                   | 52       |
|                     | `chunks_per_frame`    | int                   | 4        |
|                     | `max_pending_chunk_tasks` | int               | 16       |
|                     | `chunk_requests_per_frame` | int              | 8        |
|                     | `remesh_requests_per_frame` | int             | 4        |
|                     | `remeshes_per_frame`  | int                   | 2        |
|                     | `chunk_integration_time_budget_usec` | int      | 2000     |
| **Registries**      | `block_registry`      | VoxelBlockRegistry    | null     |
|                     | `biome_registry`      | VoxelBiomeRegistry    | null     |
| **Rendering**       | `texture_filter`      | TextureFilter         | NEAREST  |
|                     | `alpha_block_flags`   | int (bitfield)        | 0        |
| **Day/Night**       | `start_time_of_day`   | float (0–24)          | 7.0      |
|                     | `day_length_seconds`  | float                 | 600.0    |
|                     | `auto_advance_time`   | bool                  | false    |
|                     | `time_speed`          | float                 | 1.0      |
| **Fog**             | `fog_enabled`         | bool                  | true     |
|                     | `fog_distance_ratio`  | float                 | 0.85     |
| **Environment**     | `sun_path`            | NodePath              | ""       |
|                     | `moon_path`           | NodePath              | ""       |
|                     | `environment_path`    | NodePath              | ""       |
| **Debug**           | `verbose_logging`     | bool                  | true     |
|                     | `show_chunk_borders`  | bool                  | false    |
|                     | `chunk_border_color`  | Color                 | Yellow   |

### GDScript API

```gdscript
# Block interaction
var block_id: int = world.get_block_at(position)
world.set_block_at(position, VoxelWorld.BLOCK_TORCH)
var name: String = world.get_block_name_at(position)

# Coordinate conversion
var block_pos: Vector3i = world.world_to_block_pos(world_position)
var world_pos: Vector3 = world.block_to_world_pos(block_pos)

# Biome queries  
var biome_id: int = world.get_biome_at(position)
var biome_name: String = world.get_biome_name_at(position)

# Raycasting (no PhysicsServer needed)
var result: Dictionary = world.raycast_block(origin, direction, 10.0)
# Returns: { "position": Vector3, "normal": Vector3, "block_id": int, "block_pos": Vector3i }
# Returns empty dict on miss

# AABB collision (for character controllers)
var move: Dictionary = world.move_body(aabb, velocity, delta)
# Returns: { "position": Vector3, "velocity": Vector3, "on_ground": bool, "in_water": bool }

# Day/night
world.advance_time(1.0)  # Advance by 1 hour
world.is_daytime()        # true if 6:00–18:00
world.is_nighttime()      # true if 18:00–6:00
world.get_day_factor()    # 0.0 (midnight) → 1.0 (noon)
world.set_time_of_day(12.0)  # Set to noon
```

### Day/Night Cycle

When `auto_advance_time = true`, time advances in real-time based on `day_length_seconds` and `time_speed` multiplier.

The cycle controls:
- **Sun** (`DirectionalLight3D` via `sun_path`): rotation, energy, color temperature
- **Moon** (`DirectionalLight3D` via `moon_path`): rotation, energy
- **Environment** (`WorldEnvironment` via `environment_path`): ambient light, sky
- **Shader**: global `voxel_sun_intensity` parameter is updated once per frame

### Fog System

Distance-based fog scaled to draw distance:
- `fog_distance_ratio = 0.85` means fog starts fading at 85% of `chunk_load_radius * 16 * block_size`
- Controlled via the `Environment` resource referenced by `environment_path`

---

## 10. Editor Plugin

The module registers an editor inspector plugin for `VoxelBlockRegistry` resources.

**Features**:
- Graphical block editor dialog with per-block sections
- Per-block texture pickers (Top / Side / Bottom faces)
- Shader material picker (custom per-block shader)
- Physics controls: Solid checkbox, Transparent checkbox
- Vertex Color picker
- Lighting controls: Light Opacity (0–15), Emission (0–15), Light Color picker
- Full undo/redo integration via `EditorUndoRedoManager`

Access: Select a `VoxelBlockRegistry` resource in the inspector → click "Edit Block Textures & Shaders..."

---

## 11. Practical Guide

### Minimal Scene Setup

1. Create a new scene with a `Node3D` root
2. Add a `VoxelWorld` child node
3. Add a `Camera3D` as a child of `VoxelWorld` (the world tracks the active camera)
4. (Optional) Create a `VoxelBlockRegistry` resource (`.tres`) and assign it
5. (Optional) Attach a script extending `VoxelWorld` that creates a `VoxelBiomeRegistry` and assigns it in `_init()`
6. (Optional) Add `DirectionalLight3D` nodes for sun/moon and a `WorldEnvironment`
7. Run the scene — terrain generates automatically

If no `block_registry` is assigned, a default one is created with `setup_defaults()`.
If no `biome_registry` is assigned, a single default Meadow biome is used.

### Configuring Biomes from GDScript

Attach a script extending `VoxelWorld` to the VoxelWorld node. In `_init()` (which runs before `_ready()`), create and populate a `VoxelBiomeRegistry`:

```gdscript
extends VoxelWorld

func _init() -> void:
    var reg := VoxelBiomeRegistry.new()
    reg.setup_defaults()  # Adds "meadow" (id 0)

    # Desert — hot, dry, flat, no trees.
    reg.add_biome("desert", {
        "height_base": 0.0,
        "height_scale": 8.0,
        "detail_scale": 1.0,
        "density_3d_weight": 0.3,
        "temperature": 0.8,
        "humidity": -0.6,
        "surface_block": VoxelWorld.VOXEL_BLOCK_SAND,
        "subsurface_block": VoxelWorld.VOXEL_BLOCK_STONE,
        "shore_block": VoxelWorld.VOXEL_BLOCK_SAND,
        "snow_block": VoxelWorld.VOXEL_BLOCK_SNOW,
        "snow_line": 999,
    })

    # Forest — dense trees, lush.
    reg.add_biome("forest", {
        "height_base": 2.0,
        "height_scale": 18.0,
        "detail_scale": 3.0,
        "density_3d_weight": 0.8,
        "temperature": 0.0,
        "humidity": 0.6,
        "surface_block": VoxelWorld.VOXEL_BLOCK_GRASS,
        "subsurface_block": VoxelWorld.VOXEL_BLOCK_DIRT,
        "shore_block": VoxelWorld.VOXEL_BLOCK_SAND,
        "snow_block": VoxelWorld.VOXEL_BLOCK_SNOW,
        "snow_line": 165,
        "features": [
            {
                "type": "tree",
                "trunk_block": VoxelWorld.VOXEL_BLOCK_WOOD,
                "canopy_block": VoxelWorld.VOXEL_BLOCK_LEAVES,
                "density": 15,
                "canopy_shape": "sphere",
            }
        ],
    })

    # Mountains — tall, rocky, sparse cone-shaped trees.
    reg.add_biome("mountains", {
        "height_base": 20.0,
        "height_scale": 45.0,
        "detail_scale": 12.0,
        "density_3d_weight": 2.5,
        "temperature": -0.6,
        "humidity": 0.0,
        "surface_block": VoxelWorld.VOXEL_BLOCK_STONE,
        "subsurface_block": VoxelWorld.VOXEL_BLOCK_STONE,
        "shore_block": VoxelWorld.VOXEL_BLOCK_SAND,
        "snow_block": VoxelWorld.VOXEL_BLOCK_SNOW,
        "snow_line": 70,
        "features": [
            {
                "type": "tree",
                "trunk_block": VoxelWorld.VOXEL_BLOCK_WOOD,
                "canopy_block": VoxelWorld.VOXEL_BLOCK_LEAVES,
                "density": 700,
                "canopy_shape": "cone",
            }
        ],
    })

    biome_registry = reg
```

**Important**: The registry must be assigned in `_init()`, not `_ready()`, because `VoxelWorld` reads `biome_registry` during `NOTIFICATION_READY`.

### Adding a Custom Block Type

1. Add a new entry to the `VoxelBlockType` enum in `voxel_block_data.h`:
   ```cpp
   VOXEL_BLOCK_TORCH,
   VOXEL_BLOCK_MY_NEW_BLOCK,  // Add before TYPE_MAX
   VOXEL_BLOCK_TYPE_MAX
   ```

2. Add a name in `voxel_block_data.cpp`:
   ```cpp
   const char *VoxelBlockData::block_names[...] = {
       ... 
       "Torch",
       "MyNewBlock",
   };
   ```

3. Update all other arrays in `voxel_block_data.cpp` with default values.

4. Add defaults in `VoxelBlockRegistry::create_block()`:
   ```cpp
   case VOXEL_BLOCK_MY_NEW_BLOCK:
       blocks[p_id].color = Color(0.8f, 0.2f, 0.5f);
       blocks[p_id].solid = true;
       // ... etc
       break;
   ```

5. Rebuild: `scons`

6. Assign textures in the editor via the Block Registry editor dialog.

### GDScript Examples

**Place/break blocks with raycast:**
```gdscript
func _input(event):
    if event is InputEventMouseButton and event.pressed:
        var camera = get_viewport().get_camera_3d()
        var origin = camera.global_position
        var dir = -camera.global_basis.z
        var hit = voxel_world.raycast_block(origin, dir, 8.0)
        
        if hit.is_empty():
            return
        
        if event.button_index == MOUSE_BUTTON_LEFT:
            # Break block
            voxel_world.set_block_at(
                voxel_world.block_to_world_pos(hit["block_pos"]), 0)
        
        elif event.button_index == MOUSE_BUTTON_RIGHT:
            # Place block on the face
            var place_pos = hit["block_pos"] + Vector3i(hit["normal"])
            voxel_world.set_block_at(
                voxel_world.block_to_world_pos(place_pos), 10)  # Torch
```

**Simple character controller:**
```gdscript
func _physics_process(delta):
    var aabb = AABB(global_position - Vector3(0.3, 0, 0.3), Vector3(0.6, 1.8, 0.6))
    var result = voxel_world.move_body(aabb, velocity, delta)
    
    global_position = result["position"]
    velocity = result["velocity"]
    
    if result["on_ground"]:
        if Input.is_action_just_pressed("jump"):
            velocity.y = 8.0
    else:
        velocity.y -= 20.0 * delta
    
    if result["in_water"]:
        velocity.y = max(velocity.y, -2.0)  # Slow sinking
```
