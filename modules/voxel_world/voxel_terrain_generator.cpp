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

// ===========================================================================
// Biome parameter tables
// ===========================================================================

//                                                    height_base  height_scale  detail_scale  density_3d  surface             subsurface          tree_density  snow_line
const BiomeParams VoxelTerrainGenerator::BIOME_TABLE[BIOME_MAX] = {
	/* DESERT    */ { 0.0f, 8.0f, 1.0f, 3.0f, VOXEL_BLOCK_SAND, VOXEL_BLOCK_SAND, 0, 999 },
	/* MEADOW    */ { 0.0f, 15.0f, 3.0f, 5.0f, VOXEL_BLOCK_GRASS, VOXEL_BLOCK_DIRT, 120, 55 },
	/* FOREST    */ { 2.0f, 18.0f, 4.0f, 5.0f, VOXEL_BLOCK_GRASS, VOXEL_BLOCK_DIRT, 25, 55 },
	/* MOUNTAINS */ { 10.0f, 30.0f, 8.0f, 8.0f, VOXEL_BLOCK_STONE, VOXEL_BLOCK_STONE, 300, 40 },
};

// Biome centers in (temperature, humidity) space. Noise outputs are in [-1, 1].
const Vector2 VoxelTerrainGenerator::BIOME_CENTERS[BIOME_MAX] = {
	/* DESERT    */ Vector2(0.8f, -0.6f), // Hot, dry.
	/* MEADOW    */ Vector2(0.2f, 0.0f), // Moderate temp, moderate humidity.
	/* FOREST    */ Vector2(0.0f, 0.6f), // Cool, humid.
	/* MOUNTAINS */ Vector2(-0.6f, 0.0f), // Cold, any humidity.
};

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

	// --- River noise: 2D ridged noise for thin winding channels. ---
	river_noise.instantiate();
	river_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	river_noise->set_frequency(0.003f);
	river_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	river_noise->set_fractal_octaves(2);
	river_noise->set_fractal_lacunarity(2.0f);
	river_noise->set_fractal_gain(0.5f);

	// --- River warp noise: 2D domain-warp for natural river meandering. ---
	river_warp_noise.instantiate();
	river_warp_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	river_warp_noise->set_frequency(0.002f);
	river_warp_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	river_warp_noise->set_fractal_octaves(2);
	river_warp_noise->set_fractal_lacunarity(2.0f);
	river_warp_noise->set_fractal_gain(0.5f);

	// --- Lake noise: 2D noise for inland lake basins. ---
	lake_noise.instantiate();
	lake_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	lake_noise->set_frequency(0.004f);
	lake_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	lake_noise->set_fractal_octaves(3);
	lake_noise->set_fractal_lacunarity(2.0f);
	lake_noise->set_fractal_gain(0.5f);

	// --- Temperature noise: 2D, very low frequency for large biome regions. ---
	temperature_noise.instantiate();
	temperature_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	temperature_noise->set_frequency(0.001f);
	temperature_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	temperature_noise->set_fractal_octaves(3);
	temperature_noise->set_fractal_lacunarity(2.0f);
	temperature_noise->set_fractal_gain(0.5f);

	// --- Humidity noise: 2D, very low frequency for large biome regions. ---
	humidity_noise.instantiate();
	humidity_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	humidity_noise->set_frequency(0.001f);
	humidity_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	humidity_noise->set_fractal_octaves(3);
	humidity_noise->set_fractal_lacunarity(2.0f);
	humidity_noise->set_fractal_gain(0.5f);
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
	river_noise->set_seed(p_seed + 300);
	river_warp_noise->set_seed(p_seed + 400);
	lake_noise->set_seed(p_seed + 500);
	temperature_noise->set_seed(p_seed + 600);
	humidity_noise->set_seed(p_seed + 700);
}

// ===========================================================================
// Biome weight computation
// ===========================================================================

