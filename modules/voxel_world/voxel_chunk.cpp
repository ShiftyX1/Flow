#include "voxel_chunk.h"
#include "voxel_mesher.h"

VoxelChunk::VoxelChunk() {
}

VoxelChunk::~VoxelChunk() {
}

VoxelBlockType VoxelChunk::get_block(int p_x, int p_y, int p_z) const {
	if (p_x < 0 || p_x >= SIZE_X || p_y < 0 || p_y >= SIZE_Y || p_z < 0 || p_z >= SIZE_Z) {
		return VOXEL_BLOCK_AIR;
	}
	return (VoxelBlockType)blocks[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)];
}

void VoxelChunk::set_block(int p_x, int p_y, int p_z, VoxelBlockType p_type) {
	if (p_x < 0 || p_x >= SIZE_X || p_y < 0 || p_y >= SIZE_Y || p_z < 0 || p_z >= SIZE_Z) {
		return;
	}
	blocks.write[VoxelTerrainGenerator::block_index(p_x, p_y, p_z)] = (uint8_t)p_type;
}

MeshInstance3D *VoxelChunk::build_mesh(float p_block_size, const Ref<Material> &p_material, const Ref<VoxelBlockRegistry> &p_registry) {
	Array arrays = VoxelMesher::build_chunk_mesh(blocks, p_block_size, p_registry);

	if (arrays.size() == 0) {
		return nullptr;
	}

	array_mesh.instantiate();
	array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	if (p_material.is_valid()) {
		array_mesh->surface_set_material(0, p_material);
	}

	mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_mesh(array_mesh);

	float world_x = chunk_pos.x * SIZE_X * p_block_size;
	float world_z = chunk_pos.y * SIZE_Z * p_block_size;
	mesh_instance->set_position(Vector3(world_x, 0, world_z));

	return mesh_instance;
}

void VoxelChunk::rebuild_mesh(float p_block_size, const Ref<Material> &p_material, const Ref<VoxelBlockRegistry> &p_registry) {
	Array arrays = VoxelMesher::build_chunk_mesh(blocks, p_block_size, p_registry);

	if (!mesh_instance) {
		return;
	}

	if (arrays.size() == 0) {
		mesh_instance->set_mesh(Ref<Mesh>());
		array_mesh.unref();
		return;
	}

	array_mesh.instantiate();
	array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

	if (p_material.is_valid()) {
		array_mesh->surface_set_material(0, p_material);
	}

	mesh_instance->set_mesh(array_mesh);
}
