#include "voxel_terrain_generator.h"

VoxelTerrainGenerator::VoxelTerrainGenerator() {
	height_noise.instantiate();
	height_noise->set_noise_type(FastNoiseLite::TYPE_PERLIN);
	height_noise->set_frequency(0.008f);
	height_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	height_noise->set_fractal_octaves(4);
	height_noise->set_fractal_lacunarity(2.0f);
	height_noise->set_fractal_gain(0.5f);

	detail_noise.instantiate();
	detail_noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX);
	detail_noise->set_frequency(0.03f);
	detail_noise->set_fractal_type(FastNoiseLite::FRACTAL_FBM);
	detail_noise->set_fractal_octaves(2);
}

VoxelTerrainGenerator::~VoxelTerrainGenerator() {
}

void VoxelTerrainGenerator::set_seed(int p_seed) {
	seed = p_seed;
	height_noise->set_seed(p_seed);
	detail_noise->set_seed(p_seed + 1);
}

Vector<uint8_t> VoxelTerrainGenerator::generate_chunk_data(int p_chunk_x, int p_chunk_z) const {
	const int total = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
	Vector<uint8_t> blocks;
	blocks.resize(total);
	uint8_t *blocks_w = blocks.ptrw();
	memset(blocks_w, VOXEL_BLOCK_AIR, total);

	int world_x_start = p_chunk_x * CHUNK_SIZE_X;
	int world_z_start = p_chunk_z * CHUNK_SIZE_Z;

	for (int x = 0; x < CHUNK_SIZE_X; x++) {
		for (int z = 0; z < CHUNK_SIZE_Z; z++) {
			real_t wx = (real_t)(world_x_start + x);
			real_t wz = (real_t)(world_z_start + z);

			real_t h = height_noise->get_noise_2d(wx, wz);
			real_t d = detail_noise->get_noise_2d(wx, wz);

			int surface_height = (int)(32.0f + h * 20.0f + d * 4.0f);
			if (surface_height < 1) {
				surface_height = 1;
			}
			if (surface_height >= CHUNK_SIZE_Y - 1) {
				surface_height = CHUNK_SIZE_Y - 2;
			}

			for (int y = 0; y < CHUNK_SIZE_Y; y++) {
				int idx = block_index(x, y, z);

				if (y == 0) {
					blocks_w[idx] = VOXEL_BLOCK_BEDROCK;
				} else if (y < surface_height - 4) {
					blocks_w[idx] = VOXEL_BLOCK_STONE;
				} else if (y < surface_height) {
					if (surface_height <= sea_level + 2 && surface_height >= sea_level - 2) {
						blocks_w[idx] = VOXEL_BLOCK_SAND;
					} else {
						blocks_w[idx] = VOXEL_BLOCK_DIRT;
					}
				} else if (y == surface_height) {
					if (surface_height <= sea_level) {
						blocks_w[idx] = VOXEL_BLOCK_SAND;
					} else if (surface_height > 50) {
						blocks_w[idx] = VOXEL_BLOCK_SNOW;
					} else {
						blocks_w[idx] = VOXEL_BLOCK_GRASS;
					}
				} else if (y <= sea_level && y > surface_height) {
					blocks_w[idx] = VOXEL_BLOCK_WATER;
				}
				// else: AIR (already set by memset).
			}
		}
	}

	return blocks;
}
