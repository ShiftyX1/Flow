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
// Biome parameter tables — populated at runtime via set_biome_data().
// ===========================================================================

// (Removed static BIOME_TABLE and BIOME_CENTERS — now in runtime_biomes.)

VoxelTerrainGenerator::VoxelTerrainGenerator() {
	// Populate default biomes (will be overwritten if VoxelBiomeRegistry is provided).
	setup_default_biomes();

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

	// --- 3D density: main terrain shape вЂ” overhangs, natural caves (OpenSimplex2). ---
	density_noise.instantiate();
	density_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	density_noise->set_frequency(0.008f);
	density_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	density_noise->set_fractal_octaves(3);
	density_noise->set_fractal_lacunarity(2.0f);
	density_noise->set_fractal_gain(0.5f);

	// --- 3D density detail: smaller-scale 3D features (OpenSimplex2, higher freq). ---
	density_detail_noise.instantiate();
	density_detail_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	density_detail_noise->set_frequency(0.02f);
	density_detail_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	density_detail_noise->set_fractal_octaves(2);
	density_detail_noise->set_fractal_lacunarity(2.0f);
	density_detail_noise->set_fractal_gain(0.5f);

	// --- Spaghetti cave noise A (OpenSimplex2, 3D). ---
	cave_noise_a.instantiate();
	cave_noise_a->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	cave_noise_a->set_frequency(0.015f);
	cave_noise_a->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	cave_noise_a->set_fractal_octaves(2);
	cave_noise_a->set_fractal_lacunarity(2.0f);
	cave_noise_a->set_fractal_gain(0.5f);

	// --- Spaghetti cave noise B (OpenSimplex2, 3D, offset frequency). ---
	cave_noise_b.instantiate();
	cave_noise_b->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	cave_noise_b->set_frequency(0.018f);
	cave_noise_b->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	cave_noise_b->set_fractal_octaves(2);
	cave_noise_b->set_fractal_lacunarity(2.0f);
	cave_noise_b->set_fractal_gain(0.5f);

	// --- Cheese cave noise (OpenSimplex2, 3D, very low frequency вЂ” large open chambers). ---
	cave_cheese_noise.instantiate();
	cave_cheese_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX_SMOOTH);
	cave_cheese_noise->set_frequency(0.007f);
	cave_cheese_noise->set_fractal_type(FastNoiseLite::FRACTAL_NONE);
	cave_cheese_noise->set_fractal_octaves(1);

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
	cave_cheese_noise->set_seed(p_seed + 250);
	river_noise->set_seed(p_seed + 300);
	river_warp_noise->set_seed(p_seed + 400);
	lake_noise->set_seed(p_seed + 500);
	temperature_noise->set_seed(p_seed + 600);
	humidity_noise->set_seed(p_seed + 700);
}

int VoxelTerrainGenerator::get_biome_index_at(int p_world_x, int p_world_z) const {
	const int count = _get_biome_count();
	float weights[MAX_BIOMES] = {};
	_get_biome_weights(p_world_x, p_world_z, weights, count);
	int dominant = 0;
	for (int i = 1; i < count; i++) {
		if (weights[i] > weights[dominant]) {
			dominant = i;
		}
	}
	return dominant;
}

// ===========================================================================
// Biome data access helpers
// ===========================================================================

int VoxelTerrainGenerator::_get_biome_count() const {
	return runtime_biomes.size();
}

BiomeParams VoxelTerrainGenerator::_get_biome_params_at(int p_index) const {
	if (p_index >= 0 && p_index < runtime_biomes.size()) {
		return runtime_biomes[p_index].params;
	}
	return BiomeParams();
}

Vector2 VoxelTerrainGenerator::_get_biome_center_at(int p_index) const {
	if (p_index >= 0 && p_index < runtime_biomes.size()) {
		return runtime_biomes[p_index].center;
	}
	return Vector2();
}

// ===========================================================================
// set_biome_data / setup_default_biomes
// ===========================================================================

void VoxelTerrainGenerator::set_biome_data(const Vector<RuntimeBiomeData> &p_biomes) {
	runtime_biomes = p_biomes;
}

