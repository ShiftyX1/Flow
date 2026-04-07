#pragma once

#include "voxel_block_data.h"

#include "core/templates/vector.h"
#include "modules/noise/fastnoise_lite.h"

class VoxelTerrainGenerator {
public:
	static const int CHUNK_SIZE_X = 16;
	static const int CHUNK_SIZE_Y = 64;
	static const int CHUNK_SIZE_Z = 16;

	// Tree generation constants.
	static const int TREE_CHECK_BORDER = 4;
	static const int TREE_DENSITY = 80;
	static const int TREE_MIN_TRUNK = 4;
	static const int TREE_MAX_TRUNK = 6;
	static const int TREE_CANOPY_RADIUS = 2;

	// 3D density terrain constants.
	static constexpr float BASE_HEIGHT = 32.0f;
	static constexpr float HEIGHT_SCALE = 20.0f;
	static constexpr float DETAIL_SCALE = 4.0f;
	static constexpr float DENSITY_3D_WEIGHT = 6.0f; // How much 3D noise can shift the surface.
	static constexpr float SQUISH_FACTOR = 0.12f; // How quickly density increases with depth.

	static constexpr float CAVE_THRESHOLD = 0.15f;
	static constexpr float CAVE_SURFACE_THRESHOLD = 0.35f;

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

	int seed = 0;
	int sea_level = 20;

	float _get_base_height(int p_world_x, int p_world_z) const;
	float _get_density(int p_world_x, int p_world_y, int p_world_z) const;
	int _find_surface_y(int p_world_x, int p_world_z) const;

	bool _should_place_tree(int p_world_x, int p_world_z) const;
	int _tree_trunk_height(int p_world_x, int p_world_z) const;
	void _place_tree(uint8_t *p_blocks, int p_local_x, int p_surface_y, int p_local_z, int p_trunk_height) const;

public:
	VoxelTerrainGenerator();
	~VoxelTerrainGenerator();

	void set_seed(int p_seed);
	int get_seed() const { return seed; }

	void set_sea_level(int p_sea_level) { sea_level = p_sea_level; }
	int get_sea_level() const { return sea_level; }
	Vector<uint8_t> generate_chunk_data(int p_chunk_x, int p_chunk_z) const;
	static _FORCE_INLINE_ int block_index(int p_x, int p_y, int p_z) {
		return p_x + p_z * CHUNK_SIZE_X + p_y * CHUNK_SIZE_X * CHUNK_SIZE_Z;
	}
};
