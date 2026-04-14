#pragma once

#include "voxel_block_data.h"
#include "voxel_terrain_generator.h"

#include "core/templates/local_vector.h"

// Static utility class for voxel light propagation (BFS flood-fill).
// Thread-safe: operates only on raw data arrays, no Godot API calls.
class VoxelLightMap {
public:
	// Pointers to neighbor chunk light/block data (+x, -x, +z, -z). null = treat as boundary.
	struct NeighborData {
		const uint8_t *blocks_px = nullptr;
		const uint8_t *blocks_nx = nullptr;
		const uint8_t *blocks_pz = nullptr;
		const uint8_t *blocks_nz = nullptr;
		const uint8_t *light_px = nullptr;
		const uint8_t *light_nx = nullptr;
		const uint8_t *light_pz = nullptr;
		const uint8_t *light_nz = nullptr;
	};

	// Propagate sunlight and block light for a full chunk.
	// p_light_data must be pre-allocated to the same size as p_blocks (all zeroed).
	static void propagate_all(uint8_t *p_light_data, const uint8_t *p_blocks, const NeighborData &p_neighbors);

	// Sunlight only.
	static void propagate_sunlight(uint8_t *p_light_data, const uint8_t *p_blocks, const NeighborData &p_neighbors);

	// Block (emissive) light only.
	static void propagate_block_light(uint8_t *p_light_data, const uint8_t *p_blocks, const NeighborData &p_neighbors);

	// Remove and re-propagate light around a changed block position.
	// Call after a block is placed or broken. Operates on a single chunk.
	static void remove_and_repropagate_sunlight(uint8_t *p_light_data, const uint8_t *p_blocks, const NeighborData &p_neighbors);
	static void remove_and_repropagate_block_light(uint8_t *p_light_data, const uint8_t *p_blocks, const NeighborData &p_neighbors);

private:
	static const int CX = VoxelTerrainGenerator::CHUNK_SIZE_X;
	static const int CY = VoxelTerrainGenerator::CHUNK_SIZE_Y;
	static const int CZ = VoxelTerrainGenerator::CHUNK_SIZE_Z;

	struct LightNode {
		int16_t x, y, z;
		uint8_t level;
	};

	// Get block type at position, handling cross-chunk neighbor lookups.
	static _FORCE_INLINE_ VoxelBlockType get_block(const uint8_t *p_blocks, int p_x, int p_y, int p_z, const NeighborData &p_nb) {
		if (p_y < 0 || p_y >= CY) {
			return VOXEL_BLOCK_AIR;
		}
		if (p_x < 0) {
			return p_nb.blocks_nx ? (VoxelBlockType)p_nb.blocks_nx[VoxelTerrainGenerator::block_index(p_x + CX, p_y, p_z)] : VOXEL_BLOCK_AIR;
		}
		if (p_x >= CX) {
			return p_nb.blocks_px ? (VoxelBlockType)p_nb.blocks_px[VoxelTerrainGenerator::block_index(p_x - CX, p_y, p_z)] : VOXEL_BLOCK_AIR;
		}
		if (p_z < 0) {
			return p_nb.blocks_nz ? (VoxelBlockType)p_nb.blocks_nz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z + CZ)] : VOXEL_BLOCK_AIR;
		}
		if (p_z >= CZ) {
			return p_nb.blocks_pz ? (VoxelBlockType)p_nb.blocks_pz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z - CZ)] : VOXEL_BLOCK_AIR;
		}
		return (VoxelBlockType)p_blocks[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)];
	}

	// Get/set sunlight (high nibble) for local coordinates only.
	static _FORCE_INLINE_ uint8_t get_sun(const uint8_t *p_light, int p_x, int p_y, int p_z, const NeighborData &p_nb) {
		if (p_y < 0 || p_y >= CY) {
			return (p_y >= CY) ? 15 : 0; // Above chunk = full sunlight.
		}
		if (p_x < 0) {
			return p_nb.light_nx ? ((p_nb.light_nx[VoxelTerrainGenerator::block_index(p_x + CX, p_y, p_z)] >> 4) & 0x0F) : 0;
		}
		if (p_x >= CX) {
			return p_nb.light_px ? ((p_nb.light_px[VoxelTerrainGenerator::block_index(p_x - CX, p_y, p_z)] >> 4) & 0x0F) : 0;
		}
		if (p_z < 0) {
			return p_nb.light_nz ? ((p_nb.light_nz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z + CZ)] >> 4) & 0x0F) : 0;
		}
		if (p_z >= CZ) {
			return p_nb.light_pz ? ((p_nb.light_pz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z - CZ)] >> 4) & 0x0F) : 0;
		}
		return (p_light[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)] >> 4) & 0x0F;
	}

	static _FORCE_INLINE_ void set_sun(uint8_t *p_light, int p_x, int p_y, int p_z, uint8_t p_val) {
		if (p_x < 0 || p_x >= CX || p_y < 0 || p_y >= CY || p_z < 0 || p_z >= CZ) {
			return;
		}
		int idx = VoxelTerrainGenerator::block_index(p_x, p_y, p_z);
		p_light[idx] = (p_light[idx] & 0x0F) | ((p_val & 0x0F) << 4);
	}

	static _FORCE_INLINE_ uint8_t get_blight(const uint8_t *p_light, int p_x, int p_y, int p_z, const NeighborData &p_nb) {
		if (p_y < 0 || p_y >= CY) {
			return 0;
		}
		if (p_x < 0) {
			return p_nb.light_nx ? (p_nb.light_nx[VoxelTerrainGenerator::block_index(p_x + CX, p_y, p_z)] & 0x0F) : 0;
		}
		if (p_x >= CX) {
			return p_nb.light_px ? (p_nb.light_px[VoxelTerrainGenerator::block_index(p_x - CX, p_y, p_z)] & 0x0F) : 0;
		}
		if (p_z < 0) {
			return p_nb.light_nz ? (p_nb.light_nz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z + CZ)] & 0x0F) : 0;
		}
		if (p_z >= CZ) {
			return p_nb.light_pz ? (p_nb.light_pz[VoxelTerrainGenerator::block_index(p_x, p_y, p_z - CZ)] & 0x0F) : 0;
		}
		return p_light[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)] & 0x0F;
	}

	static _FORCE_INLINE_ void set_blight(uint8_t *p_light, int p_x, int p_y, int p_z, uint8_t p_val) {
		if (p_x < 0 || p_x >= CX || p_y < 0 || p_y >= CY || p_z < 0 || p_z >= CZ) {
			return;
		}
		int idx = VoxelTerrainGenerator::block_index(p_x, p_y, p_z);
		p_light[idx] = (p_light[idx] & 0xF0) | (p_val & 0x0F);
	}
};