void VoxelTerrainGenerator::setup_default_biomes() {
	runtime_biomes.clear();

	// Single default biome: Meadow.
	{
		RuntimeBiomeData bd;
		bd.params.height_base = 0.0f;
		bd.params.height_scale = 15.0f;
		bd.params.detail_scale = 3.0f;
		bd.params.density_3d_weight = 0.5f;
		bd.params.surface_block = VOXEL_BLOCK_GRASS;
		bd.params.subsurface_block = VOXEL_BLOCK_DIRT;
		bd.params.shore_block = VOXEL_BLOCK_SAND;
		bd.params.snow_block = VOXEL_BLOCK_SNOW;
		bd.params.snow_line = 165;
		bd.center = Vector2(0.0f, 0.0f);

		VoxelBiomeRegistry::FeatureConfig tree;
		tree.type = VoxelBiomeRegistry::FeatureConfig::FEATURE_TREE;
		tree.trunk_block = VOXEL_BLOCK_WOOD;
		tree.canopy_block = VOXEL_BLOCK_LEAVES;
		tree.density = 400;
		tree.canopy_shape = VoxelBiomeRegistry::CANOPY_SPHERE;
		bd.params.features.push_back(tree);

		runtime_biomes.push_back(bd);
	}
}

// ===========================================================================
// Biome weight computation
// ===========================================================================

void VoxelTerrainGenerator::_get_biome_weights(int p_world_x, int p_world_z, float *r_weights, int p_biome_count) const {
	real_t wx = (real_t)p_world_x;
	real_t wz = (real_t)p_world_z;

	float temp = temperature_noise->get_noise_2d(wx, wz);
	float humid = humidity_noise->get_noise_2d(wx, wz);

	float total = 0.0f;
	for (int i = 0; i < p_biome_count; i++) {
		Vector2 center = _get_biome_center_at(i);
		float dt = temp - center.x;
		float dh = humid - center.y;
		float dist_sq = dt * dt + dh * dh;
		r_weights[i] = Math::exp(-4.0f * dist_sq);
		total += r_weights[i];
	}

	if (total > 0.0f) {
		float inv_total = 1.0f / total;
		for (int i = 0; i < p_biome_count; i++) {
			r_weights[i] *= inv_total;
		}
	} else {
		for (int i = 0; i < p_biome_count; i++) {
			r_weights[i] = 1.0f / (float)p_biome_count;
		}
	}
}

BiomeParams VoxelTerrainGenerator::_get_blended_params(const float *p_weights, int p_biome_count) const {
	BiomeParams result = {};
	result.height_base = 0.0f;
	result.height_scale = 0.0f;
	result.detail_scale = 0.0f;
	result.density_3d_weight = 0.0f;
	float snow_line_f = 0.0f;

	int dominant = 0;
	for (int i = 0; i < p_biome_count; i++) {
		BiomeParams bp = _get_biome_params_at(i);
		result.height_base += bp.height_base * p_weights[i];
		result.height_scale += bp.height_scale * p_weights[i];
		result.detail_scale += bp.detail_scale * p_weights[i];
		result.density_3d_weight += bp.density_3d_weight * p_weights[i];
		snow_line_f += (float)bp.snow_line * p_weights[i];
		if (p_weights[i] > p_weights[dominant]) {
			dominant = i;
		}
	}

	BiomeParams dominant_bp = _get_biome_params_at(dominant);
	result.surface_block = dominant_bp.surface_block;
	result.subsurface_block = dominant_bp.subsurface_block;
	result.shore_block = dominant_bp.shore_block;
	result.snow_block = dominant_bp.snow_block;
	result.snow_line = (int)snow_line_f;
	result.features = dominant_bp.features;

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
		float n3d_detail = density_detail_noise->get_noise_3d((real_t)p_world_x, (real_t)y, (real_t)p_world_z) * 0.4f;
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

	// Ridged noise: abs(noise) near 0 в†’ river center.
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
		return (val - LAKE_THRESHOLD) / (1.0f - LAKE_THRESHOLD); // Normalized 0в†’1.
	}
	return 0.0f;
}

