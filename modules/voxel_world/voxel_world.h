#pragma once

#include "voxel_biome_registry.h"
#include "voxel_block_registry.h"
#include "voxel_chunk.h"
#include "voxel_light_map.h"
#include "voxel_mesher.h"
#include "voxel_structure_registry.h"
#include "voxel_terrain_generator.h"
#include "voxel_world_save.h"

#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/resources/environment.h"
#include "scene/resources/material.h"

class AnimationPlayer;

class VoxelWorld : public Node3D {
	GDCLASS(VoxelWorld, Node3D);

public:

private:
	int seed = -1;
	int chunk_load_radius = 8;
	float block_size = 1.0f;
	int sea_level = 52;
	int chunks_per_frame = 4; // Max chunks to integrate into scene tree per frame.
	int max_pending_chunk_tasks = 16; // Max background chunk generation tasks in flight.
	int chunk_requests_per_frame = 8; // Max new chunk generation tasks to enqueue per frame.
	int remesh_requests_per_frame = 4; // Max deferred remesh tasks to enqueue per frame.
	int remeshes_per_frame = 2; // Max remesh results to apply per frame.
	int chunk_integration_time_budget_usec = 2000; // Main-thread mesh integration budget.
	BaseMaterial3D::TextureFilter texture_filter = BaseMaterial3D::TEXTURE_FILTER_NEAREST;
	bool verbose_logging = true;

	// --- Chunk border debug visualization ---
	bool show_chunk_borders = false;
	Color chunk_border_color = Color(1.0f, 1.0f, 0.0f, 1.0f); // Yellow by default.
	HashMap<Vector2i, MeshInstance3D *> chunk_border_instances;

	void _spawn_chunk_border(const Vector2i &p_chunk_pos);
	void _despawn_chunk_border(const Vector2i &p_chunk_pos);
	void _rebuild_all_chunk_borders();
	void _clear_all_chunk_borders();

	// --- Day/Night Cycle ---
	float start_time_of_day = 7.0f; // Editor-set start time (0.0–24.0).
	float time_of_day = 7.0f; // Runtime current time (0.0–24.0 hours).
	float day_length_seconds = 600.0f; // Real seconds per full day cycle.
	bool auto_advance_time = false;
	float time_speed = 1.0f; // Multiplier for time advancement speed.

	// --- Fog ---
	bool fog_enabled = true;
	float fog_distance_ratio = 0.85f; // Fog end as fraction of draw distance.
	float current_local_light = 1.0f; // Smoothed local light level at camera (0=dark cave, 1=full sun).

	// --- Environment node references (resolved from NodePath) ---
	NodePath sun_path;
	NodePath moon_path;
	NodePath environment_path;
	DirectionalLight3D *sun_node = nullptr;
	DirectionalLight3D *moon_node = nullptr;
	WorldEnvironment *env_node = nullptr;

	Ref<VoxelBlockRegistry> block_registry;
	Ref<VoxelBiomeRegistry> biome_registry;
	Ref<VoxelStructureRegistry> structure_registry;

	VoxelTerrainGenerator *generator = nullptr;
	HashMap<Vector2i, VoxelChunk *> loaded_chunks;
	HashSet<Vector2i> dirty_saved_chunks;
	HashMap<Vector2i, Vector<uint16_t>> pending_saved_chunk_blocks;

	struct WorldObjectEntry {
		int64_t id = 0;
		StringName type;
		Vector3i block_pos;
		float rotation_y = 0.0f;
		Dictionary state;
		bool blocking = false;
	};