void VoxelTerrainGenerator::_get_biome_weights(int p_world_x, int p_world_z, float r_weights[BIOME_MAX]) const {
	real_t wx = (real_t)p_world_x;
	real_t wz = (real_t)p_world_z;

	float temp = temperature_noise->get_noise_2d(wx, wz);
	float humid = humidity_noise->get_noise_2d(wx, wz);

	float total = 0.0f;
	for (int i = 0; i < BIOME_MAX; i++) {
		float dt = temp - BIOME_CENTERS[i].x;
		float dh = humid - BIOME_CENTERS[i].y;
		float dist_sq = dt * dt + dh * dh;
		r_weights[i] = Math::exp(-4.0f * dist_sq);
		total += r_weights[i];
	}

	if (total > 0.0f) {
		float inv_total = 1.0f / total;
		for (int i = 0; i < BIOME_MAX; i++) {
			r_weights[i] *= inv_total;
		}
	} else {
		for (int i = 0; i < BIOME_MAX; i++) {
			r_weights[i] = 1.0f / (float)BIOME_MAX;
		}
	}
}

BiomeParams VoxelTerrainGenerator::_get_blended_params(const float p_weights[BIOME_MAX]) const {
	BiomeParams result = {};
	result.height_base = 0.0f;
	result.height_scale = 0.0f;
	result.detail_scale = 0.0f;
	result.density_3d_weight = 0.0f;
	float snow_line_f = 0.0f;
	float tree_density_f = 0.0f;

	int dominant = 0;
	for (int i = 0; i < BIOME_MAX; i++) {
		result.height_base += BIOME_TABLE[i].height_base * p_weights[i];
		result.height_scale += BIOME_TABLE[i].height_scale * p_weights[i];
		result.detail_scale += BIOME_TABLE[i].detail_scale * p_weights[i];
		result.density_3d_weight += BIOME_TABLE[i].density_3d_weight * p_weights[i];
		snow_line_f += (float)BIOME_TABLE[i].snow_line * p_weights[i];
		tree_density_f += (float)BIOME_TABLE[i].tree_density * p_weights[i];
		if (p_weights[i] > p_weights[dominant]) {
			dominant = i;
		}
	}

	result.surface_block = BIOME_TABLE[dominant].surface_block;
	result.subsurface_block = BIOME_TABLE[dominant].subsurface_block;
	result.snow_line = (int)snow_line_f;
	result.tree_density = (int)(tree_density_f + 0.5f);

	return result;
}

// ===========================================================================
// 3D Density-based terrain (Minecraft-style, biome-aware)
// ===========================================================================
//
// density(x,y,z) = depth_bias + 3D_noise
//   depth_bias  = (base_height - y) * SQUISH_FACTOR
//   3D_noise    = density_noise * biome_density_3d_weight + detail_noise * 1.5
//
// Biome parameters (height_scale, detail_scale, density_3d_weight) are
// smoothly interpolated across biome boundaries using temperature/humidity
// noise weights, producing organic transitions with no hard edges.
// ===========================================================================

float VoxelTerrainGenerator::_get_base_height(int p_world_x, int p_world_z, const BiomeParams &p_params) const {
	real_t wx = (real_t)p_world_x;
	real_t wz = (real_t)p_world_z;
	real_t c = continentalness_noise->get_noise_2d(wx, wz);
	real_t e = erosion_noise->get_noise_2d(wx, wz);
	float base_h = BASE_HEIGHT + p_params.height_base + c * p_params.height_scale + e * p_params.detail_scale;

	// Ocean: depress terrain below sea level for deeper ocean basins.
	if (c < OCEAN_THRESHOLD) {
		base_h -= (OCEAN_THRESHOLD - c) * OCEAN_DEPTH_SCALE;
	}

	return base_h;
}

int VoxelTerrainGenerator::_find_surface_y(int p_world_x, int p_world_z, const BiomeParams &p_params) const {
	float base_h = _get_base_height(p_world_x, p_world_z, p_params);
	for (int y = CHUNK_SIZE_Y - 1; y >= 1; y--) {
		float depth_bias = (base_h - (float)y) * SQUISH_FACTOR;
		float n3d = density_noise->get_noise_3d((real_t)p_world_x, (real_t)y, (real_t)p_world_z) * p_params.density_3d_weight;
		float n3d_detail = density_detail_noise->get_noise_3d((real_t)p_world_x, (real_t)y, (real_t)p_world_z) * 1.5f;
		if (depth_bias + n3d + n3d_detail > 0.0f) {
			return y;
		}
	}
	return 1;
}