float VoxelTerrainGenerator::_get_lake_bank_factor(int p_world_x, int p_world_z) const {
	real_t wx = (real_t)p_world_x;
	real_t wz = (real_t)p_world_z;

	real_t val = lake_noise->get_noise_2d(wx, wz);

	// Bank zone: where noise is just below LAKE_THRESHOLD (approaching lake).
	if (val > LAKE_THRESHOLD - LAKE_BANK_WIDTH && val <= LAKE_THRESHOLD) {
		float t = (LAKE_THRESHOLD - val) / LAKE_BANK_WIDTH;
		return _smoothstep(1.0f - t); // 1.0 at lake edge, 0.0 at outer bank.
	}
	return 0.0f;
}

int VoxelTerrainGenerator::_get_local_water_level(int p_world_x, int p_world_z) const {
	// Always return sea_level: lakes are carved below sea_level and water
	// fills naturally from below. This eliminates cross-chunk water walls.
	return sea_level;
}

// --- Tree helpers ---

bool VoxelTerrainGenerator::_should_place_feature(int p_world_x, int p_world_z, int p_density, int p_feature_salt) const {
	if (p_density <= 0) {
		return false;
	}
	uint32_t h = _hash_u32((uint32_t)(seed + p_feature_salt) ^ ((uint32_t)p_world_x * 73856093U) ^ ((uint32_t)p_world_z * 19349663U));
	return ((int)(h % (uint32_t)p_density)) == 0;
}

int VoxelTerrainGenerator::_tree_trunk_height(int p_world_x, int p_world_z, int p_min_trunk, int p_max_trunk) const {
	uint32_t h = _hash_u32((uint32_t)(seed + 7777) ^ ((uint32_t)p_world_x * 83492791U) ^ ((uint32_t)p_world_z * 29560117U));
	int range = p_max_trunk - p_min_trunk + 1;
	if (range <= 0) {
		return p_min_trunk;
	}
	return p_min_trunk + (int)(h % (uint32_t)range);
}

void VoxelTerrainGenerator::_place_tree_sphere(uint16_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, int p_trunk_height, int p_canopy_radius, uint16_t p_trunk_block, uint16_t p_canopy_block) const {
	int trunk_top = p_surface_y + p_trunk_height;

	// Trunk.
	for (int y = p_surface_y + 1; y <= trunk_top; y++) {
		if (p_local_x >= 0 && p_local_x < CHUNK_SIZE_X &&
				p_local_z >= 0 && p_local_z < CHUNK_SIZE_Z &&
				y >= 0 && y < CHUNK_SIZE_Y) {
			p_blocks[block_index(p_local_x, y, p_local_z)] = p_trunk_block;
		}
	}

	// Canopy: sphere with clipped corners.
	int canopy_center_y = trunk_top;
	int r = p_canopy_radius;

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
					p_blocks[idx] = p_canopy_block;
				}
			}
		}
	}
}

