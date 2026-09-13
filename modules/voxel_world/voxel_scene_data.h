#pragma once

#include "voxel_block_registry.h"

#include "core/io/resource.h"
#include "core/math/vector3i.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/typed_array.h"

// Storage of voxel block data for a finite voxel scene.
// Blocks are stored as uint16 IDs; ID 0 is always AIR (see VOXEL_BLOCK_AIR).
//
// Default size is 64x64x64 (set in the constructor). resize() preserves overlapping
// blocks and fills new cells with air.
class VoxelSceneData : public Resource {
	GDCLASS(VoxelSceneData, Resource);

public:
	static constexpr int DEFAULT_SIZE = 64;
	// Soft warn threshold (~1M voxels). Larger volumes are allowed but log a warning.
	static constexpr int LARGE_VOLUME_THRESHOLD = 1000000;

private:
	Vector3i size = Vector3i(DEFAULT_SIZE, DEFAULT_SIZE, DEFAULT_SIZE);
	Vector<uint16_t> blocks; // length = size.x * size.y * size.z

	void _ensure_allocated();

protected:
	static void _bind_methods();

public:
	void set_size(const Vector3i &p_size);
	Vector3i get_size() const { return size; }

	int get_volume() const { return size.x * size.y * size.z; }

	bool in_bounds(int x, int y, int z) const {
		return x >= 0 && y >= 0 && z >= 0 && x < size.x && y < size.y && z < size.z;
	}
	bool in_bounds_v(const Vector3i &p_pos) const { return in_bounds(p_pos.x, p_pos.y, p_pos.z); }

	_FORCE_INLINE_ int index_of(int x, int y, int z) const {
		return (y * size.z + z) * size.x + x;
	}

	int get_block(int x, int y, int z) const;
	int get_block_v(const Vector3i &p_pos) const { return get_block(p_pos.x, p_pos.y, p_pos.z); }
	void set_block(int x, int y, int z, int p_id);
	void set_block_v(const Vector3i &p_pos, int p_id) { set_block(p_pos.x, p_pos.y, p_pos.z, p_id); }

	// Direct access for the mesher.
	const Vector<uint16_t> &get_blocks_array() const { return blocks; }
	Vector<uint16_t> &get_blocks_array_mutable() { return blocks; }

	// Bulk operations.
	void fill(int p_id);
	void clear() { fill(0); }
	int get_block_count_non_air() const;

	// Serialization helpers (used by editor undo/redo for box ops, and by .voxscene saver).
	PackedByteArray get_blocks_packed() const;
	void set_blocks_packed(const PackedByteArray &p_data);

	VoxelSceneData();
};