	HashMap<int64_t, WorldObjectEntry> world_objects;
	HashMap<Vector2i, Vector<int64_t>> object_ids_by_chunk;
	HashSet<Vector2i> dirty_saved_object_chunks;
	HashMap<Vector2i, Array> pending_saved_chunk_objects;
	int64_t next_world_object_id = 1;
	Ref<StandardMaterial3D> material;
	Ref<Shader> voxel_shader;
	Ref<ShaderMaterial> voxel_untextured_shader_material;
	HashMap<Ref<Texture2D>, Ref<ShaderMaterial>> voxel_textured_shader_material_cache;
	HashMap<Ref<Texture2D>, Ref<StandardMaterial3D>> standard_texture_material_cache;
	HashMap<Ref<Texture2D>, Ref<StandardMaterial3D>> standard_alpha_texture_material_cache;
	float last_voxel_sun_intensity = -1.0f;

	String world_save_dir;
	Dictionary world_save_metadata;
	Dictionary world_save_player_state;
	Dictionary world_save_character_state;
	bool world_save_loaded = false;

	bool initialized = false;
	Vector2i last_camera_chunk = Vector2i(INT32_MAX, INT32_MAX);

	// --- Async chunk loading ---

	// Data produced by background thread (no Godot scene API).
	struct ChunkTaskResult {
		Vector2i key;
		Vector<uint16_t> blocks;
		Vector<uint8_t> light_data;
		Vector<VoxelMesher::MeshSurface> surfaces;
		bool is_remesh = false; // true = update existing chunk, false = create new chunk
		bool blocks_from_save = false;
	};

	// Chunks currently being generated on background threads.
	HashMap<Vector2i, int64_t> pending_chunks; // key -> WorkerThreadPool TaskID
	// Pending mesh-rebuild tasks for already-loaded chunks.
	HashMap<Vector2i, int64_t> pending_remesh; // key -> WorkerThreadPool TaskID
	// Chunks whose light needs to be recomputed (deferred when remesh can't be queued immediately).
	HashSet<Vector2i> dirty_light;
	// Finished results waiting to be integrated on the main thread.
	Mutex finished_mutex;
	// Keep this lock for handoff only; turning result reshuffles into a critical section makes workers part of frame pacing.
	Vector<ChunkTaskResult> finished_chunks;
	int last_integrated_load_count = 0;
	int last_integrated_remesh_count = 0;
	int last_remesh_request_count = 0;
	int last_integration_time_usec = 0;

	// Background task entry point (static, thread-safe).
	struct ChunkTaskData {
		// This raw owner pointer stays legal only while teardown owns every task; otherwise it is just a delayed use-after-free.
		VoxelWorld *world;
		Vector2i key;
		int chunk_x = 0;
		int chunk_z = 0;
		bool is_remesh = false;
		Vector<uint16_t> pre_blocks; // block data snapshot used for remesh tasks
		// Snapshots of loaded neighbour block arrays at task-queue time (empty = not loaded)
		Vector<uint16_t> neighbor_px, neighbor_nx, neighbor_pz, neighbor_nz;
		// Light data snapshots for neighbours (used during remesh).
		Vector<uint8_t> neighbor_light_px, neighbor_light_nx, neighbor_light_pz, neighbor_light_nz;
		Vector<uint8_t> pre_light; // light snapshot for remesh
	};
	static void _chunk_generation_task(void *p_userdata);

	// Integrate finished chunks into the scene tree (main thread only).
	void _integrate_finished_chunks();

	// Apply pre-built mesh surfaces to a chunk (main thread only).
	void _apply_surfaces_to_chunk(VoxelChunk *p_chunk, const Vector<VoxelMesher::MeshSurface> &p_surfaces);
	// Queue a background mesh-rebuild task for an already-loaded chunk.
	void _request_remesh(const Vector2i &p_key);
	// Build mesh on main thread (used only for immediate response in set_block_at).
	void _rebuild_chunk_mesh(VoxelChunk *p_chunk);
	int _request_missing_chunks_around(const Vector2i &p_center, int p_radius);
	Vector<Vector2i> _sorted_chunk_keys_by_distance(const Vector2i &p_center, int p_radius) const;
	Ref<ShaderMaterial> _get_voxel_shader_material(const Ref<Texture2D> &p_texture);
	Ref<StandardMaterial3D> _get_standard_texture_material(const Ref<Texture2D> &p_texture, bool p_alpha_scissor);
	void _ensure_voxel_global_shader_parameter(float p_value);