void VoxelTerrainGenerator::_place_tree_cone(uint16_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, int p_trunk_height, int p_canopy_radius, uint16_t p_trunk_block, uint16_t p_canopy_block) const {
	int trunk_top = p_surface_y + p_trunk_height;

	// Trunk.
	for (int y = p_surface_y + 1; y <= trunk_top; y++) {
		if (p_local_x >= 0 && p_local_x < CHUNK_SIZE_X &&
				p_local_z >= 0 && p_local_z < CHUNK_SIZE_Z &&
				y >= 0 && y < CHUNK_SIZE_Y) {
			p_blocks[block_index(p_local_x, y, p_local_z)] = p_trunk_block;
		}
	}

	// Cone canopy: widest at bottom, tapers to top.
	int canopy_height = p_canopy_radius * 2 + 1;
	int canopy_start = trunk_top - 1;

	for (int dy = 0; dy < canopy_height; dy++) {
		int ly = canopy_start + dy;
		if (ly < 0 || ly >= CHUNK_SIZE_Y) {
			continue;
		}

		// Layer radius tapers linearly from max at bottom to 0 at top.
		int layer_r = p_canopy_radius - (dy * p_canopy_radius / canopy_height);
		if (layer_r < 0) {
			layer_r = 0;
		}

		for (int dx = -layer_r; dx <= layer_r; dx++) {
			for (int dz = -layer_r; dz <= layer_r; dz++) {
				// Skip corners for more natural shape.
				if (abs(dx) == layer_r && abs(dz) == layer_r && layer_r > 1) {
					continue;
				}

				int lx = p_local_x + dx;
				int lz = p_local_z + dz;

				if (lx < 0 || lx >= CHUNK_SIZE_X || lz < 0 || lz >= CHUNK_SIZE_Z) {
					continue;
				}

				int idx = block_index(lx, ly, lz);
				if (p_blocks[idx] == VOXEL_BLOCK_AIR) {
					p_blocks[idx] = p_canopy_block;
				}
			}
		}
	}

	// Tip: single block on top.
	int tip_y = canopy_start + canopy_height;
	if (p_local_x >= 0 && p_local_x < CHUNK_SIZE_X &&
			p_local_z >= 0 && p_local_z < CHUNK_SIZE_Z &&
			tip_y >= 0 && tip_y < CHUNK_SIZE_Y) {
		int idx = block_index(p_local_x, tip_y, p_local_z);
		if (p_blocks[idx] == VOXEL_BLOCK_AIR) {
			p_blocks[idx] = p_canopy_block;
		}
	}
}

void VoxelTerrainGenerator::_place_tree_bush(uint16_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, int p_trunk_height, int p_canopy_radius, uint16_t p_trunk_block, uint16_t p_canopy_block) const {
	// Bush: very short trunk (1 block hidden inside canopy), wide canopy.
	int trunk_top = p_surface_y + 1;

	// Minimal trunk.
	if (p_local_x >= 0 && p_local_x < CHUNK_SIZE_X &&
			p_local_z >= 0 && p_local_z < CHUNK_SIZE_Z &&
			trunk_top >= 0 && trunk_top < CHUNK_SIZE_Y) {
		p_blocks[block_index(p_local_x, trunk_top, p_local_z)] = p_trunk_block;
	}

	// Wide, short canopy: 2 layers tall, radius from feature.
	int r = p_canopy_radius;
	for (int dy = 0; dy <= 1; dy++) {
		int ly = trunk_top + dy;
		if (ly < 0 || ly >= CHUNK_SIZE_Y) {
			continue;
		}

		int layer_r = (dy == 0) ? r : r - 1;

		for (int dx = -layer_r; dx <= layer_r; dx++) {
			for (int dz = -layer_r; dz <= layer_r; dz++) {
				if (abs(dx) == layer_r && abs(dz) == layer_r) {
					continue;
				}

				int lx = p_local_x + dx;
				int lz = p_local_z + dz;

				if (lx < 0 || lx >= CHUNK_SIZE_X || lz < 0 || lz >= CHUNK_SIZE_Z) {
					continue;
				}

				int idx = block_index(lx, ly, lz);
				if (p_blocks[idx] == VOXEL_BLOCK_AIR) {
					p_blocks[idx] = p_canopy_block;
				}
			}
		}
	}
}

void VoxelTerrainGenerator::_place_scatter(uint16_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, uint16_t p_block) const {
	int y = p_surface_y + 1;
	if (p_local_x >= 0 && p_local_x < CHUNK_SIZE_X &&
			p_local_z >= 0 && p_local_z < CHUNK_SIZE_Z &&
			y >= 0 && y < CHUNK_SIZE_Y) {
		int idx = block_index(p_local_x, y, p_local_z);
		if (p_blocks[idx] == VOXEL_BLOCK_AIR) {
			p_blocks[idx] = p_block;
		}
	}
}

// ===========================================================================
// Main chunk generation
// ===========================================================================

