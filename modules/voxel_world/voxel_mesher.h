#pragma once

#include "voxel_block_data.h"
#include "voxel_terrain_generator.h"

#include "core/math/vector3.h"
#include "core/variant/array.h"
#include "scene/resources/mesh.h"

class VoxelMesher {
public:
	static Array build_chunk_mesh(const Vector<uint8_t> &p_blocks, float p_block_size);

private:
	static _FORCE_INLINE_ VoxelBlockType get_block(const uint8_t *p_blocks, int p_x, int p_y, int p_z) {
		if (p_x < 0 || p_x >= VoxelTerrainGenerator::CHUNK_SIZE_X ||
				p_y < 0 || p_y >= VoxelTerrainGenerator::CHUNK_SIZE_Y ||
				p_z < 0 || p_z >= VoxelTerrainGenerator::CHUNK_SIZE_Z) {
			return VOXEL_BLOCK_AIR;
		}
		return (VoxelBlockType)p_blocks[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)];
	}

	static void add_face(
			Vector<Vector3> &r_vertices,
			Vector<Vector3> &r_normals,
			Vector<Color> &r_colors,
			const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3,
			const Vector3 &p_normal,
			const Color &p_color);
};