	// Block-emitting light nodes (OmniLight3D per emissive block in loaded chunks).
	HashMap<Vector3i, OmniLight3D *> block_lights;
	void _spawn_block_light(const Vector3i &p_block_pos, int p_type);
	void _remove_block_light(const Vector3i &p_block_pos);
	void _scan_chunk_for_lights(VoxelChunk *p_chunk);

	// Runtime model nodes for rare model-backed blocks. Not saved; rebuilt from block data.
	HashMap<Vector3i, Node3D *> block_model_nodes;
	Node3D *_spawn_block_model(const Vector3i &p_block_pos, int p_type);
	void _remove_block_model(const Vector3i &p_block_pos);
	void _scan_chunk_for_block_models(VoxelChunk *p_chunk);
	void _remove_block_models_in_chunk(const Vector2i &p_key);
	AnimationPlayer *_find_block_model_animation_player(Node *p_root) const;
	StringName _select_block_model_animation(const Vector3i &p_block_pos, int p_type) const;
	void _play_block_model_animation(const Vector3i &p_block_pos, int p_type);
	void _refresh_block_model_animation_at(const Vector3i &p_block_pos);
	void _refresh_block_model_animations_near_object(const WorldObjectEntry &p_object);

	// Background chunk generation
	void _initialize_world();
	void _cleanup_world();
	void _update_chunks(const Vector3 &p_camera_pos);
	void _request_chunk(int p_cx, int p_cz);
	int _effective_chunk_region_radius(int p_radius) const;
	void _unload_chunk(int p_cx, int p_cz);
	Error _save_chunk_blocks(const Vector2i &p_key, const Vector<uint16_t> &p_blocks);
	Error _save_chunk_if_dirty(const Vector2i &p_key, VoxelChunk *p_chunk, bool p_force = false);
	void _queue_chunk_save_snapshot(const Vector2i &p_key, VoxelChunk *p_chunk);
	Error _flush_dirty_saved_chunks(int p_max_chunks = -1);
	Error _load_world_save_metadata(const String &p_save_dir);
	Error _write_world_save_metadata();
	Dictionary _build_world_save_metadata(const String &p_display_name, int p_resolved_seed, const Dictionary &p_player_state, const Dictionary &p_character_state) const;

	// Helpers for coordinate conversion.
	Vector2i _world_to_chunk(const Vector3 &p_world_pos) const;
	Vector3i _world_to_local_block(const Vector3 &p_world_pos) const;
	Vector2i _block_to_chunk(const Vector3i &p_block_pos) const;

	// Sparse world object helpers.
	Dictionary _world_object_to_dictionary(const WorldObjectEntry &p_object) const;
	WorldObjectEntry _world_object_from_dictionary(const Dictionary &p_dict) const;
	int64_t _add_world_object_internal(const WorldObjectEntry &p_object, bool p_mark_dirty, bool p_emit_signal);
	bool _remove_world_object_internal(int64_t p_id, bool p_mark_dirty, bool p_emit_signal);
	void _index_world_object(const WorldObjectEntry &p_object);
	void _unindex_world_object(const WorldObjectEntry &p_object);
	Array _get_world_objects_array_for_chunk(const Vector2i &p_key) const;
	Error _save_chunk_objects(const Vector2i &p_key, const Array &p_objects);
	Error _save_chunk_objects_if_dirty(const Vector2i &p_key, bool p_force = false);
	void _queue_object_save_snapshot(const Vector2i &p_key);
	Error _flush_dirty_saved_object_chunks(int p_max_chunks = -1);
	bool _load_world_objects_for_chunk(const Vector2i &p_key);
	void _remove_world_objects_in_chunk(const Vector2i &p_key, bool p_emit_signal = false);

