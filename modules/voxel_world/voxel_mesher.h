#pragma once

#include "voxel_block_data.h"
#include "voxel_block_registry.h"
#include "voxel_terrain_generator.h"

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"

class VoxelMesher {
public:
	struct MeshSurface {
		Array arrays;
		Ref<Texture2D> texture; // null = vertex-colored (no texture).
		int block_type = -1; // Primary block type for this surface (-1 = mixed/untextured).
	};

	// Build multiple mesh surfaces: one per unique texture + one for untextured faces.
	static Vector<MeshSurface> build_chunk_mesh(const Vector<uint8_t> &p_blocks, float p_block_size, const Ref<VoxelBlockRegistry> &p_registry);

private:
	static _FORCE_INLINE_ VoxelBlockType get_block(const uint8_t *p_blocks, int p_x, int p_y, int p_z) {
		if (p_x < 0 || p_x >= VoxelTerrainGenerator::CHUNK_SIZE_X ||
				p_y < 0 || p_y >= VoxelTerrainGenerator::CHUNK_SIZE_Y ||
				p_z < 0 || p_z >= VoxelTerrainGenerator::CHUNK_SIZE_Z) {
			return VOXEL_BLOCK_AIR;
		}
		return (VoxelBlockType)p_blocks[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)];
	}

	struct SurfaceData {
		Vector<Vector3> vertices;
		Vector<Vector3> normals;
		Vector<Color> colors;
		Vector<Vector2> uvs;
		bool has_uv = false;
	};

	static void add_face(
			SurfaceData &r_surface,
			const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3,
			const Vector3 &p_normal,
			const Color &p_color,
			bool p_has_uv,
			bool p_flip_v = false);
};
