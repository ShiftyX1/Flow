#include "voxel_light_map.h"

void VoxelLightMap::propagate_all(uint8_t *p_light_data, const uint16_t *p_blocks, const NeighborData &p_neighbors, const uint8_t *p_opacity, const uint8_t *p_emission) {
	propagate_sunlight(p_light_data, p_blocks, p_neighbors, p_opacity);
	propagate_block_light(p_light_data, p_blocks, p_neighbors, p_opacity, p_emission);
}

void VoxelLightMap::propagate_sunlight(uint8_t *p_light_data, const uint16_t *p_blocks, const NeighborData &p_neighbors, const uint8_t *p_opacity) {
	LocalVector<LightNode> queue;

	// Phase 1: Seed sunlight from above вЂ” columns with no solid block above get sunlight=15.
	// Sunlight propagates straight down through transparent blocks without attenuation.
	for (int z = 0; z < CZ; z++) {
		for (int x = 0; x < CX; x++) {
			for (int y = CY - 1; y >= 0; y--) {
				int type = (int)p_blocks[VoxelTerrainGenerator::block_index(x, y, z)];
				uint8_t opacity = p_opacity[type];

				if (opacity >= 15) {
					break; // Fully opaque вЂ” no more sunlight below.
				}

				if (opacity == 0) {
					// Fully transparent вЂ” full sunlight passes through.
					set_sun(p_light_data, x, y, z, 15);
					// Add to queue for horizontal spread.
					queue.push_back({ (int16_t)x, (int16_t)y, (int16_t)z, 15 });
				} else {
					// Partially transparent (e.g. water, leaves) вЂ” sunlight still drops.
					// For downward propagation, reduce by opacity.
					uint8_t above_sun = (y < CY - 1) ? get_sun(p_light_data, x, y + 1, z, p_neighbors) : 15;
					int new_val = (int)above_sun - (int)opacity;
					if (new_val > 0) {
						set_sun(p_light_data, x, y, z, (uint8_t)new_val);
						queue.push_back({ (int16_t)x, (int16_t)y, (int16_t)z, (uint8_t)new_val });
					} else {
						break;
					}
				}
			}
		}
	}

	// Phase 2: Seed from neighbor chunk borders (if neighbors have light data).
	// Check all four horizontal neighbors' border columns for sunlight that can bleed in.
	static const int dx[4] = { 1, -1, 0, 0 };
	static const int dz[4] = { 0, 0, 1, -1 };

	for (int y = 0; y < CY; y++) {
		// +X border: neighbor at x=0 of +X chunk can bleed into our x=CX-1.
		if (p_neighbors.light_px) {
			for (int z = 0; z < CZ; z++) {
				uint8_t nb_sun = (p_neighbors.light_px[VoxelTerrainGenerator::block_index(0, y, z)] >> 4) & 0x0F;
				if (nb_sun > 1) {
					int our_type = (int)p_blocks[VoxelTerrainGenerator::block_index(CX - 1, y, z)];
					uint8_t our_opacity = p_opacity[our_type];
					if (our_opacity < 15) {
						int new_val = (int)nb_sun - 1 - (int)our_opacity;
						if (new_val > (int)get_sun(p_light_data, CX - 1, y, z, p_neighbors)) {
							set_sun(p_light_data, CX - 1, y, z, (uint8_t)new_val);
							queue.push_back({ (int16_t)(CX - 1), (int16_t)y, (int16_t)z, (uint8_t)new_val });
						}
					}
				}
			}
		}
		// -X border
		if (p_neighbors.light_nx) {
			for (int z = 0; z < CZ; z++) {
				uint8_t nb_sun = (p_neighbors.light_nx[VoxelTerrainGenerator::block_index(CX - 1, y, z)] >> 4) & 0x0F;
				if (nb_sun > 1) {
					int our_type = (int)p_blocks[VoxelTerrainGenerator::block_index(0, y, z)];
					uint8_t our_opacity = p_opacity[our_type];
					if (our_opacity < 15) {
						int new_val = (int)nb_sun - 1 - (int)our_opacity;
						if (new_val > (int)get_sun(p_light_data, 0, y, z, p_neighbors)) {
							set_sun(p_light_data, 0, y, z, (uint8_t)new_val);
							queue.push_back({ 0, (int16_t)y, (int16_t)z, (uint8_t)new_val });
						}
					}
				}
			}
		}
		// +Z border
		if (p_neighbors.light_pz) {
			for (int x = 0; x < CX; x++) {
				uint8_t nb_sun = (p_neighbors.light_pz[VoxelTerrainGenerator::block_index(x, y, 0)] >> 4) & 0x0F;
				if (nb_sun > 1) {
					int our_type = (int)p_blocks[VoxelTerrainGenerator::block_index(x, y, CZ - 1)];
					uint8_t our_opacity = p_opacity[our_type];
					if (our_opacity < 15) {
						int new_val = (int)nb_sun - 1 - (int)our_opacity;
						if (new_val > (int)get_sun(p_light_data, x, y, CZ - 1, p_neighbors)) {
							set_sun(p_light_data, x, y, CZ - 1, (uint8_t)new_val);
							queue.push_back({ (int16_t)x, (int16_t)y, (int16_t)(CZ - 1), (uint8_t)new_val });
						}
					}
				}
			}
		}
		// -Z border
		if (p_neighbors.light_nz) {
			for (int x = 0; x < CX; x++) {
				uint8_t nb_sun = (p_neighbors.light_nz[VoxelTerrainGenerator::block_index(x, y, CZ - 1)] >> 4) & 0x0F;
				if (nb_sun > 1) {
					int our_type = (int)p_blocks[VoxelTerrainGenerator::block_index(x, y, 0)];
					uint8_t our_opacity = p_opacity[our_type];
					if (our_opacity < 15) {
						int new_val = (int)nb_sun - 1 - (int)our_opacity;
						if (new_val > (int)get_sun(p_light_data, x, y, 0, p_neighbors)) {
							set_sun(p_light_data, x, y, 0, (uint8_t)new_val);
							queue.push_back({ (int16_t)x, (int16_t)y, 0, (uint8_t)new_val });
						}
					}
				}
			}
		}
	}

	// Phase 3: BFS spread sunlight horizontally and upward.
	uint32_t head = 0;
	while (head < queue.size()) {
		LightNode node = queue[head++];

		for (int dir = 0; dir < 6; dir++) {
			int nx = node.x + dx[dir % 4];
			int ny = node.y;
			int nz = node.z + dz[dir % 4];

			// Directions 4 and 5 are Y-axis.
			if (dir == 4) {
				nx = node.x;
				ny = node.y + 1;
				nz = node.z;
			} else if (dir == 5) {
				nx = node.x;
				ny = node.y - 1;
				nz = node.z;
			}

			// Only propagate within this chunk.
			if (nx < 0 || nx >= CX || ny < 0 || ny >= CY || nz < 0 || nz >= CZ) {
				continue;
			}

			int nb_type = (int)p_blocks[VoxelTerrainGenerator::block_index(nx, ny, nz)];
			uint8_t opacity = p_opacity[nb_type];
			if (opacity >= 15) {
				continue;
			}

			int new_level;
			if (dir == 5 && opacity == 0 && node.level == 15) {
				// Sunlight going straight down through fully transparent block: no attenuation.
				new_level = 15;
			} else {
				new_level = (int)node.level - 1 - (int)opacity;
			}

			if (new_level <= 0) {
				continue;
			}

			uint8_t current = get_sun(p_light_data, nx, ny, nz, p_neighbors);
			if ((uint8_t)new_level > current) {
				set_sun(p_light_data, nx, ny, nz, (uint8_t)new_level);
				queue.push_back({ (int16_t)nx, (int16_t)ny, (int16_t)nz, (uint8_t)new_level });
			}
		}
	}
}