Vector<uint16_t> VoxelTerrainGenerator::generate_chunk_data(int p_chunk_x, int p_chunk_z) const {
	const int total = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
	Vector<uint16_t> blocks;
	blocks.resize(total);
	uint16_t *blocks_w = blocks.ptrw();
	memset(blocks_w, 0, total * sizeof(uint16_t));

	const int biome_count = _get_biome_count();

	int world_x_start = p_chunk_x * CHUNK_SIZE_X;
	int world_z_start = p_chunk_z * CHUNK_SIZE_Z;

	// Per-column caches.
	BiomeParams blended[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float base_heights[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float original_base_heights[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float river_factors[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float river_bank_factors[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float lake_factors[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	float lake_bank_factors[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	int local_water_levels[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	int surface_y_cache[CHUNK_SIZE_X][CHUNK_SIZE_Z];
	int slope_cache[CHUNK_SIZE_X][CHUNK_SIZE_Z]; // Max height diff to axis-aligned in-chunk neighbors.

	// Pre-compute 2D per-column data: biomes, heights, water features.
	for (int x = 0; x < CHUNK_SIZE_X; x++) {
		for (int z = 0; z < CHUNK_SIZE_Z; z++) {
			int wx = world_x_start + x;
			int wz = world_z_start + z;

			// Biome weights and blended parameters.
			float weights[MAX_BIOMES] = {};
			_get_biome_weights(wx, wz, weights, biome_count);
			blended[x][z] = _get_blended_params(weights, biome_count);

			// Dithered surface block: probabilistic selection by biome weights
			// eliminates the hard colour snap at biome borders.
			// Only applied in genuine transition zones (dominant weight < BIOME_DITHER_THRESHOLD)
			// to prevent scattered foreign blocks (sand/stone stippling) deep inside a biome.
			{
				int dominant_bi = 0;
				for (int bi = 1; bi < biome_count; bi++) {
					if (weights[bi] > weights[dominant_bi]) {
						dominant_bi = bi;
					}
				}
				if (weights[dominant_bi] < BIOME_DITHER_THRESHOLD) {
					uint32_t bseed = _hash_u32((uint32_t)seed ^ ((uint32_t)wx * 374761393U) ^ ((uint32_t)wz * 668265263U));
					float r = (float)(bseed & 0xFFFFu) * (1.0f / 65536.0f);
					float cumul = 0.0f;
					for (int bi = 0; bi < biome_count; bi++) {
						cumul += weights[bi];
						if (r < cumul) {
							BiomeParams bp_bi = _get_biome_params_at(bi);
							blended[x][z].surface_block = bp_bi.surface_block;
							blended[x][z].subsurface_block = bp_bi.subsurface_block;
							break;
						}
					}
				}
			}

			float bh = _get_base_height(wx, wz, blended[x][z]);
			original_base_heights[x][z] = bh;

			float rf = _get_river_factor(wx, wz);
			float rbf = _get_river_bank_factor(wx, wz);
			float lf = _get_lake_factor(wx, wz);
			float lbf = _get_lake_bank_factor(wx, wz);
			river_factors[x][z] = rf;
			river_bank_factors[x][z] = rbf;
			lake_factors[x][z] = lf;
			lake_bank_factors[x][z] = lbf;

			// Compute lake water level early (needed for bank blending).
			int wl = _get_local_water_level(wx, wz);
			local_water_levels[x][z] = wl;

			float river_bed = (float)(sea_level - RIVER_BED_OFFSET);
			float river_bank_target = (float)(sea_level + 1);

			// River bank blending FIRST. Inside the river zone (rf > 0) treat the bank
			// factor as fully applied (1.0) so the bankв†’river transition is spatially
			// continuous вЂ” prevents the "wall" that forms when the bank effect vanishes
			// exactly where the river factor starts from zero.
			float effective_rbf = (rf > 0.0f) ? 1.0f : rbf;
			if (effective_rbf > 0.0f && bh > (float)(sea_level + RIVER_MIN_HEIGHT)) {
				bh = Math::lerp(bh, river_bank_target, effective_rbf * 0.85f);
			}

			// Carve river channel on top of the bank-modified height.
			if (rf > 0.0f && bh > (float)(sea_level + RIVER_MIN_HEIGHT)) {
				bh = Math::lerp(bh, river_bed, rf);
			}

			// Lake bank blending FIRST (same continuity fix as rivers).
			float effective_lbf = (lf > 0.0f) ? 1.0f : lbf;
			if (effective_lbf > 0.0f && bh > (float)(wl + 1)) {
				float lake_bank_target = (float)(wl + 1);
				bh = Math::lerp(bh, lake_bank_target, effective_lbf * 0.8f);
			}

			// Carve lakes: push terrain below sea_level at the basin centre
			// so water fills naturally at sea_level regardless of terrain altitude.
			if (lf > 0.0f) {
				float excess = MAX(bh - (float)sea_level + 3.0f, 0.0f);
				bh -= lf * (LAKE_MAX_DEPTH + excess);
			}

			base_heights[x][z] = bh;
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
				float n3d_detail = density_detail_noise->get_noise_3d((real_t)wx, (real_t)y, (real_t)wz) * 0.4f;
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

	// --- Slope pre-computation ---
	// For each column compute the steepest drop to any axis-aligned in-chunk
	// neighbor. Out-of-chunk neighbors are skipped (treated as neutral) to
	// avoid extra noise queries; this causes at most a 1-block artefact at
	// chunk borders which is not visible in practice.
	static const int _slope_dx[4] = { -1, 1, 0, 0 };
	static const int _slope_dz[4] = { 0, 0, -1, 1 };
	for (int x = 0; x < CHUNK_SIZE_X; x++) {
		for (int z = 0; z < CHUNK_SIZE_Z; z++) {
			int sh = surface_y_cache[x][z];
			int max_diff = 0;
			for (int d = 0; d < 4; d++) {
				int nx = x + _slope_dx[d];
				int nz = z + _slope_dz[d];
				if (nx < 0 || nx >= CHUNK_SIZE_X || nz < 0 || nz >= CHUNK_SIZE_Z) {
					continue;
				}
				int diff = Math::abs(surface_y_cache[nx][nz] - sh);
				if (diff > max_diff) {
					max_diff = diff;
				}
			}
			slope_cache[x][z] = max_diff;
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
			float lbf = lake_bank_factors[x][z];
			const BiomeParams &bp = blended[x][z];
			bool flat_shore = (slope_cache[x][z] <= BEACH_MAX_SLOPE);

			bool is_water_body = (rf > 0.0f && original_base_heights[x][z] > (float)(sea_level + RIVER_MIN_HEIGHT)) ||
					(rbf > 0.3f && original_base_heights[x][z] > (float)(sea_level + RIVER_MIN_HEIGHT)) ||
					(lf > 0.0f) ||
					(lbf > 0.3f);

			for (int y = CHUNK_SIZE_Y - 1; y >= 1; y--) {
				int idx = block_index(x, y, z);
				uint16_t block = blocks_w[idx];

				if (block != VOXEL_BLOCK_STONE) {
					continue;
				}

				int depth = sh - y;

				if (depth == 0) {
					// Surface block вЂ” biome-dependent.
					if (is_water_body || sh <= wl) {
						blocks_w[idx] = bp.shore_block;
					} else if (sh <= wl + BEACH_HEIGHT && flat_shore) {
						blocks_w[idx] = bp.shore_block; // Flat shore / beach.
					} else if (sh > bp.snow_line) {
						blocks_w[idx] = bp.snow_block;
					} else {
						blocks_w[idx] = bp.surface_block;
					}
				} else if (depth <= 4) {
					// Subsurface — biome-dependent.
					if (is_water_body || (sh <= wl + BEACH_HEIGHT && flat_shore)) {
						blocks_w[idx] = bp.shore_block;
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
				uint16_t block = blocks_w[idx];

				if (block == VOXEL_BLOCK_AIR || block == VOXEL_BLOCK_WATER || block == VOXEL_BLOCK_BEDROCK) {
					continue;
				}

				// Spaghetti caves: carve only where BOTH noise values are near
				// zero (|n| < threshold). Zero-crossings in 3D form thin sheets
				// that intersect as continuous winding tunnels.
				float threshold = (y >= sh - 2) ? CAVE_SURFACE_THRESHOLD : CAVE_THRESHOLD;

				float na = Math::abs(cave_noise_a->get_noise_3d((real_t)wx, (real_t)y, (real_t)wz));
				float nb = Math::abs(cave_noise_b->get_noise_3d((real_t)wx, (real_t)y, (real_t)wz));
				bool spaghetti = (na < threshold && nb < threshold);

				// Cheese caves: large open chambers, restricted to deep underground
				// (lower 65% of the column, well below y=4).
				bool cheese = false;
				if (!spaghetti && y > 4 && sh > 0) {
					float depth_factor = (float)(sh - y) / (float)sh;
					if (depth_factor > 0.35f) {
						float nc = cave_cheese_noise->get_noise_3d((real_t)wx, (real_t)y, (real_t)wz);
						cheese = (nc > CAVE_CHEESE_THRESHOLD);
					}
				}

				if (spaghetti || cheese) {
					blocks_w[idx] = VOXEL_BLOCK_AIR;
				}
			}
		}
	}

	// --- Pass 4: Feature generation (biome-aware: trees, scatter, etc.) ---
	int border = TREE_CHECK_BORDER;
	for (int wx = world_x_start - border; wx < world_x_start + CHUNK_SIZE_X + border; wx++) {
		for (int wz = world_z_start - border; wz < world_z_start + CHUNK_SIZE_Z + border; wz++) {
			// Get biome params for this column.
			float weights[MAX_BIOMES] = {};
			_get_biome_weights(wx, wz, weights, biome_count);
			BiomeParams bp = _get_blended_params(weights, biome_count);

			// Skip features in water bodies, river banks, and lake banks.
			float rf = _get_river_factor(wx, wz);
			float rbf = _get_river_bank_factor(wx, wz);
			float lf = _get_lake_factor(wx, wz);
			float lbf = _get_lake_bank_factor(wx, wz);
			if (rf > 0.0f || rbf > 0.0f || lf > 0.0f || lbf > 0.0f) {
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

			// Only place features above sea level + beach, below biome snow line.
			if (sh <= sea_level + 2 || sh > bp.snow_line) {
				continue;
			}

			// Iterate features for this biome.
			for (int fi = 0; fi < bp.features.size(); fi++) {
				const VoxelBiomeRegistry::FeatureConfig &fc = bp.features[fi];

				if (!_should_place_feature(wx, wz, fc.density, fi * 1000)) {
					continue;
				}

				if (fc.type == VoxelBiomeRegistry::FeatureConfig::FEATURE_TREE) {
					// Tree features only within extended border (for cross-chunk canopies).
					int trunk_height = _tree_trunk_height(wx, wz, fc.min_trunk, fc.max_trunk);

					switch (fc.canopy_shape) {
						case VoxelBiomeRegistry::CANOPY_CONE:
							_place_tree_cone(blocks_w, local_x, sh, local_z, trunk_height, fc.canopy_radius, (uint16_t)fc.trunk_block, (uint16_t)fc.canopy_block);
							break;
						case VoxelBiomeRegistry::CANOPY_BUSH:
							_place_tree_bush(blocks_w, local_x, sh, local_z, trunk_height, fc.canopy_radius, (uint16_t)fc.trunk_block, (uint16_t)fc.canopy_block);
							break;
						default: // CANOPY_SPHERE
							_place_tree_sphere(blocks_w, local_x, sh, local_z, trunk_height, fc.canopy_radius, (uint16_t)fc.trunk_block, (uint16_t)fc.canopy_block);
							break;
					}
				} else if (fc.type == VoxelBiomeRegistry::FeatureConfig::FEATURE_SCATTER) {
					// Scatter: only inside chunk bounds (no border needed).
					if (local_x >= 0 && local_x < CHUNK_SIZE_X && local_z >= 0 && local_z < CHUNK_SIZE_Z) {
						if (sh >= fc.min_y && sh <= fc.max_y) {
							_place_scatter(blocks_w, local_x, sh, local_z, (uint16_t)fc.block);
						}
					}
				}
			}
		}
	}

	return blocks;
}
