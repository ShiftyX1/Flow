#pragma once

#include "voxel_block_registry.h"
#include "voxel_chunk.h"
#include "voxel_mesher.h"
#include "voxel_terrain_generator.h"

#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/material.h"

class VoxelWorld : public Node3D {
	GDCLASS(VoxelWorld, Node3D);

public:
	// Bitfield flags: which block types use alpha-transparency textures.
	enum AlphaBlockFlags {
		ALPHA_BLOCK_LEAVES = 1 << 0,
	};

private:
	int seed = -1;
	int chunk_load_radius = 8;
	float block_size = 1.0f;
	int sea_level = 20;
	int chunks_per_frame = 4; // Max chunks to integrate into scene tree per frame.
	uint32_t alpha_block_flags = 0; // Bitfield of AlphaBlockFlags.
	BaseMaterial3D::TextureFilter texture_filter = BaseMaterial3D::TEXTURE_FILTER_NEAREST;

	Ref<VoxelBlockRegistry> block_registry;

	VoxelTerrainGenerator *generator = nullptr;
	HashMap<Vector2i, VoxelChunk *> loaded_chunks;
	Ref<StandardMaterial3D> material;

	bool initialized = false;
	Vector2i last_camera_chunk = Vector2i(INT32_MAX, INT32_MAX);

	// --- Async chunk loading ---

	// Data produced by background thread (no Godot scene API).
	struct ChunkTaskResult {
		Vector2i key;
		Vector<uint8_t> blocks;
		Vector<VoxelMesher::MeshSurface> surfaces;
	};

	// Chunks currently being generated on background threads.
	HashMap<Vector2i, int64_t> pending_chunks; // key -> WorkerThreadPool TaskID
	// Finished results waiting to be integrated on the main thread.
	Mutex finished_mutex;
	Vector<ChunkTaskResult> finished_chunks;

	// Background task entry point (static, thread-safe).
	struct ChunkTaskData {
		VoxelWorld *world;
		Vector2i key;
		int chunk_x;
		int chunk_z;
	};
	static void _chunk_generation_task(void *p_userdata);

	// Integrate finished chunks into the scene tree (main thread only).
	void _integrate_finished_chunks();

	void _initialize_world();
	void _cleanup_world();
	void _update_chunks(const Vector3 &p_camera_pos);
	void _request_chunk(int p_cx, int p_cz);
	void _unload_chunk(int p_cx, int p_cz);

	// Helpers for coordinate conversion.
	Vector2i _world_to_chunk(const Vector3 &p_world_pos) const;
	Vector3i _world_to_local_block(const Vector3 &p_world_pos) const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_seed(int p_seed);
	int get_seed() const { return seed; }

	void set_chunk_load_radius(int p_radius);
	int get_chunk_load_radius() const { return chunk_load_radius; }

	void set_block_size(float p_size);
	float get_block_size() const { return block_size; }

	void set_sea_level(int p_level);
	int get_sea_level() const { return sea_level; }

	void set_texture_filter(BaseMaterial3D::TextureFilter p_filter);
	BaseMaterial3D::TextureFilter get_texture_filter() const { return texture_filter; }

	void set_alpha_block_flags(uint32_t p_flags);
	uint32_t get_alpha_block_flags() const { return alpha_block_flags; }

	void set_block_registry(const Ref<VoxelBlockRegistry> &p_registry);
	Ref<VoxelBlockRegistry> get_block_registry() const;

	// --- Block interaction API (exposed to GDScript) ---

	// Get/set blocks by world position.
	int get_block_at(const Vector3 &p_world_pos) const;
	void set_block_at(const Vector3 &p_world_pos, int p_block_id);

	// Get block name by world position.
	String get_block_name_at(const Vector3 &p_world_pos) const;

	// Convert world position to block grid position.
	Vector3i world_to_block_pos(const Vector3 &p_world_pos) const;
	Vector3 block_to_world_pos(const Vector3i &p_block_pos) const;

	// Simple voxel raycast. Returns Dictionary with "position", "normal", "block_id",
	// "block_pos" keys, or empty Dictionary if nothing hit.
	Dictionary raycast_block(const Vector3 &p_origin, const Vector3 &p_direction, float p_max_distance = 10.0f) const;

	VoxelWorld();
	~VoxelWorld();
};
