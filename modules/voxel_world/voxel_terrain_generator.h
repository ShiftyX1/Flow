#pragma once

#include "voxel_biome_registry.h"
#include "voxel_block_data.h"

#include "core/math/vector2.h"
#include "core/templates/vector.h"
#include "modules/noise/fastnoise_lite.h"

// ---- Biome system ----

enum BiomeType : uint8_t {
	BIOME_DESERT = 0,
	BIOME_MEADOW,
	BIOME_FOREST,
	BIOME_MOUNTAINS,
	BIOME_MAX
};

struct BiomeParams {
	float height_base; // Additive offset to BASE_HEIGHT.
	float height_scale; // Multiplier for continentalness contribution.
	float detail_scale; // Multiplier for erosion contribution.
	float density_3d_weight; // How much 3D noise affects terrain shape.
	uint8_t surface_block; // Top block type.
	uint8_t subsurface_block; // Blocks below surface (depth 1-4).
	int tree_density; // Hash modulus for tree placement (lower = denser, 0 = no trees).
	int snow_line; // Y-level above which snow replaces surface block.
};

class VoxelTerrainGenerator {
public:
	static const int CHUNK_SIZE_X = 16;
	static const int CHUNK_SIZE_Y = 192;
	static const int CHUNK_SIZE_Z = 16;

	// Maximum number of biomes supported when using a VoxelBiomeRegistry.
	static const int MAX_BIOMES = 64;

	// Tree generation constants.
	static const int TREE_CHECK_BORDER = 4;
	static const int TREE_MIN_TRUNK = 4;
	static const int TREE_MAX_TRUNK = 6;
	static const int TREE_CANOPY_RADIUS = 2;

	// 3D density terrain constants (defaults, overridden per-biome).
	static constexpr float BASE_HEIGHT = 64.0f;
	static constexpr float SQUISH_FACTOR = 0.12f;

	static constexpr float CAVE_THRESHOLD = 0.16f;        // abs(noise) < threshold → carve tunnel
	static constexpr float CAVE_SURFACE_THRESHOLD = 0.05f; // tighter near surface → almost no caves
	static constexpr float CAVE_CHEESE_THRESHOLD = 0.26f;  // cheese > threshold → carve open chamber

	// Water feature constants.
	static constexpr float RIVER_WIDTH = 0.07f;
	static constexpr float RIVER_BANK_WIDTH = 0.06f;
	static constexpr int RIVER_BED_OFFSET = 2;
	static constexpr int RIVER_MIN_HEIGHT = 5;
	static constexpr float LAKE_THRESHOLD = 0.35f;
	static constexpr float LAKE_MAX_DEPTH = 6.0f;
	static constexpr float LAKE_BANK_WIDTH = 0.10f;
	static constexpr int LAKE_LEVEL_GRID = 64;
	static constexpr float OCEAN_THRESHOLD = -0.3f;
	static constexpr float OCEAN_DEPTH_SCALE = 15.0f;

	// Biome lookup tables.
	static const BiomeParams BIOME_TABLE[BIOME_MAX];
	static const Vector2 BIOME_CENTERS[BIOME_MAX]; // Ideal (temperature, humidity) per biome.

private:
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

	int seed = 0;
	int sea_level = 40;

	Ref<VoxelBiomeRegistry> biome_registry;

	// Biome data access (falls back to static tables when no registry is set).
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

	bool _should_place_tree(int p_world_x, int p_world_z, int p_tree_density) const;
	int _tree_trunk_height(int p_world_x, int p_world_z) const;
	void _place_tree(uint8_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, int p_trunk_height) const;

public:
	VoxelTerrainGenerator();
	~VoxelTerrainGenerator();

	void set_seed(int p_seed);
	int get_seed() const { return seed; }

	void set_sea_level(int p_sea_level) { sea_level = p_sea_level; }
	int get_sea_level() const { return sea_level; }

	void set_biome_registry(const Ref<VoxelBiomeRegistry> &p_registry) { biome_registry = p_registry; }

	// Returns the index of the dominant biome at the given world block coordinates.
	int get_biome_index_at(int p_world_x, int p_world_z) const;

	Vector<uint8_t> generate_chunk_data(int p_chunk_x, int p_chunk_z) const;
	static _FORCE_INLINE_ int block_index(int p_x, int p_y, int p_z) {
		return p_x + p_z * CHUNK_SIZE_X + p_y * CHUNK_SIZE_X * CHUNK_SIZE_Z;
	}
};
