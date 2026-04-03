#pragma once

#include "core/math/color.h"
#include "core/string/ustring.h"

enum VoxelBlockType : uint8_t {
	VOXEL_BLOCK_AIR = 0,
	VOXEL_BLOCK_GRASS,
	VOXEL_BLOCK_DIRT,
	VOXEL_BLOCK_STONE,
	VOXEL_BLOCK_SAND,
	VOXEL_BLOCK_WATER,
	VOXEL_BLOCK_SNOW,
	VOXEL_BLOCK_WOOD,
	VOXEL_BLOCK_LEAVES,
	VOXEL_BLOCK_BEDROCK,
	VOXEL_BLOCK_TYPE_MAX
};

class VoxelBlockData {
public:
	static const Color block_colors[VOXEL_BLOCK_TYPE_MAX];
	static const char *block_names[VOXEL_BLOCK_TYPE_MAX];

	static _FORCE_INLINE_ bool is_solid(VoxelBlockType p_type) {
		return p_type != VOXEL_BLOCK_AIR && p_type != VOXEL_BLOCK_WATER;
	}

	static _FORCE_INLINE_ bool is_transparent(VoxelBlockType p_type) {
		return p_type == VOXEL_BLOCK_AIR || p_type == VOXEL_BLOCK_WATER;
	}

	static _FORCE_INLINE_ String get_block_name(VoxelBlockType p_type) {
		if (p_type < VOXEL_BLOCK_TYPE_MAX) {
			return String(block_names[p_type]);
		}
		return String("Unknown");
	}
};