	// Deterministic structure placement.
	uint32_t _hash_structure_anchor(int p_structure_id, const Vector2i &p_anchor_key, uint32_t p_salt = 0) const;
	bool _structure_should_place(int p_structure_id, const Vector2i &p_anchor_key) const;
	int _select_structure_rotation(const VoxelStructureRegistry::StructureEntry &p_entry, int p_structure_id, const Vector2i &p_anchor_key) const;
	Vector3i _rotate_structure_local(const Vector3i &p_local, const Vector3i &p_size, int p_rotation) const;
	bool _apply_structures_to_chunk(const Vector2i &p_key, VoxelChunk *p_chunk, bool p_blocks_from_save);
	void _generate_structure_objects_for_chunk(const Vector2i &p_key);

	// Day/night cycle helpers.
	void _resolve_environment_nodes();
	void _update_day_night_cycle(float p_delta, float p_local_light = 1.0f);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_seed(int p_seed);
	int get_seed() const { return seed; }

	void set_chunk_load_radius(int p_radius);
	int get_chunk_load_radius() const { return chunk_load_radius; }

	void set_chunks_per_frame(int p_count);
	int get_chunks_per_frame() const { return chunks_per_frame; }

	void set_max_pending_chunk_tasks(int p_count);
	int get_max_pending_chunk_tasks() const { return max_pending_chunk_tasks; }

	void set_chunk_requests_per_frame(int p_count);
	int get_chunk_requests_per_frame() const { return chunk_requests_per_frame; }

	void set_remesh_requests_per_frame(int p_count);
	int get_remesh_requests_per_frame() const { return remesh_requests_per_frame; }

	void set_remeshes_per_frame(int p_count);
	int get_remeshes_per_frame() const { return remeshes_per_frame; }

	void set_chunk_integration_time_budget_usec(int p_usec);
	int get_chunk_integration_time_budget_usec() const { return chunk_integration_time_budget_usec; }

	void set_block_size(float p_size);
	float get_block_size() const { return block_size; }

	void set_sea_level(int p_level);
	int get_sea_level() const { return sea_level; }

	void set_texture_filter(BaseMaterial3D::TextureFilter p_filter);
	BaseMaterial3D::TextureFilter get_texture_filter() const { return texture_filter; }

	void set_block_registry(const Ref<VoxelBlockRegistry> &p_registry);
	Ref<VoxelBlockRegistry> get_block_registry() const;

	void set_biome_registry(const Ref<VoxelBiomeRegistry> &p_registry);
	Ref<VoxelBiomeRegistry> get_biome_registry() const;

	void set_structure_registry(const Ref<VoxelStructureRegistry> &p_registry);
	Ref<VoxelStructureRegistry> get_structure_registry() const;

	void set_verbose_logging(bool p_enabled) { verbose_logging = p_enabled; }
	bool get_verbose_logging() const { return verbose_logging; }

	// --- Chunk border debug visualization ---
	void set_show_chunk_borders(bool p_show);
	bool get_show_chunk_borders() const { return show_chunk_borders; }

	void set_chunk_border_color(const Color &p_color);
	Color get_chunk_border_color() const { return chunk_border_color; }

	// --- Day/Night Cycle ---
	void set_start_time_of_day(float p_time);
	float get_start_time_of_day() const { return start_time_of_day; }

	void set_time_of_day(float p_time);
	float get_time_of_day() const { return time_of_day; }

	void set_day_length_seconds(float p_seconds);
	float get_day_length_seconds() const { return day_length_seconds; }

	void set_auto_advance_time(bool p_enabled) { auto_advance_time = p_enabled; }
	bool get_auto_advance_time() const { return auto_advance_time; }

	void set_time_speed(float p_speed);
	float get_time_speed() const { return time_speed; }

	// Convenience API.
	void advance_time(float p_hours);
	bool is_daytime() const;
	bool is_nighttime() const;
	float get_day_factor() const;

