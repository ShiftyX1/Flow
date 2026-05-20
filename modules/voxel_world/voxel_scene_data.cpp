#include "voxel_scene_data.h"

#include "core/io/marshalls.h"
#include "core/object/class_db.h"

VoxelSceneData::VoxelSceneData() {
	_ensure_allocated();
}

void VoxelSceneData::_ensure_allocated() {
	int total = get_volume();
	if (blocks.size() != total) {
		blocks.resize(total);
		uint16_t *w = blocks.ptrw();
		for (int i = 0; i < total; i++) {
			w[i] = 0; // AIR.
		}
	}
}

void VoxelSceneData::set_size(const Vector3i &p_size) {
	Vector3i s(MAX(1, p_size.x), MAX(1, p_size.y), MAX(1, p_size.z));
	if (s == size) {
		return;
	}
	int new_total = s.x * s.y * s.z;
	if (new_total > LARGE_VOLUME_THRESHOLD) {
		WARN_PRINT(vformat("VoxelSceneData: size %s = %d voxels exceeds soft threshold (%d). Memory: ~%d KB.",
				String(s), new_total, LARGE_VOLUME_THRESHOLD, (new_total * 2) / 1024));
	}

	// Preserve overlapping cells.
	Vector<uint16_t> old_blocks = blocks;
	Vector3i old_size = size;
	size = s;
	blocks.resize(new_total);
	uint16_t *w = blocks.ptrw();
	for (int i = 0; i < new_total; i++) {
		w[i] = 0;
	}
	int copy_x = MIN(old_size.x, size.x);
	int copy_y = MIN(old_size.y, size.y);
	int copy_z = MIN(old_size.z, size.z);
	if (old_blocks.size() == old_size.x * old_size.y * old_size.z) {
		const uint16_t *r = old_blocks.ptr();
		for (int y = 0; y < copy_y; y++) {
			for (int z = 0; z < copy_z; z++) {
				for (int x = 0; x < copy_x; x++) {
					int oi = (y * old_size.z + z) * old_size.x + x;
					int ni = (y * size.z + z) * size.x + x;
					w[ni] = r[oi];
				}
			}
		}
	}
	emit_changed();
	notify_property_list_changed();
}

int VoxelSceneData::get_block(int x, int y, int z) const {
	if (!in_bounds(x, y, z)) {
		return 0;
	}
	return (int)blocks[index_of(x, y, z)];
}

void VoxelSceneData::set_block(int x, int y, int z, int p_id) {
	if (!in_bounds(x, y, z)) {
		return;
	}
	int idx = index_of(x, y, z);
	uint16_t v = (uint16_t)CLAMP(p_id, 0, 65535);
	if (blocks[idx] == v) {
		return;
	}
	blocks.write[idx] = v;
	emit_changed();
}

void VoxelSceneData::fill(int p_id) {
	uint16_t v = (uint16_t)CLAMP(p_id, 0, 65535);
	int total = get_volume();
	if (blocks.size() != total) {
		blocks.resize(total);
	}
	uint16_t *w = blocks.ptrw();
	for (int i = 0; i < total; i++) {
		w[i] = v;
	}
	emit_changed();
}

int VoxelSceneData::get_block_count_non_air() const {
	int count = 0;
	const uint16_t *r = blocks.ptr();
	int total = blocks.size();
	for (int i = 0; i < total; i++) {
		if (r[i] != 0) {
			count++;
		}
	}
	return count;
}

PackedByteArray VoxelSceneData::get_blocks_packed() const {
	PackedByteArray pba;
	int byte_size = blocks.size() * 2;
	pba.resize(byte_size);
	if (byte_size > 0) {
		memcpy(pba.ptrw(), blocks.ptr(), byte_size);
	}
	return pba;
}

void VoxelSceneData::set_blocks_packed(const PackedByteArray &p_data) {
	int expected = get_volume() * 2;
	if (p_data.size() != expected) {
		ERR_PRINT(vformat("VoxelSceneData::set_blocks_packed: data size %d != expected %d for size %s.",
				p_data.size(), expected, String(size)));
		return;
	}
	blocks.resize(get_volume());
	memcpy(blocks.ptrw(), p_data.ptr(), expected);
	emit_changed();
}

void VoxelSceneData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_size", "size"), &VoxelSceneData::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &VoxelSceneData::get_size);
	ClassDB::bind_method(D_METHOD("get_volume"), &VoxelSceneData::get_volume);

	ClassDB::bind_method(D_METHOD("get_block", "x", "y", "z"), &VoxelSceneData::get_block);
	ClassDB::bind_method(D_METHOD("get_block_v", "pos"), &VoxelSceneData::get_block_v);
	ClassDB::bind_method(D_METHOD("set_block", "x", "y", "z", "id"), &VoxelSceneData::set_block);
	ClassDB::bind_method(D_METHOD("set_block_v", "pos", "id"), &VoxelSceneData::set_block_v);

	ClassDB::bind_method(D_METHOD("fill", "id"), &VoxelSceneData::fill);
	ClassDB::bind_method(D_METHOD("clear"), &VoxelSceneData::clear);
	ClassDB::bind_method(D_METHOD("get_block_count_non_air"), &VoxelSceneData::get_block_count_non_air);

	ClassDB::bind_method(D_METHOD("get_blocks_packed"), &VoxelSceneData::get_blocks_packed);
	ClassDB::bind_method(D_METHOD("set_blocks_packed", "data"), &VoxelSceneData::set_blocks_packed);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3I, "size"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "blocks_packed", PROPERTY_HINT_NONE, "",
						 PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE),
			"set_blocks_packed", "get_blocks_packed");
}
