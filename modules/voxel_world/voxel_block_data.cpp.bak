#include "voxel_block_data.h"

#include "voxel_block_registry.h"

// Mutable cache arrays — default values match setup_defaults() in VoxelBlockRegistry.
// Overwritten by load_from_registry() at world init.

Color VoxelBlockData::block_colors[VOXEL_BLOCK_TYPE_MAX] = {
	Color(0.0f, 0.0f, 0.0f, 0.0f), // AIR (transparent)
	Color(0.36f, 0.63f, 0.20f),     // GRASS (green)
	Color(0.55f, 0.37f, 0.24f),     // DIRT (brown)
	Color(0.50f, 0.50f, 0.50f),     // STONE (gray)
	Color(0.86f, 0.82f, 0.55f),     // SAND (yellow)
	Color(0.24f, 0.46f, 0.72f, 0.7f), // WATER (blue, semi-transparent)
	Color(0.93f, 0.93f, 0.97f),     // SNOW (white)
	Color(0.40f, 0.26f, 0.13f),     // WOOD (dark brown)
	Color(0.16f, 0.38f, 0.14f),     // LEAVES (dark green)
	Color(0.20f, 0.20f, 0.20f),     // BEDROCK (dark gray)
	Color(1.0f, 0.8f, 0.3f, 1.0f),  // TORCH (warm orange-yellow)
};

const char *VoxelBlockData::block_names[VOXEL_BLOCK_TYPE_MAX] = {
	"Air",
	"Grass",
	"Dirt",
	"Stone",
	"Sand",
	"Water",
	"Snow",
	"Wood",
	"Leaves",
	"Bedrock",
	"Torch",
};

bool VoxelBlockData::block_solid[VOXEL_BLOCK_TYPE_MAX] = {
	false, // AIR
	true,  // GRASS
	true,  // DIRT
	true,  // STONE
	true,  // SAND
	false, // WATER
	true,  // SNOW
	true,  // WOOD
	true,  // LEAVES
	true,  // BEDROCK
	false, // TORCH
};

bool VoxelBlockData::block_transparent[VOXEL_BLOCK_TYPE_MAX] = {
	true,  // AIR
	false, // GRASS
	false, // DIRT
	false, // STONE
	false, // SAND
	true,  // WATER
	false, // SNOW
	false, // WOOD
	false, // LEAVES
	false, // BEDROCK
	true,  // TORCH
};

uint8_t VoxelBlockData::block_emission[VOXEL_BLOCK_TYPE_MAX] = {
	0,  // AIR
	0,  // GRASS
	0,  // DIRT
	0,  // STONE
	0,  // SAND
	0,  // WATER
	0,  // SNOW
	0,  // WOOD
	0,  // LEAVES
	0,  // BEDROCK
	14, // TORCH
};

Color VoxelBlockData::block_light_color[VOXEL_BLOCK_TYPE_MAX] = {
	Color(1, 1, 1),            // AIR (unused)
	Color(1, 1, 1),            // GRASS
	Color(1, 1, 1),            // DIRT
	Color(1, 1, 1),            // STONE
	Color(1, 1, 1),            // SAND
	Color(1, 1, 1),            // WATER
	Color(1, 1, 1),            // SNOW
	Color(1, 1, 1),            // WOOD
	Color(1, 1, 1),            // LEAVES
	Color(1, 1, 1),            // BEDROCK
	Color(1.0f, 0.6f, 0.2f),   // TORCH (warm orange)
};

uint8_t VoxelBlockData::block_light_opacity[VOXEL_BLOCK_TYPE_MAX] = {
	0,  // AIR (fully transparent to light)
	15, // GRASS (fully opaque)
	15, // DIRT
	15, // STONE
	15, // SAND
	2,  // WATER (absorbs some light)
	15, // SNOW
	15, // WOOD
	1,  // LEAVES (slightly transparent)
	15, // BEDROCK
	0,  // TORCH (fully transparent to light)
};

void VoxelBlockData::load_from_registry(const VoxelBlockRegistry &p_registry) {
	const HashMap<int, VoxelBlockRegistry::BlockEntry> &map = p_registry.get_blocks_map();
	for (const KeyValue<int, VoxelBlockRegistry::BlockEntry> &E : map) {
		int id = E.key;
		if (id < 0 || id >= VOXEL_BLOCK_TYPE_MAX) {
			continue;
		}
		const VoxelBlockRegistry::BlockEntry &entry = E.value;
		block_colors[id] = entry.color;
		block_solid[id] = entry.solid;
		block_transparent[id] = entry.transparent;
		block_emission[id] = entry.emission;
		block_light_opacity[id] = entry.light_opacity;
		block_light_color[id] = entry.light_color;
	}
}
