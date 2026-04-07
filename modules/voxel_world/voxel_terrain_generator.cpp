#include "voxel_terrain_generator.h"

#include "core/math/math_funcs.h"

// --- Fast deterministic hash (integer-only, no noise lookup). ---
static _FORCE_INLINE_ uint32_t _hash_u32(uint32_t x) {
	x ^= x >> 16;
	x *= 0x45d9f3bU;
	x ^= x >> 16;
	x *= 0x45d9f3bU;
	x ^= x >> 16;
	return x;
}

VoxelTerrainGenerator::VoxelTerrainGenerator() {
	// --- 2D continentalness: base terrain height (OpenSimplex2, low frequency). ---
	continentalness_noise.instantiate();
	continentalness_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	continentalness_noise->set_frequency(0.005f);
	continentalness_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	continentalness_noise->set_fractal_octaves(4);
	continentalness_noise->set_fractal_lacunarity(2.0f);
	continentalness_noise->set_fractal_gain(0.5f);

	// --- 2D erosion: terrain detail / roughness (OpenSimplex2, medium frequency). ---
	erosion_noise.instantiate();
	erosion_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	erosion_noise->set_frequency(0.02f);
	erosion_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	erosion_noise->set_fractal_octaves(3);
	erosion_noise->set_fractal_lacunarity(2.0f);
	erosion_noise->set_fractal_gain(0.5f);

	// --- 3D density: main terrain shape — overhangs, natural caves (OpenSimplex2). ---
	density_noise.instantiate();
	density_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	density_noise->set_frequency(0.015f);
	density_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	density_noise->set_fractal_octaves(3);
	density_noise->set_fractal_lacunarity(2.0f);
	density_noise->set_fractal_gain(0.5f);

	// --- 3D density detail: smaller-scale 3D features (OpenSimplex2, higher freq). ---
	density_detail_noise.instantiate();
	density_detail_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	density_detail_noise->set_frequency(0.04f);
	density_detail_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	density_detail_noise->set_fractal_octaves(2);
	density_detail_noise->set_fractal_lacunarity(2.0f);
	density_detail_noise->set_fractal_gain(0.5f);

	// --- Spaghetti cave noise A (OpenSimplex2, 3D). ---
	cave_noise_a.instantiate();
	cave_noise_a->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	cave_noise_a->set_frequency(0.03f);
	cave_noise_a->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	cave_noise_a->set_fractal_octaves(2);
	cave_noise_a->set_fractal_lacunarity(2.0f);
	cave_noise_a->set_fractal_gain(0.5f);

	// --- Spaghetti cave noise B (OpenSimplex2, 3D, offset frequency). ---
	cave_noise_b.instantiate();
	cave_noise_b->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	cave_noise_b->set_frequency(0.035f);
	cave_noise_b->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	cave_noise_b->set_fractal_octaves(2);
	cave_noise_b->set_fractal_lacunarity(2.0f);
	cave_noise_b->set_fractal_gain(0.5f);
}

VoxelTerrainGenerator::~VoxelTerrainGenerator() {
}

void VoxelTerrainGenerator::set_seed(int p_seed) {
	seed = p_seed;
	continentalness_noise->set_seed(p_seed);
	erosion_noise->set_seed(p_seed + 1);
	density_noise->set_seed(p_seed + 10);
	density_detail_noise->set_seed(p_seed + 20);
	cave_noise_a->set_seed(p_seed + 100);
	cave_noise_b->set_seed(p_seed + 200);
}

// ===========================================================================
// 3D Density-based terrain (Minecraft-style)
// ===========================================================================
//
// density(x,y,z) = depth_bias + 3D_noise
//   depth_bias  = (base_height - y) * SQUISH_FACTOR   (positive below base_height)
//   3D_noise    = density_noise * DENSITY_3D_WEIGHT + detail_noise * 1.5
//
// density > 0  => solid
// density <= 0 => air (or water if y <= sea_level)
//
// This naturally produces caves, overhangs, cliffs, and varied terrain.
// ===========================================================================

float VoxelTerrainGenerator::_get_base_height(int p_world_x, int p_world_z) const {
	real_t wx = (real_t)p_world_x;
	real_t wz = (real_t)p_world_z;
	real_t c = continentalness_noise->get_noise_2d(wx, wz);
	real_t e = erosion_noise->get_noise_2d(wx, wz);
	return BASE_HEIGHT + c * HEIGHT_SCALE + e * DETAIL_SCALE;
}

float VoxelTerrainGenerator::_get_density(int p_world_x, int p_world_y, int p_world_z) const {
	real_t wx = (real_t)p_world_x;
	real_t wy = (real_t)p_world_y;
	real_t wz = (real_t)p_world_z;

	float base_h = _get_base_height(p_world_x, p_world_z);
	float depth_bias = (base_h - (float)p_world_y) * SQUISH_FACTOR;

	float n3d = density_noise->get_noise_3d(wx, wy, wz) * DENSITY_3D_WEIGHT;
	float n3d_detail = density_detail_noise->get_noise_3d(wx, wy, wz) * 1.5f;

	return depth_bias + n3d + n3d_detail;
}

