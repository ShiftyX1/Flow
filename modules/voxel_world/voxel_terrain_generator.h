#pragma once

#include "voxel_block_data.h"

#include "core/templates/vector.h"
#include "modules/noise/fastnoise_lite.h"

class VoxelTerrainGenerator {
public:
	static const int CHUNK_SIZE_X = 16;
	static const int CHUNK_SIZE_Y = 64;
	static const int CHUNK_SIZE_Z = 16;

private:
	Ref<FastNoiseLite> height_noise;
	Ref<FastNoiseLite> detail_noise;
	int seed = 0;
	int sea_level = 20;

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