void VoxelLightMap::propagate_block_light(uint8_t *p_light_data, const uint16_t *p_blocks, const NeighborData &p_neighbors, const uint8_t *p_opacity, const uint8_t *p_emission) {
	LocalVector<LightNode> queue;

	// Seed from emissive blocks.
	for (int y = 0; y < CY; y++) {
		for (int z = 0; z < CZ; z++) {
			for (int x = 0; x < CX; x++) {
				int type = (int)p_blocks[VoxelTerrainGenerator::block_index(x, y, z)];
				uint8_t emission = p_emission[type];
				if (emission > 0) {
					set_blight(p_light_data, x, y, z, emission);
					queue.push_back({ (int16_t)x, (int16_t)y, (int16_t)z, emission });
				}
			}
		}
	}

	// Seed from neighbor chunk borders (block light bleeding in).
	for (int y = 0; y < CY; y++) {
		if (p_neighbors.light_px) {
			for (int z = 0; z < CZ; z++) {
				uint8_t nb_bl = p_neighbors.light_px[VoxelTerrainGenerator::block_index(0, y, z)] & 0x0F;
				if (nb_bl > 1) {
					int our_type = (int)p_blocks[VoxelTerrainGenerator::block_index(CX - 1, y, z)];
					uint8_t our_opacity = p_opacity[our_type];
					if (our_opacity < 15) {
						int new_val = (int)nb_bl - 1 - (int)our_opacity;
						if (new_val > (int)get_blight(p_light_data, CX - 1, y, z, p_neighbors)) {
							set_blight(p_light_data, CX - 1, y, z, (uint8_t)new_val);
							queue.push_back({ (int16_t)(CX - 1), (int16_t)y, (int16_t)z, (uint8_t)new_val });
						}
					}
				}
			}
		}
		if (p_neighbors.light_nx) {
			for (int z = 0; z < CZ; z++) {
				uint8_t nb_bl = p_neighbors.light_nx[VoxelTerrainGenerator::block_index(CX - 1, y, z)] & 0x0F;
				if (nb_bl > 1) {
					int our_type = (int)p_blocks[VoxelTerrainGenerator::block_index(0, y, z)];
					uint8_t our_opacity = p_opacity[our_type];
					if (our_opacity < 15) {
						int new_val = (int)nb_bl - 1 - (int)our_opacity;
						if (new_val > (int)get_blight(p_light_data, 0, y, z, p_neighbors)) {
							set_blight(p_light_data, 0, y, z, (uint8_t)new_val);
							queue.push_back({ 0, (int16_t)y, (int16_t)z, (uint8_t)new_val });
						}
					}
				}
			}
		}
		if (p_neighbors.light_pz) {
			for (int x = 0; x < CX; x++) {
				uint8_t nb_bl = p_neighbors.light_pz[VoxelTerrainGenerator::block_index(x, y, 0)] & 0x0F;
				if (nb_bl > 1) {
					int our_type = (int)p_blocks[VoxelTerrainGenerator::block_index(x, y, CZ - 1)];
					uint8_t our_opacity = p_opacity[our_type];
					if (our_opacity < 15) {
						int new_val = (int)nb_bl - 1 - (int)our_opacity;
						if (new_val > (int)get_blight(p_light_data, x, y, CZ - 1, p_neighbors)) {
							set_blight(p_light_data, x, y, CZ - 1, (uint8_t)new_val);
							queue.push_back({ (int16_t)x, (int16_t)y, (int16_t)(CZ - 1), (uint8_t)new_val });
						}
					}
				}
			}
		}
		if (p_neighbors.light_nz) {
			for (int x = 0; x < CX; x++) {
				uint8_t nb_bl = p_neighbors.light_nz[VoxelTerrainGenerator::block_index(x, y, CZ - 1)] & 0x0F;
				if (nb_bl > 1) {
					int our_type = (int)p_blocks[VoxelTerrainGenerator::block_index(x, y, 0)];
					uint8_t our_opacity = p_opacity[our_type];
					if (our_opacity < 15) {
						int new_val = (int)nb_bl - 1 - (int)our_opacity;
						if (new_val > (int)get_blight(p_light_data, x, y, 0, p_neighbors)) {
							set_blight(p_light_data, x, y, 0, (uint8_t)new_val);
							queue.push_back({ (int16_t)x, (int16_t)y, 0, (uint8_t)new_val });
						}
					}
				}
			}
		}
	}

	// BFS spread.
	static const int dx[6] = { 1, -1, 0, 0, 0, 0 };
	static const int dy[6] = { 0, 0, 0, 0, 1, -1 };
	static const int dz[6] = { 0, 0, 1, -1, 0, 0 };

	uint32_t head = 0;
	while (head < queue.size()) {
		LightNode node = queue[head++];

		for (int dir = 0; dir < 6; dir++) {
			int nx = node.x + dx[dir];
			int ny = node.y + dy[dir];
			int nz = node.z + dz[dir];

			if (nx < 0 || nx >= CX || ny < 0 || ny >= CY || nz < 0 || nz >= CZ) {
				continue;
			}

			int nb_type = (int)p_blocks[VoxelTerrainGenerator::block_index(nx, ny, nz)];
			uint8_t opacity = p_opacity[nb_type];
			if (opacity >= 15) {
				continue;
			}

			int new_level = (int)node.level - 1 - (int)opacity;
			if (new_level <= 0) {
				continue;
			}

			uint8_t current = get_blight(p_light_data, nx, ny, nz, p_neighbors);
			if ((uint8_t)new_level > current) {
				set_blight(p_light_data, nx, ny, nz, (uint8_t)new_level);
				queue.push_back({ (int16_t)nx, (int16_t)ny, (int16_t)nz, (uint8_t)new_level });
			}
		}
	}
}

void VoxelLightMap::remove_and_repropagate_sunlight(uint8_t *p_light_data, const uint16_t *p_blocks, const NeighborData &p_neighbors, const uint8_t *p_opacity) {
	// Full recompute of sunlight: clear all sunlight, then propagate from scratch.
	for (int i = 0; i < CX * CY * CZ; i++) {
		p_light_data[i] = p_light_data[i] & 0x0F; // Keep block light, clear sunlight.
	}
	propagate_sunlight(p_light_data, p_blocks, p_neighbors, p_opacity);
}

void VoxelLightMap::remove_and_repropagate_block_light(uint8_t *p_light_data, const uint16_t *p_blocks, const NeighborData &p_neighbors, const uint8_t *p_opacity, const uint8_t *p_emission) {
	// Full recompute of block light: clear all block light, then propagate from scratch.
	for (int i = 0; i < CX * CY * CZ; i++) {
		p_light_data[i] = p_light_data[i] & 0xF0; // Keep sunlight, clear block light.
	}
	propagate_block_light(p_light_data, p_blocks, p_neighbors, p_opacity, p_emission);
}
