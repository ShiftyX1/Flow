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
	VOXEL_BLOCK_TORCH,
	VOXEL_BLOCK_TYPE_MAX
};

class VoxelBlockRegistry;

class VoxelBlockData {
public:
	// Mutable cache — populated by load_from_registry() at world init.
	static Color block_colors[VOXEL_BLOCK_TYPE_MAX];
	static const char *block_names[VOXEL_BLOCK_TYPE_MAX];
	static bool block_solid[VOXEL_BLOCK_TYPE_MAX];
	static bool block_transparent[VOXEL_BLOCK_TYPE_MAX];
	static uint8_t block_emission[VOXEL_BLOCK_TYPE_MAX];
	static uint8_t block_light_opacity[VOXEL_BLOCK_TYPE_MAX];
	static Color block_light_color[VOXEL_BLOCK_TYPE_MAX];

	static void load_from_registry(const VoxelBlockRegistry &p_registry);

	static _FORCE_INLINE_ bool is_solid(VoxelBlockType p_type) {
		return p_type < VOXEL_BLOCK_TYPE_MAX && block_solid[p_type];
	}

	static _FORCE_INLINE_ bool is_transparent(VoxelBlockType p_type) {
		return p_type >= VOXEL_BLOCK_TYPE_MAX || block_transparent[p_type];
	}

	static _FORCE_INLINE_ String get_block_name(VoxelBlockType p_type) {
		if (p_type < VOXEL_BLOCK_TYPE_MAX) {
			return String(block_names[p_type]);
		}
		return String("Unknown");
	}

	static _FORCE_INLINE_ uint8_t get_block_emission(VoxelBlockType p_type) {
		if (p_type < VOXEL_BLOCK_TYPE_MAX) {
			return block_emission[p_type];
		}
		return 0;
	}

	static _FORCE_INLINE_ uint8_t get_block_light_opacity(VoxelBlockType p_type) {
		if (p_type < VOXEL_BLOCK_TYPE_MAX) {
			return block_light_opacity[p_type];
		}
		return 1;
	}

	static _FORCE_INLINE_ Color get_block_light_color(VoxelBlockType p_type) {
		if (p_type < VOXEL_BLOCK_TYPE_MAX) {
			return block_light_color[p_type];
		}
		return Color(1, 1, 1);
	}
};
