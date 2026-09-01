#pragma once

#include "voxel_biome_registry.h"
#include "voxel_block_registry.h"

#include "core/math/vector2.h"
#include "core/templates/vector.h"
#include "modules/noise/fastnoise_lite.h"

// ---- Runtime biome data (populated from VoxelBiomeRegistry) ----

struct BiomeParams {
	float height_base = 0.0f;
	float height_scale = 15.0f;
	float detail_scale = 3.0f;
	float density_3d_weight = 0.5f;
	uint16_t surface_block = 1;
	uint16_t subsurface_block = 2;
	uint16_t shore_block = 4;
	uint16_t snow_block = 6;
	int snow_line = 165;
	// Features (copied from registry at init time).
	Vector<VoxelBiomeRegistry::FeatureConfig> features;
};

struct RuntimeBiomeData {
	BiomeParams params;
	Vector2 center; // (temperature, humidity) in [-1, 1].
};

class VoxelTerrainGenerator {
public:
	static const int CHUNK_SIZE_X = 16;
	static const int CHUNK_SIZE_Y = 192;
	static const int CHUNK_SIZE_Z = 16;

	// Maximum number of biomes supported.
	static const int MAX_BIOMES = 64;

	// 3D density terrain constants (defaults, overridden per-biome).
	static constexpr float BASE_HEIGHT = 64.0f;
	static constexpr float SQUISH_FACTOR = 0.25f;

	static constexpr float CAVE_THRESHOLD = 0.16f;        // abs(noise) < threshold → carve tunnel
	static constexpr float CAVE_SURFACE_THRESHOLD = 0.05f; // tighter near surface → almost no caves
	static constexpr float CAVE_CHEESE_THRESHOLD = 0.26f;  // cheese > threshold → carve open chamber

	// Water feature constants.
	static constexpr float RIVER_WIDTH = 0.07f;
	static constexpr float RIVER_BANK_WIDTH = 0.06f;
	static constexpr int RIVER_BED_OFFSET = 2;
	static constexpr int RIVER_MIN_HEIGHT = 3;
	static constexpr float LAKE_THRESHOLD = 0.35f;
	static constexpr float LAKE_MAX_DEPTH = 6.0f;
	static constexpr float LAKE_BANK_WIDTH = 0.10f;
	static constexpr int LAKE_LEVEL_GRID = 64;
	static constexpr float OCEAN_THRESHOLD = -0.2f;
	static constexpr float OCEAN_DEPTH_SCALE = 25.0f;

	// Beach / shore constants.
	// Sand only appears on shores where the slope is gentle (flat beach).
	// Steep cliffs dropping into water keep the biome block (stone, grass, etc.).
	static constexpr int BEACH_HEIGHT = 3;    // Max blocks above sea_level for the beach zone.
	static constexpr int BEACH_MAX_SLOPE = 3; // Max height diff to a neighbor to qualify as flat.

	// Minimum dominant-biome weight to stay in pure-biome mode (no block dithering).
	// Below this threshold (genuine transition zone) probabilistic mixing is applied.
	static constexpr float BIOME_DITHER_THRESHOLD = 0.30f;
	static constexpr int IRON_ORE_MIN_Y = 4;
	static constexpr int IRON_ORE_MAX_Y = 48;
	static constexpr int COPPER_ORE_MIN_Y = 8;
	static constexpr int COPPER_ORE_MAX_Y = 64;
	static constexpr float IRON_ORE_THRESHOLD = 0.52f;
	static constexpr float COPPER_ORE_THRESHOLD = 0.54f;

	// Tree generation border for cross-chunk canopy.
	static const int TREE_CHECK_BORDER = 4;

