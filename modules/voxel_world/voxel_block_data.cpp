#include "voxel_block_data.h"

const Color VoxelBlockData::block_colors[VOXEL_BLOCK_TYPE_MAX] = {
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
};