int VoxelTerrainGenerator::_find_surface_y(int p_world_x, int p_world_z) const {
	// Scan from top down to find the first solid block (density > 0).
	for (int y = CHUNK_SIZE_Y - 1; y >= 1; y--) {
		if (_get_density(p_world_x, y, p_world_z) > 0.0f) {
			return y;
		}
	}
	return 1;
}

// --- Tree helpers (unchanged) ---

bool VoxelTerrainGenerator::_should_place_tree(int p_world_x, int p_world_z) const {
	uint32_t h = _hash_u32((uint32_t)seed ^ ((uint32_t)p_world_x * 73856093U) ^ ((uint32_t)p_world_z * 19349663U));
	return (h % TREE_DENSITY) == 0;
}

int VoxelTerrainGenerator::_tree_trunk_height(int p_world_x, int p_world_z) const {
	uint32_t h = _hash_u32((uint32_t)(seed + 7777) ^ ((uint32_t)p_world_x * 83492791U) ^ ((uint32_t)p_world_z * 29560117U));
	return TREE_MIN_TRUNK + (int)(h % (TREE_MAX_TRUNK - TREE_MIN_TRUNK + 1));
}

void VoxelTerrainGenerator::_place_tree(uint8_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, int p_trunk_height) const {
	int trunk_top = p_surface_y + p_trunk_height;

	for (int y = p_surface_y + 1; y <= trunk_top; y++) {
		if (p_local_x >= 0 && p_local_x < CHUNK_SIZE_X &&
				p_local_z >= 0 && p_local_z < CHUNK_SIZE_Z &&
				y >= 0 && y < CHUNK_SIZE_Y) {
			p_blocks[block_index(p_local_x, y, p_local_z)] = VOXEL_BLOCK_WOOD;
		}
	}

	int canopy_center_y = trunk_top;
	int r = TREE_CANOPY_RADIUS;

	for (int dy = -1; dy <= r; dy++) {
		int layer_r = r;
		if (dy == -1 || dy == r) {
			layer_r = r - 1;
		}

		for (int dx = -layer_r; dx <= layer_r; dx++) {
			for (int dz = -layer_r; dz <= layer_r; dz++) {
				if (abs(dx) == layer_r && abs(dz) == layer_r) {
					continue;
				}

				int lx = p_local_x + dx;
				int ly = canopy_center_y + dy;
				int lz = p_local_z + dz;

				if (lx < 0 || lx >= CHUNK_SIZE_X || lz < 0 || lz >= CHUNK_SIZE_Z ||
						ly < 0 || ly >= CHUNK_SIZE_Y) {
					continue;
				}

				int idx = block_index(lx, ly, lz);
				if (p_blocks[idx] == VOXEL_BLOCK_AIR) {
					p_blocks[idx] = VOXEL_BLOCK_LEAVES;
				}
			}
		}
	}
}

// ===========================================================================
// Main chunk generation
// ===========================================================================