// ===========================================================================
// Water feature helpers (rivers, lakes)
// ===========================================================================

// Smoothstep helper: maps t in [0,1] to smooth Hermite interpolation.
static _FORCE_INLINE_ float _smoothstep(float t) {
	t = CLAMP(t, 0.0f, 1.0f);
	return t * t * (3.0f - 2.0f * t);
}

float VoxelTerrainGenerator::_get_river_factor(int p_world_x, int p_world_z) const {
	real_t wx = (real_t)p_world_x;
	real_t wz = (real_t)p_world_z;

	// Domain-warp with two octaves for organic meanders.
	real_t warp_x = river_warp_noise->get_noise_2d(wx, wz) * 50.0f;
	real_t warp_z = river_warp_noise->get_noise_2d(wx + 1000.0f, wz + 1000.0f) * 50.0f;
	// Second octave at 2x frequency for finer bends.
	warp_x += river_warp_noise->get_noise_2d(wx * 2.0f + 500.0f, wz * 2.0f) * 20.0f;
	warp_z += river_warp_noise->get_noise_2d(wx * 2.0f + 1500.0f, wz * 2.0f + 500.0f) * 20.0f;

	// Ridged noise: abs(noise) near 0 → river center.
	real_t raw = river_noise->get_noise_2d(wx + warp_x, wz + warp_z);
	float ridge = Math::abs(raw);

	if (ridge < RIVER_WIDTH) {
		// Smoothstep for parabolic U-shaped cross-section (deeper center, shallow edges).
		return _smoothstep(1.0f - (ridge / RIVER_WIDTH));
	}
	return 0.0f;
}

float VoxelTerrainGenerator::_get_river_bank_factor(int p_world_x, int p_world_z) const {
	real_t wx = (real_t)p_world_x;
	real_t wz = (real_t)p_world_z;

	// Same warp as _get_river_factor to stay consistent.
	real_t warp_x = river_warp_noise->get_noise_2d(wx, wz) * 50.0f;
	real_t warp_z = river_warp_noise->get_noise_2d(wx + 1000.0f, wz + 1000.0f) * 50.0f;
	warp_x += river_warp_noise->get_noise_2d(wx * 2.0f + 500.0f, wz * 2.0f) * 20.0f;
	warp_z += river_warp_noise->get_noise_2d(wx * 2.0f + 1500.0f, wz * 2.0f + 500.0f) * 20.0f;

	real_t raw = river_noise->get_noise_2d(wx + warp_x, wz + warp_z);
	float ridge = Math::abs(raw);

	// Bank zone: between RIVER_WIDTH and RIVER_WIDTH + RIVER_BANK_WIDTH.
	if (ridge >= RIVER_WIDTH && ridge < RIVER_WIDTH + RIVER_BANK_WIDTH) {
		float t = (ridge - RIVER_WIDTH) / RIVER_BANK_WIDTH;
		return _smoothstep(1.0f - t); // 1.0 at river edge, 0.0 at outer bank.
	}
	return 0.0f;
}

float VoxelTerrainGenerator::_get_lake_factor(int p_world_x, int p_world_z) const {
	real_t wx = (real_t)p_world_x;
	real_t wz = (real_t)p_world_z;

	real_t val = lake_noise->get_noise_2d(wx, wz);
	if (val > LAKE_THRESHOLD) {
		return (val - LAKE_THRESHOLD) / (1.0f - LAKE_THRESHOLD); // Normalized 0→1.
	}
	return 0.0f;
}

