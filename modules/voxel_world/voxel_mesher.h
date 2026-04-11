#pragma once

#include "voxel_block_data.h"
#include "voxel_block_registry.h"
#include "voxel_terrain_generator.h"

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"

class VoxelMesher {
public:
	struct MeshSurface {
		Array arrays;
		Ref<Texture2D> texture; // null = vertex-colored (no texture).
		Ref<ShaderMaterial> shader_material; // Optional custom shader for this surface.
		int block_type = -1; // Primary block type for this surface (-1 = mixed/untextured).
	};

	// Pointers to the block arrays of the four horizontal neighbours (null = treat as air).
	// px = +X neighbour, nx = -X neighbour, pz = +Z neighbour, nz = -Z neighbour.
	struct NeighborBlocks {
		const uint8_t *px = nullptr;
		const uint8_t *nx = nullptr;
		const uint8_t *pz = nullptr;
		const uint8_t *nz = nullptr;
	};

	// Build multiple mesh surfaces: one per unique texture + one for untextured faces.
	static Vector<MeshSurface> build_chunk_mesh(const Vector<uint8_t> &p_blocks, float p_block_size, const Ref<VoxelBlockRegistry> &p_registry, uint32_t p_alpha_block_flags = 0, const NeighborBlocks &p_neighbors = NeighborBlocks());

private:
	static _FORCE_INLINE_ VoxelBlockType get_block(const uint8_t *p_blocks, int p_x, int p_y, int p_z, const NeighborBlocks &p_neighbors = NeighborBlocks()) {
		const int CX = VoxelTerrainGenerator::CHUNK_SIZE_X;
		const int CY = VoxelTerrainGenerator::CHUNK_SIZE_Y;
		const int CZ = VoxelTerrainGenerator::CHUNK_SIZE_Z;
		if (p_y < 0 || p_y >= CY) {
			return VOXEL_BLOCK_AIR;
		}
		if (p_x < 0) {
			if (p_neighbors.nx) {
				return (VoxelBlockType)p_neighbors.nx[VoxelTerrainGenerator::block_index(p_x + CX, p_y, p_z)];
			}
			return VOXEL_BLOCK_AIR;
		}
		if (p_x >= CX) {
			if (p_neighbors.px) {
				return (VoxelBlockType)p_neighbors.px[VoxelTerrainGenerator::block_index(p_x - CX, p_y, p_z)];
			}
			return VOXEL_BLOCK_AIR;
		}
		if (p_z < 0) {
			if (p_neighbors.nz) {
				return (VoxelBlockType)p_neighbors.nz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z + CZ)];
			}
			return VOXEL_BLOCK_AIR;
		}
		if (p_z >= CZ) {
			if (p_neighbors.pz) {
				return (VoxelBlockType)p_neighbors.pz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z - CZ)];
			}
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
