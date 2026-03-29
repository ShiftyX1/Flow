#pragma once

#include "voxel_chunk.h"
#include "voxel_terrain_generator.h"

#include "core/templates/hash_map.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/material.h"

class VoxelWorld : public Node3D {
	GDCLASS(VoxelWorld, Node3D);

	int seed = -1;
	int chunk_load_radius = 8;
	float block_size = 1.0f;
	int sea_level = 20;

	VoxelTerrainGenerator *generator = nullptr;
	HashMap<Vector2i, VoxelChunk *> loaded_chunks;
	Ref<StandardMaterial3D> material;

	bool initialized = false;
	Vector2i last_camera_chunk = Vector2i(INT32_MAX, INT32_MAX);

	void _initialize_world();
	void _cleanup_world();
	void _update_chunks(const Vector3 &p_camera_pos);
	void _load_chunk(int p_cx, int p_cz);
	void _unload_chunk(int p_cx, int p_cz);

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

	VoxelWorld();
	~VoxelWorld();
};