int VoxelTerrainGenerator::_get_local_water_level(int p_world_x, int p_world_z, float p_base_height) const {
	int water_level = sea_level;

	float lake_f = _get_lake_factor(p_world_x, p_world_z);

	// Rivers: water surface is always at sea_level — guarantees flat,
	// continuous water across the entire river, connecting to oceans.
	// No per-column river water level — carving handles the channel shape.

	// Lakes: fill basin with water partway up.
	if (lake_f > 0.0f && p_base_height > (float)(sea_level + 5)) {
		int lake_surface = (int)(p_base_height - lake_f * LAKE_MAX_DEPTH * 0.3f);
		if (lake_surface > water_level) {
			water_level = lake_surface;
		}
	}

	return water_level;
}

// --- Tree helpers ---

bool VoxelTerrainGenerator::_should_place_tree(int p_world_x, int p_world_z, int p_tree_density) const {
	if (p_tree_density <= 0) {
		return false;
	}
	uint32_t h = _hash_u32((uint32_t)seed ^ ((uint32_t)p_world_x * 73856093U) ^ ((uint32_t)p_world_z * 19349663U));
	return ((int)(h % (uint32_t)p_tree_density)) == 0;
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

	// Per-column caches.
	BiomeParams blended[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float base_heights[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float original_base_heights[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float river_factors[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float river_bank_factors[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float lake_factors[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	int local_water_levels[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	int surface_y_cache[CHUNK_SIZE_X][CHUNK_SIZE_Z];

	// Pre-compute 2D per-column data: biomes, heights, water features.
	for (int x = 0; x < CHUNK_SIZE_X; x++) {
		for (int z = 0; z < CHUNK_SIZE_Z; z++) {
			int wx = world_x_start + x;
			int wz = world_z_start + z;

			// Biome weights and blended parameters.
			float weights[BIOME_MAX];
			_get_biome_weights(wx, wz, weights);
			blended[x][z] = _get_blended_params(weights);

			float bh = _get_base_height(wx, wz, blended[x][z]);
			original_base_heights[x][z] = bh;

			float rf = _get_river_factor(wx, wz);
			float rbf = _get_river_bank_factor(wx, wz);
			float lf = _get_lake_factor(wx, wz);
			river_factors[x][z] = rf;
			river_bank_factors[x][z] = rbf;
			lake_factors[x][z] = lf;

			// Carve rivers.
			float river_bed = (float)(sea_level - RIVER_BED_OFFSET);
			if (rf > 0.0f && bh > (float)(sea_level + RIVER_MIN_HEIGHT)) {
				bh = Math::lerp(bh, river_bed, rf);
			}

			// Bank blending.
			if (rbf > 0.0f && bh > (float)(sea_level + RIVER_MIN_HEIGHT)) {
				float bank_target = (float)(sea_level + 1);
				bh = Math::lerp(bh, bank_target, rbf * 0.5f);
			}

			// Carve lakes.
			if (lf > 0.0f && bh > (float)(sea_level + 5)) {
				bh -= lf * LAKE_MAX_DEPTH;
			}

			base_heights[x][z] = bh;
			local_water_levels[x][z] = _get_local_water_level(wx, wz, original_base_heights[x][z]);
			surface_y_cache[x][z] = 0;
		}
	}

	// --- Pass 1: 3D density terrain (biome-aware) ---
	for (int x = 0; x < CHUNK_SIZE_X; x++) {
		for (int z = 0; z < CHUNK_SIZE_Z; z++) {
			int wx = world_x_start + x;
			int wz = world_z_start + z;
			float bh = base_heights[x][z];
			int wl = local_water_levels[x][z];
			float d3d_weight = blended[x][z].density_3d_weight;

			for (int y = 0; y < CHUNK_SIZE_Y; y++) {
				int idx = block_index(x, y, z);

				if (y == 0) {
					blocks_w[idx] = VOXEL_BLOCK_BEDROCK;
					continue;
				}

				// Compute density: depth bias + biome-scaled 3D noise.
				float depth_bias = (bh - (float)y) * SQUISH_FACTOR;
				float n3d = density_noise->get_noise_3d((real_t)wx, (real_t)y, (real_t)wz) * d3d_weight;
				float n3d_detail = density_detail_noise->get_noise_3d((real_t)wx, (real_t)y, (real_t)wz) * 1.5f;
				float density = depth_bias + n3d + n3d_detail;

				if (density > 0.0f) {
					blocks_w[idx] = VOXEL_BLOCK_STONE;
					surface_y_cache[x][z] = y;
				} else if (y <= wl) {
					blocks_w[idx] = VOXEL_BLOCK_WATER;
				}
			}
		}
	}

	// --- Pass 2: Block type assignment (biome-aware) ---
	for (int x = 0; x < CHUNK_SIZE_X; x++) {
		for (int z = 0; z < CHUNK_SIZE_Z; z++) {
			int sh = surface_y_cache[x][z];
			int wl = local_water_levels[x][z];
			float rf = river_factors[x][z];
			float rbf = river_bank_factors[x][z];
			float lf = lake_factors[x][z];
			const BiomeParams &bp = blended[x][z];

			bool is_water_body = (rf > 0.0f && original_base_heights[x][z] > (float)(sea_level + RIVER_MIN_HEIGHT)) ||
					(rbf > 0.3f && original_base_heights[x][z] > (float)(sea_level + RIVER_MIN_HEIGHT)) ||
					(lf > 0.0f && original_base_heights[x][z] > (float)(sea_level + 5));

			for (int y = CHUNK_SIZE_Y - 1; y >= 1; y--) {
				int idx = block_index(x, y, z);
				uint8_t block = blocks_w[idx];

				if (block != VOXEL_BLOCK_STONE) {
					continue;
				}

				int depth = sh - y;

				if (depth == 0) {
					// Surface block — biome-dependent.
					if (is_water_body || sh <= wl) {
						blocks_w[idx] = VOXEL_BLOCK_SAND;
					} else if (sh <= wl + 2) {
						blocks_w[idx] = VOXEL_BLOCK_SAND; // Shore / beach.
					} else if (sh > bp.snow_line) {
						blocks_w[idx] = VOXEL_BLOCK_SNOW;
					} else {
						blocks_w[idx] = bp.surface_block;
					}
				} else if (depth <= 4) {
					// Subsurface — biome-dependent.
					if (is_water_body || sh <= wl + 2) {
						blocks_w[idx] = VOXEL_BLOCK_SAND;
					} else {
						blocks_w[idx] = bp.subsurface_block;
					}
				}
				// else: stays STONE.
			}
		}
	}

	// --- Pass 3: Spaghetti cave carving ---
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

	// --- Pass 4: Tree generation (biome-aware density) ---
	int border = TREE_CHECK_BORDER;
	for (int wx = world_x_start - border; wx < world_x_start + CHUNK_SIZE_X + border; wx++) {
		for (int wz = world_z_start - border; wz < world_z_start + CHUNK_SIZE_Z + border; wz++) {
			// Get biome tree density for this column.
			float weights[BIOME_MAX];
			_get_biome_weights(wx, wz, weights);
			BiomeParams bp = _get_blended_params(weights);

			if (!_should_place_tree(wx, wz, bp.tree_density)) {
				continue;
			}

			// Skip trees in water bodies and river banks.
			float rf = _get_river_factor(wx, wz);
			float rbf = _get_river_bank_factor(wx, wz);
			float lf = _get_lake_factor(wx, wz);
			if (rf > 0.0f || rbf > 0.0f || lf > 0.0f) {
				continue;
			}

			int local_x = wx - world_x_start;
			int local_z = wz - world_z_start;

			int sh;
			if (local_x >= 0 && local_x < CHUNK_SIZE_X && local_z >= 0 && local_z < CHUNK_SIZE_Z) {
				sh = surface_y_cache[local_x][local_z];
			} else {
				sh = _find_surface_y(wx, wz, bp);
			}

			// Only place trees above sea level + beach, below biome snow line.
			if (sh <= sea_level + 2 || sh > bp.snow_line) {
				continue;
			}

			int trunk_height = _tree_trunk_height(wx, wz);
			_place_tree(blocks_w, local_x, sh, local_z, trunk_height);
		}
	}

	return blocks;
}
