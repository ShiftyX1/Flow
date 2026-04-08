#include "voxel_chunk.h"
#include "voxel_mesher.h"
#include "voxel_world.h"

#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"

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

MeshInstance3D *VoxelChunk::build_mesh(float p_block_size, const Ref<Material> &p_material, const Ref<VoxelBlockRegistry> &p_registry, BaseMaterial3D::TextureFilter p_filter, uint32_t p_alpha_block_flags) {
	Vector<VoxelMesher::MeshSurface> surfaces = VoxelMesher::build_chunk_mesh(blocks, p_block_size, p_registry);

	if (surfaces.size() == 0) {
		return nullptr;
	}

	array_mesh.instantiate();
	for (int i = 0; i < surfaces.size(); i++) {
		array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, surfaces[i].arrays);
		if (surfaces[i].texture.is_valid()) {
			Ref<StandardMaterial3D> mat;
			mat.instantiate();
			mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, surfaces[i].texture);
			mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			mat->set_texture_filter(p_filter);

			bool use_alpha = false;
			int btype = surfaces[i].block_type;
			if (btype == VOXEL_BLOCK_LEAVES && (p_alpha_block_flags & (1 << 0))) {
				use_alpha = true;
			}
			if (use_alpha) {
				mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
				mat->set_alpha_scissor_threshold(0.5f);
			}

			array_mesh->surface_set_material(i, mat);
		} else if (p_material.is_valid()) {
			array_mesh->surface_set_material(i, p_material);
		}
	}

	mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_mesh(array_mesh);

	float world_x = chunk_pos.x * SIZE_X * p_block_size;
	float world_z = chunk_pos.y * SIZE_Z * p_block_size;
	mesh_instance->set_position(Vector3(world_x, 0, world_z));

	return mesh_instance;
}

void VoxelChunk::rebuild_mesh(float p_block_size, const Ref<Material> &p_material, const Ref<VoxelBlockRegistry> &p_registry, BaseMaterial3D::TextureFilter p_filter, uint32_t p_alpha_block_flags) {
	Vector<VoxelMesher::MeshSurface> surfaces = VoxelMesher::build_chunk_mesh(blocks, p_block_size, p_registry);

	if (!mesh_instance) {
		return;
	}

	if (surfaces.size() == 0) {
		mesh_instance->set_mesh(Ref<Mesh>());
		array_mesh.unref();
		// Remove old collision.
		for (int c = mesh_instance->get_child_count() - 1; c >= 0; c--) {
			Node *child = mesh_instance->get_child(c);
			mesh_instance->remove_child(child);
			memdelete(child);
		}
		return;
	}

	array_mesh.instantiate();
	for (int i = 0; i < surfaces.size(); i++) {
		array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, surfaces[i].arrays);
		if (surfaces[i].texture.is_valid()) {
			Ref<StandardMaterial3D> mat;
			mat.instantiate();
			mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, surfaces[i].texture);
			mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			mat->set_texture_filter(p_filter);

			bool use_alpha = false;
			int btype = surfaces[i].block_type;
			if (btype == VOXEL_BLOCK_LEAVES && (p_alpha_block_flags & (1 << 0))) {
				use_alpha = true;
			}
			if (use_alpha) {
				mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
				mat->set_alpha_scissor_threshold(0.5f);
			}

			array_mesh->surface_set_material(i, mat);
		} else if (p_material.is_valid()) {
			array_mesh->surface_set_material(i, p_material);
		}
	}

	mesh_instance->set_mesh(array_mesh);

	// Rebuild collision from updated mesh.
	for (int c = mesh_instance->get_child_count() - 1; c >= 0; c--) {
		Node *child = mesh_instance->get_child(c);
		mesh_instance->remove_child(child);
		memdelete(child);
	}
	PackedVector3Array faces = VoxelWorld::_build_collision_faces(surfaces);
	if (faces.size() > 0) {
		StaticBody3D *sb = memnew(StaticBody3D);
		CollisionShape3D *cs = memnew(CollisionShape3D);
		Ref<ConcavePolygonShape3D> shape;
		shape.instantiate();
		shape->set_faces(faces);
		cs->set_shape(shape);
		sb->add_child(cs);
		mesh_instance->add_child(sb);
	}
}
