#pragma once

#include "voxel_block_registry.h"
#include "voxel_terrain_generator.h"

#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"
#include "servers/rendering/rendering_server_enums.h"

class VoxelMesher {
public:
	struct MeshSurface {
		Array arrays;
		uint64_t custom_format_flags = 0; // Mesh format flags for custom vertex attributes.
		Ref<Texture2D> texture; // null = vertex-colored (no texture).
		Ref<ShaderMaterial> shader_material; // Optional custom shader for this surface.
		int block_type = -1; // Primary block type for this surface (-1 = mixed/untextured).
	};

	// Pointers to the block arrays of the four horizontal neighbours (null = treat as air).
	// px = +X neighbour, nx = -X neighbour, pz = +Z neighbour, nz = -Z neighbour.
	struct NeighborBlocks {
		const uint16_t *px;
		const uint16_t *nx;
		const uint16_t *pz;
		const uint16_t *nz;
		NeighborBlocks() : px(nullptr), nx(nullptr), pz(nullptr), nz(nullptr) {}
	};

	// Pointers to neighbor chunk light data arrays.
	struct NeighborLight {
		const uint8_t *px = nullptr;
		const uint8_t *nx = nullptr;
		const uint8_t *pz = nullptr;
		const uint8_t *nz = nullptr;
	};

	// Build multiple mesh surfaces: one per unique texture + one for untextured faces.
	struct VertexLight {
		float sun;
		float block;
		float ao;
	};

	static Vector<MeshSurface> build_chunk_mesh(const Vector<uint16_t> &p_blocks, float p_block_size, const Ref<VoxelBlockRegistry> &p_registry, const NeighborBlocks &p_neighbors = NeighborBlocks(), const uint8_t *p_light_data = nullptr, const NeighborLight &p_neighbor_light = NeighborLight());

private:
	static const int CX = VoxelTerrainGenerator::CHUNK_SIZE_X;
	static const int CY = VoxelTerrainGenerator::CHUNK_SIZE_Y;
	static const int CZ = VoxelTerrainGenerator::CHUNK_SIZE_Z;

	static _FORCE_INLINE_ int get_block(const uint16_t *p_blocks, int p_x, int p_y, int p_z, const NeighborBlocks &p_neighbors = NeighborBlocks()) {
		if (p_y < 0 || p_y >= CY) {
			return VOXEL_BLOCK_AIR;
		}
		if (p_x < 0) {
			if (p_neighbors.nx) {
				return (int)p_neighbors.nx[VoxelTerrainGenerator::block_index(p_x + CX, p_y, p_z)];
			}
			return VOXEL_BLOCK_AIR;
		}
		if (p_x >= CX) {
			if (p_neighbors.px) {
				return (int)p_neighbors.px[VoxelTerrainGenerator::block_index(p_x - CX, p_y, p_z)];
			}
			return VOXEL_BLOCK_AIR;
		}
		if (p_z < 0) {
			if (p_neighbors.nz) {
				return (int)p_neighbors.nz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z + CZ)];
			}
			return VOXEL_BLOCK_AIR;
		}
		if (p_z >= CZ) {
			if (p_neighbors.pz) {
				return (int)p_neighbors.pz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z - CZ)];
			}
			return VOXEL_BLOCK_AIR;
		}
		return (int)p_blocks[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)];
	}

	// Get sunlight (high nibble) from light data, with cross-chunk neighbor support.
	static _FORCE_INLINE_ uint8_t get_light_sun(const uint8_t *p_light, int p_x, int p_y, int p_z, const NeighborLight &p_nl) {
		if (!p_light) {
			return 15; // No light data = full sunlight (fallback).
		}
		if (p_y < 0) {
			return 0;
		}
		if (p_y >= CY) {
			return 15;
		}
		if (p_x < 0) {
			return p_nl.nx ? ((p_nl.nx[VoxelTerrainGenerator::block_index(p_x + CX, p_y, p_z)] >> 4) & 0x0F) : 0;
		}
		if (p_x >= CX) {
			return p_nl.px ? ((p_nl.px[VoxelTerrainGenerator::block_index(p_x - CX, p_y, p_z)] >> 4) & 0x0F) : 0;
		}
		if (p_z < 0) {
			return p_nl.nz ? ((p_nl.nz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z + CZ)] >> 4) & 0x0F) : 0;
		}
		if (p_z >= CZ) {
			return p_nl.pz ? ((p_nl.pz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z - CZ)] >> 4) & 0x0F) : 0;
		}
		return (p_light[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)] >> 4) & 0x0F;
	}

	// Get block light (low nibble) from light data, with cross-chunk neighbor support.
	static _FORCE_INLINE_ uint8_t get_light_block(const uint8_t *p_light, int p_x, int p_y, int p_z, const NeighborLight &p_nl) {
		if (!p_light) {
			return 0;
		}
		if (p_y < 0 || p_y >= CY) {
			return 0;
		}
		if (p_x < 0) {
			return p_nl.nx ? (p_nl.nx[VoxelTerrainGenerator::block_index(p_x + CX, p_y, p_z)] & 0x0F) : 0;
		}
		if (p_x >= CX) {
			return p_nl.px ? (p_nl.px[VoxelTerrainGenerator::block_index(p_x - CX, p_y, p_z)] & 0x0F) : 0;
		}
		if (p_z < 0) {
			return p_nl.nz ? (p_nl.nz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z + CZ)] & 0x0F) : 0;
		}
		if (p_z >= CZ) {
			return p_nl.pz ? (p_nl.pz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z - CZ)] & 0x0F) : 0;
		}
		return p_light[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)] & 0x0F;
	}

	// Compute per-vertex AO (0-3 occluders) for a face vertex.
	// side1/side2 = two edge-adjacent blocks, corner = diagonal block.
	static _FORCE_INLINE_ float compute_ao(bool side1, bool side2, bool corner) {
		int occluders = (int)side1 + (int)side2 + (int)(corner && !(side1 && side2));
		// If both sides are solid, the corner is fully occluded regardless.
		if (side1 && side2) {
			occluders = 3;
		}
		return 1.0f - occluders * 0.22f; // Range: [0.34, 1.0]
	}

	// Compute smooth light + AO for a single face vertex.
	static VertexLight compute_vertex_light(const uint16_t *p_blocks, const uint8_t *p_light,
			int bx, int by, int bz,
			const Vector3 &p_normal, int p_corner_u, int p_corner_v,
			const NeighborBlocks &p_nb, const NeighborLight &p_nl,
			const VoxelBlockRegistry *p_reg);

	struct SurfaceData {
		Vector<Vector3> vertices;
		Vector<Vector3> normals;
		Vector<Color> colors;
		Vector<Vector2> uvs;
		Vector<uint8_t> custom0; // RGBA8 packed: R=sun, G=block_light, B=AO, A=255.
		bool has_uv = false;
	};

	static void add_face(
			SurfaceData &r_surface,
			const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3,
			const Vector3 &p_normal,
			const Color &p_color,
			bool p_has_uv,
			bool p_flip_v,
			const VertexLight &p_vl0, const VertexLight &p_vl1, const VertexLight &p_vl2, const VertexLight &p_vl3);

	// Overload without light data (fallback: full bright).
	static void add_face(
			SurfaceData &r_surface,
			const Vector3 &p_v0, const Vector3 &p_v1, const Vector3 &p_v2, const Vector3 &p_v3,
			const Vector3 &p_normal,
			const Color &p_color,
			bool p_has_uv,
			bool p_flip_v = false);
};