	// Biome lookup tables.
	// (no longer static — populated at runtime from VoxelBiomeRegistry or defaults)

private:
	// Runtime biome data (set via set_biome_data or default fallback).
	Vector<RuntimeBiomeData> runtime_biomes;
	// 2D continentalness — base terrain height (OpenSimplex2, low freq).
	Ref<FastNoiseLite> continentalness_noise;
	// 2D erosion — terrain roughness / detail (OpenSimplex2, medium freq).
	Ref<FastNoiseLite> erosion_noise;
	// 3D density — overhangs, natural caves, terrain shape (OpenSimplex2).
	Ref<FastNoiseLite> density_noise;
	// 3D density detail — smaller-scale 3D features (OpenSimplex2, higher freq).
	Ref<FastNoiseLite> density_detail_noise;
	// Spaghetti cave noise pair — guaranteed tunnel networks.
	Ref<FastNoiseLite> cave_noise_a;
	Ref<FastNoiseLite> cave_noise_b;
	// Cheese cave noise — large open chambers deep underground.
	Ref<FastNoiseLite> cave_cheese_noise;

	// Water feature noise layers.
	Ref<FastNoiseLite> river_noise;
	Ref<FastNoiseLite> river_warp_noise;
	Ref<FastNoiseLite> lake_noise;

	// Biome noise layers — very low frequency for large biome regions.
	Ref<FastNoiseLite> temperature_noise;
	Ref<FastNoiseLite> humidity_noise;
	Ref<FastNoiseLite> iron_ore_noise;
	Ref<FastNoiseLite> copper_ore_noise;

	int seed = 0;
	int sea_level = 52;

	// Runtime biome data access.
	int _get_biome_count() const;
	BiomeParams _get_biome_params_at(int p_index) const;
	Vector2 _get_biome_center_at(int p_index) const;

	// Biome weight and blending helpers.
	void _get_biome_weights(int p_world_x, int p_world_z, float *r_weights, int p_biome_count) const;
	BiomeParams _get_blended_params(const float *p_weights, int p_biome_count) const;

	float _get_base_height(int p_world_x, int p_world_z, const BiomeParams &p_params) const;
	int _find_surface_y(int p_world_x, int p_world_z, const BiomeParams &p_params) const;

	// Water feature helpers (per-column, 2D).
	float _get_river_factor(int p_world_x, int p_world_z) const;
	float _get_river_bank_factor(int p_world_x, int p_world_z) const;
	float _get_lake_factor(int p_world_x, int p_world_z) const;
	float _get_lake_bank_factor(int p_world_x, int p_world_z) const;
	int _get_local_water_level(int p_world_x, int p_world_z) const;

	bool _should_place_feature(int p_world_x, int p_world_z, int p_density, int p_feature_salt) const;
	int _tree_trunk_height(int p_world_x, int p_world_z, int p_min_trunk, int p_max_trunk) const;
	void _place_tree_sphere(uint16_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, int p_trunk_height, int p_canopy_radius, uint16_t p_trunk_block, uint16_t p_canopy_block) const;
	void _place_tree_cone(uint16_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, int p_trunk_height, int p_canopy_radius, uint16_t p_trunk_block, uint16_t p_canopy_block) const;
	void _place_tree_bush(uint16_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, int p_trunk_height, int p_canopy_radius, uint16_t p_trunk_block, uint16_t p_canopy_block) const;
	void _place_scatter(uint16_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, uint16_t p_block) const;

public:
	VoxelTerrainGenerator();
	~VoxelTerrainGenerator();

	void set_seed(int p_seed);
	int get_seed() const { return seed; }

	void set_sea_level(int p_sea_level) { sea_level = p_sea_level; }
	int get_sea_level() const { return sea_level; }

	// Set biome data from a VoxelBiomeRegistry. Call before generating chunks.
	void set_biome_data(const Vector<RuntimeBiomeData> &p_biomes);
	// Populate built-in biomes (fallback if no registry provided).
	void setup_default_biomes();

	// Returns the index of the dominant biome at the given world block coordinates.
	int get_biome_index_at(int p_world_x, int p_world_z) const;
	int get_surface_y_at(int p_world_x, int p_world_z) const;

	Vector<uint16_t> generate_chunk_data(int p_chunk_x, int p_chunk_z) const;
	static _FORCE_INLINE_ int block_index(int p_x, int p_y, int p_z) {
		return p_x + p_z * CHUNK_SIZE_X + p_y * CHUNK_SIZE_X * CHUNK_SIZE_Z;
	}
};
