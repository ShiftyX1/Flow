# Flow MOBA Map Editor

Native map-authoring tools inside Flow's normal 3D editor. The module builds plateau terrain, ramps and collision in 16 × 16-cell chunks. Objects are Godot scenes; navigation uses the existing NavigationServer3D. No voxel_world dependency is required.

## Getting started

Use a Flow editor built with `moba_map` and `navigation_3d` enabled. Open `modules/moba_map/demo/project.godot` for an editable example, select its **Map** node and use the MOBA toolbar. Run the example with F6/F5 to move the capsule hero by clicking the ground.

In another project:

1. Create a 3D scene and use **Project → Tools → Create MOBA map…**. Alternatively add a `MobaMap3D` node and click **Map settings**.
2. Choose the map dimensions, cell size and plateau height step. Defaults are 64 × 64 cells, 4 world units per cell and 2 units per level.
3. Select **Raise plateau**, **Lower plateau** or **Flatten**. Drag LMB to paint; the first cell establishes the target level for the whole stroke. Shift temporarily flattens to the first cell. Ctrl reverses raising/lowering.
4. Select **Ramp**, point at the low cell next to a cliff, and choose the direction toward the higher plateau and the ramp width. A green outline means the connection is valid. Click to place; Ctrl-click removes ramps across the chosen width.
5. Use **Paint material**, **Place object**, **Forest brush**, **Erase objects**, **Spawn** and **Point of interest**. Assign your own materials and PackedScenes in the map's **Palette** resource through the Inspector.
6. **Bake navigation** updates the navigation overlay. **Play map** bakes navigation, saves a new preview scene under `res://moba_previews/`, and starts it. Each preview gets a unique filename; existing scenes are not overwritten. Those scenes can be reopened or exported with matching Flow export templates.

## Editing behavior

- Select mode leaves the viewport to Godot's normal object selection and transform tools. Alt, RMB and MMB retain camera navigation. Reselect the Map node to return to its brushes.
- One drag is one undo action, including a whole forest brush stroke. Escape cancels an active stroke; otherwise it switches to Select. Ending a stroke outside the viewport also commits it.
- Circle and square brushes use a radius in terrain cells. Forest density is the probability of one placement in each visited cell per stroke; rotations vary, and placements within 1.5 units of an existing painted object are skipped.
- Object scenes must have a Node3D root. Add static collision shapes to scenes which should block the hero. The default tree and rock scenes already contain them. Navigation is baked from collision, not from decorative foliage.
- Painted object roots carry `moba_placed_object`, `moba_grounded` and `moba_height_offset` metadata. Grounded objects follow surface changes. Disable `moba_grounded` for a free-standing object, or change `moba_height_offset` for a vertical offset. The erase brush only removes objects placed through these tools.
- File-backed object scenes retain their prefab link. Embedded default scenes become ordinary owned scene nodes on placement, so their geometry and collision can be saved without external asset files.
- Spawn and point markers are ordinary selectable nodes with `kind` and `marker_id` Inspector properties. The preview uses the first Spawn; without one, it chooses the nearest navigable point to the map center.
- Level colors repeat every twelve levels; the hovered cell's exact level appears in the dock. Green navigation is current; red navigation is stale and disabled. Bake again after changing geometry or objects. Play map always requests a new bake.
- Scene data is saved normally in `.tscn`/`.tres`. Generated meshes, physics bodies and navigation are reconstructed from source data and are not duplicated in the saved scene. Runtime maps bake navigation when entering the scene tree; wait for `navigation_baked(true)` before querying paths.

## Terrain and script API

`MobaMapData.size` is an X/Z grid. Each cell stores `(level, material_index, ramp_direction)` in a flat PackedInt32Array, in Z-major order. Use `get_cell`, `set_cell` or `apply_cells` rather than editing packed offsets. Bulk edits are applied atomically and clear nearby ramps whose endpoints no longer connect.

Ramps occupy one low cell, rising by one height step toward North (-Z), East (+X), South (+Z) or West (-X). The cell behind must be flat at the low level; the cell ahead must be flat exactly one level higher. Wider ramps place adjacent parallel ramp cells. Changing levels can remove incompatible ramps, as part of the same undo action. Cells store materials, not blended texture weights; user materials receive repeating UVs with one repeat per cell.

`MobaMap3D.get_surface_height(Vector2(x, z))` and `get_plateau_level(Vector2(x, z))` use **map-local** coordinates. Heights on ramps are interpolated, while the plateau query returns the ramp's base level. Outside the map both return zero; use `map_data.contains(cell)` to distinguish an out-of-bounds sample. `raycast_surface(origin, direction)` accepts world coordinates and returns `position` and `local_position`, or an empty Dictionary. `get_markers()` returns direct marker children.

Keep map rotation and scale at their defaults. The preview/navigation profile uses radius 0.4, height 1.7, maximum climb 0.1 and maximum slope 50 degrees. Increase cell size or reduce height step if your ramps exceed this slope. Data dimensions are limited to 1024 per axis, levels to -128…128 and material indices to 0…255. These are storage limits, not measured performance guarantees.

The module is excluded from builds without 3D or 3D navigation. Editor-only code lives under `editor/`; `MobaMapPreview3D` is an optional runtime authoring harness, and gameplay scenes may use `MobaMap3D` alone.

## Version boundaries

This version covers plateau terrain, static obstacles, surface materials, object painting, markers and a click-to-move preview. It does not implement Hammer import, free sculpting, destructible trees, fog of war, creeps, multiplayer or game rules. Shared palettes are intentionally reusable; make a terrain resource unique in the Inspector before deliberately reusing it across independently edited maps.

Implementation was delivered without running builds, automated tests, editor sessions, exports or benchmarks, as requested. Verification and acceptance are left to the user.