Vector<uint8_t> VoxelTerrainGenerator::generate_chunk_data(int p_chunk_x, int p_chunk_z) const {
	const int total = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
	Vector<uint8_t> blocks;
	blocks.resize(total);
	uint8_t *blocks_w = blocks.ptrw();
	memset(blocks_w, VOXEL_BLOCK_AIR, total);

	int world_x_start = p_chunk_x * CHUNK_SIZE_X;
	int world_z_start = p_chunk_z * CHUNK_SIZE_Z;

	// Per-column: cache base height and track actual surface Y for block-type assignment.
	float base_heights[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	int surface_y_cache[CHUNK_SIZE_X][CHUNK_SIZE_Z];

	// Pre-compute 2D base heights.
	for (int x = 0; x < CHUNK_SIZE_X; x++) {
		for (int z = 0; z < CHUNK_SIZE_Z; z++) {
			base_heights[x][z] = _get_base_height(world_x_start + x, world_z_start + z);
			surface_y_cache[x][z] = 0;
		}
	}

	// --- Pass 1: 3D density terrain ---
	for (int x = 0; x < CHUNK_SIZE_X; x++) {
		for (int z = 0; z < CHUNK_SIZE_Z; z++) {
			int wx = world_x_start + x;
			int wz = world_z_start + z;
			float bh = base_heights[x][z];

			for (int y = 0; y < CHUNK_SIZE_Y; y++) {
				int idx = block_index(x, y, z);

				if (y == 0) {
					blocks_w[idx] = VOXEL_BLOCK_BEDROCK;
					continue;
				}

				// Compute density: depth bias + 3D noise.
				float depth_bias = (bh - (float)y) * SQUISH_FACTOR;
				float n3d = density_noise->get_noise_3d((real_t)wx, (real_t)y, (real_t)wz) * DENSITY_3D_WEIGHT;
				float n3d_detail = density_detail_noise->get_noise_3d((real_t)wx, (real_t)y, (real_t)wz) * 1.5f;
				float density = depth_bias + n3d + n3d_detail;

				if (density > 0.0f) {
					// Solid — will assign block type in pass 2.
					blocks_w[idx] = VOXEL_BLOCK_STONE; // Placeholder, refined below.
					surface_y_cache[x][z] = y; // Track highest solid.
				} else if (y <= sea_level) {
					blocks_w[idx] = VOXEL_BLOCK_WATER;
				}
				// else: AIR (already memset).
			}
		}
	}

	// --- Pass 2: Block type assignment based on depth from surface ---
	for (int x = 0; x < CHUNK_SIZE_X; x++) {
		for (int z = 0; z < CHUNK_SIZE_Z; z++) {
			int sh = surface_y_cache[x][z];

			for (int y = CHUNK_SIZE_Y - 1; y >= 1; y--) {
				int idx = block_index(x, y, z);
				uint8_t block = blocks_w[idx];

				if (block != VOXEL_BLOCK_STONE) {
					continue; // Skip air, water, bedrock.
				}

				int depth = sh - y; // 0 = surface, 1 = one below, etc.

				if (depth == 0) {
					// Surface block.
					if (sh <= sea_level) {
						blocks_w[idx] = VOXEL_BLOCK_SAND;
					} else if (sh <= sea_level + 2) {
						blocks_w[idx] = VOXEL_BLOCK_SAND; // Beach.
					} else if (sh > 50) {
						blocks_w[idx] = VOXEL_BLOCK_SNOW;
					} else {
						blocks_w[idx] = VOXEL_BLOCK_GRASS;
					}
				} else if (depth <= 4) {
					// Below surface (1-4 blocks deep).
					if (sh <= sea_level + 2) {
						blocks_w[idx] = VOXEL_BLOCK_SAND;
					} else {
						blocks_w[idx] = VOXEL_BLOCK_DIRT;
					}
				}
				// else: stays STONE.
			}
		}
	}

	// --- Pass 3: Spaghetti cave carving (additional tunnel networks) ---
	for (int x = 0; x < CHUNK_SIZE_X; x++) {
		for (int z = 0; z < CHUNK_SIZE_Z; z++) {
			int wx = world_x_start + x;
			int wz = world_z_start + z;
			int sh = surface_y_cache[x][z];

			if (sh < 2) {
				continue;
			}

			for (int y = 1; y <= sh; y++) {
				int idx = block_index(x, y, z);
				uint8_t block = blocks_w[idx];

				if (block == VOXEL_BLOCK_AIR || block == VOXEL_BLOCK_WATER || block == VOXEL_BLOCK_BEDROCK) {
					continue;
				}

				float threshold = CAVE_THRESHOLD;
				if (y >= sh - 2) {
					threshold = CAVE_SURFACE_THRESHOLD;
				}

				real_t na = cave_noise_a->get_noise_3d((real_t)wx, (real_t)y, (real_t)wz);
				if (na < threshold) {
					continue;
				}

				real_t nb = cave_noise_b->get_noise_3d((real_t)wx, (real_t)y, (real_t)wz);
				if (nb >= threshold) {
					blocks_w[idx] = VOXEL_BLOCK_AIR;
				}
			}
		}
	}

	// --- Pass 4: Tree generation (with cross-chunk border scan) ---
	int border = TREE_CHECK_BORDER;
	for (int wx = world_x_start - border; wx < world_x_start + CHUNK_SIZE_X + border; wx++) {
		for (int wz = world_z_start - border; wz < world_z_start + CHUNK_SIZE_Z + border; wz++) {
			if (!_should_place_tree(wx, wz)) {
				continue;
			}

			int local_x = wx - world_x_start;
			int local_z = wz - world_z_start;

			// For border columns, find surface by scanning density.
			// For inner columns, use the cache.
			int sh;
			if (local_x >= 0 && local_x < CHUNK_SIZE_X && local_z >= 0 && local_z < CHUNK_SIZE_Z) {
				sh = surface_y_cache[local_x][local_z];
			} else {
				sh = _find_surface_y(wx, wz);
			}

			// Only place trees on grass (above sea level + beach, below snow).
			if (sh <= sea_level + 2 || sh > 50) {
				continue;
			}

			int trunk_height = _tree_trunk_height(wx, wz);
			_place_tree(blocks_w, local_x, sh, local_z, trunk_height);
		}
	}

	return blocks;
}
