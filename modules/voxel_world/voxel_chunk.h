#pragma once

#include "voxel_block_data.h"
#include "voxel_block_registry.h"
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
	Vector<uint8_t> blocks;
	Vector2i chunk_pos;
	MeshInstance3D *mesh_instance = nullptr;
	Ref<ArrayMesh> array_mesh;

public:
	VoxelChunk();
	~VoxelChunk();

	void set_chunk_pos(const Vector2i &p_pos) { chunk_pos = p_pos; }
	Vector2i get_chunk_pos() const { return chunk_pos; }

	void set_blocks(const Vector<uint8_t> &p_blocks) { blocks = p_blocks; }
	const Vector<uint8_t> &get_blocks() const { return blocks; }

	VoxelBlockType get_block(int p_x, int p_y, int p_z) const;
	void set_block(int p_x, int p_y, int p_z, VoxelBlockType p_type);

	MeshInstance3D *build_mesh(float p_block_size, const Ref<Material> &p_material, const Ref<VoxelBlockRegistry> &p_registry = Ref<VoxelBlockRegistry>());
	void rebuild_mesh(float p_block_size, const Ref<Material> &p_material, const Ref<VoxelBlockRegistry> &p_registry = Ref<VoxelBlockRegistry>());

	MeshInstance3D *get_mesh_instance() const { return mesh_instance; }
};