	void set_fog_enabled(bool p_enabled) { fog_enabled = p_enabled; }
	bool get_fog_enabled() const { return fog_enabled; }

	void set_fog_distance_ratio(float p_ratio);
	float get_fog_distance_ratio() const { return fog_distance_ratio; }

	void set_sun_path(const NodePath &p_path);
	NodePath get_sun_path() const { return sun_path; }

	void set_moon_path(const NodePath &p_path);
	NodePath get_moon_path() const { return moon_path; }

	void set_environment_path(const NodePath &p_path);
	NodePath get_environment_path() const { return environment_path; }

	// --- Block interaction API (exposed to GDScript) ---

	// Get/set blocks by world position.
	int get_block_at(const Vector3 &p_world_pos) const;
	void set_block_at(const Vector3 &p_world_pos, int p_block_id);

	// Get block name by world position.
	String get_block_name_at(const Vector3 &p_world_pos) const;

	// Get biome index (dominant biome) at a world position. Returns -1 before world is initialized.
	int get_biome_at(const Vector3 &p_world_pos) const;
	// Get biome name at a world position (uses registry name if set, fallback otherwise).
	String get_biome_name_at(const Vector3 &p_world_pos) const;

	int64_t add_world_object(const StringName &p_type, const Vector3i &p_block_pos, float p_rotation_y = 0.0f, const Dictionary &p_state = Dictionary(), bool p_blocking = false);
	bool remove_world_object(int64_t p_id);
	Dictionary get_world_object(int64_t p_id) const;
	bool set_world_object_state(int64_t p_id, const Dictionary &p_state);
	bool set_world_object_blocking(int64_t p_id, bool p_blocking);
	Array get_world_objects_in_chunk(const Vector2i &p_chunk_pos) const;

	// Convert world position to block grid position.
	Vector3i world_to_block_pos(const Vector3 &p_world_pos) const;
	Vector3 block_to_world_pos(const Vector3i &p_block_pos) const;

	// Simple voxel raycast. Returns Dictionary with "position", "normal", "block_id",
	// "block_pos" keys, or empty Dictionary if nothing hit.
	Dictionary raycast_block(const Vector3 &p_origin, const Vector3 &p_direction, float p_max_distance = 10.0f) const;

	void request_chunks_around(const Vector3 &p_world_position, int p_radius = -1);
	Dictionary get_chunk_region_load_status(const Vector3 &p_world_position, int p_radius = -1);
	bool is_chunk_region_loaded(const Vector3 &p_world_position, int p_radius = -1);
	Dictionary get_debug_metrics();

	// Move an AABB body through the voxel world with collision.
	// Returns Dictionary: { "position": Vector3, "velocity": Vector3, "on_ground": bool, "in_water": bool }
	Dictionary move_body(const AABB &p_body, const Vector3 &p_velocity, float p_delta) const;

	// --- Persistent world save API ---
	Error create_world_save(const String &p_save_dir, const String &p_display_name, int p_save_seed = -1, const Dictionary &p_player_state = Dictionary(), const Dictionary &p_character_state = Dictionary());
	Error load_world_save(const String &p_save_dir);
	Error save_world_state(const Dictionary &p_player_state = Dictionary(), const Dictionary &p_character_state = Dictionary(), int p_max_dirty_chunks = -1);
	Error flush_world_save_dirty_chunks(int p_max_chunks = 1);
	int get_world_save_dirty_chunk_count() const { return (int)dirty_saved_chunks.size() + (int)pending_saved_chunk_blocks.size() + (int)dirty_saved_object_chunks.size() + (int)pending_saved_chunk_objects.size(); }
	Error close_world_save();
	bool is_world_save_loaded() const { return world_save_loaded; }
	Dictionary get_world_save_metadata() const { return world_save_metadata; }
	Dictionary get_world_save_player_state() const { return world_save_player_state; }

	VoxelWorld();
	~VoxelWorld();
};
