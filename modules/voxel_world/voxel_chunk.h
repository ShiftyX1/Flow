#pragma once

#include "voxel_block_registry.h"
#include "voxel_mesher.h"
#include "voxel_terrain_generator.h"

#include "core/math/vector2i.h"
#include "core/templates/vector.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/material.h"

class VoxelChunk {
public:
	static const int SIZE_X = VoxelTerrainGenerator::CHUNK_SIZE_X;
	static const int SIZE_Y = VoxelTerrainGenerator::CHUNK_SIZE_Y;
	static const int SIZE_Z = VoxelTerrainGenerator::CHUNK_SIZE_Z;

private:
	Vector<uint16_t> blocks;
	Vector<uint8_t> light_data; // Per-block light: high nibble = sunlight (0-15), low nibble = block light (0-15).
	Vector2i chunk_pos;
	MeshInstance3D *mesh_instance = nullptr;
	Ref<ArrayMesh> array_mesh;

public:
	VoxelChunk();
	~VoxelChunk();

	void set_chunk_pos(const Vector2i &p_pos) { chunk_pos = p_pos; }
	Vector2i get_chunk_pos() const { return chunk_pos; }

	void set_blocks(const Vector<uint16_t> &p_blocks) { blocks = p_blocks; }
	const Vector<uint16_t> &get_blocks() const { return blocks; }

	void set_light_data(const Vector<uint8_t> &p_light) { light_data = p_light; }
	const Vector<uint8_t> &get_light_data() const { return light_data; }
	Vector<uint8_t> &get_light_data_rw() { return light_data; }

	// Initialize light_data to the same size as blocks, all zeroed.
	void ensure_light_data() {
		if (light_data.size() != blocks.size()) {
			light_data.resize(blocks.size());
			memset(light_data.ptrw(), 0, light_data.size());
		}
	}

	uint8_t get_sunlight(int p_x, int p_y, int p_z) const {
		if (p_x < 0 || p_x >= SIZE_X || p_y < 0 || p_y >= SIZE_Y || p_z < 0 || p_z >= SIZE_Z) {
			return 0;
		}
		if (light_data.size() == 0) {
			return 0;
		}
		return (light_data[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)] >> 4) & 0x0F;
	}

	void set_sunlight(int p_x, int p_y, int p_z, uint8_t p_val) {
		if (p_x < 0 || p_x >= SIZE_X || p_y < 0 || p_y >= SIZE_Y || p_z < 0 || p_z >= SIZE_Z) {
			return;
		}
		ensure_light_data();
		int idx = VoxelTerrainGenerator::block_index(p_x, p_y, p_z);
		light_data.write[idx] = (light_data[idx] & 0x0F) | ((p_val & 0x0F) << 4);
	}

	uint8_t get_block_light(int p_x, int p_y, int p_z) const {
		if (p_x < 0 || p_x >= SIZE_X || p_y < 0 || p_y >= SIZE_Y || p_z < 0 || p_z >= SIZE_Z) {
			return 0;
		}
		if (light_data.size() == 0) {
			return 0;
		}
		return light_data[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)] & 0x0F;
	}

	void set_block_light(int p_x, int p_y, int p_z, uint8_t p_val) {
		if (p_x < 0 || p_x >= SIZE_X || p_y < 0 || p_y >= SIZE_Y || p_z < 0 || p_z >= SIZE_Z) {
			return;
		}
		ensure_light_data();
		int idx = VoxelTerrainGenerator::block_index(p_x, p_y, p_z);
		light_data.write[idx] = (light_data[idx] & 0xF0) | (p_val & 0x0F);
	}

	int get_block(int p_x, int p_y, int p_z) const;
	void set_block(int p_x, int p_y, int p_z, int p_type);

	MeshInstance3D *build_mesh(float p_block_size, const Ref<Material> &p_material, const Ref<VoxelBlockRegistry> &p_registry = Ref<VoxelBlockRegistry>(), BaseMaterial3D::TextureFilter p_filter = BaseMaterial3D::TEXTURE_FILTER_NEAREST, const VoxelMesher::NeighborBlocks &p_neighbors = VoxelMesher::NeighborBlocks());
	void rebuild_mesh(float p_block_size, const Ref<Material> &p_material, const Ref<VoxelBlockRegistry> &p_registry = Ref<VoxelBlockRegistry>(), BaseMaterial3D::TextureFilter p_filter = BaseMaterial3D::TEXTURE_FILTER_NEAREST, const VoxelMesher::NeighborBlocks &p_neighbors = VoxelMesher::NeighborBlocks());

	MeshInstance3D *get_mesh_instance() const { return mesh_instance; }
	void set_mesh_instance(MeshInstance3D *p_mi) { mesh_instance = p_mi; }
};
