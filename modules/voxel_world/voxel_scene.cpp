#include "voxel_scene.h"

#include "voxel_mesher.h"

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"

VoxelScene::VoxelScene() {
}

VoxelScene::~VoxelScene() {
	_disconnect_data();
	_disconnect_registry();
}

// ---- Property setters ----

void VoxelScene::set_voxel_data(const Ref<VoxelSceneData> &p_data) {
	if (voxel_data == p_data) {
		return;
	}
	_disconnect_data();
	voxel_data = p_data;
	_connect_data();
	if (auto_rebuild) {
		rebuild_mesh();
	}
	update_configuration_warnings();
}

void VoxelScene::set_block_registry(const Ref<VoxelBlockRegistry> &p_registry) {
	if (block_registry == p_registry) {
		return;
	}
	_disconnect_registry();
	block_registry = p_registry;
	_connect_registry();
	if (auto_rebuild) {
		rebuild_mesh();
	}
	update_configuration_warnings();
}

void VoxelScene::set_block_size(float p_size) {
	float v = MAX(0.01f, p_size);
	if (block_size == v) {
		return;
	}
	block_size = v;
	if (auto_rebuild) {
		rebuild_mesh();
	}
}

void VoxelScene::set_texture_filter(TextureFilter p_filter) {
	if (texture_filter == p_filter) {
		return;
	}
	texture_filter = p_filter;
	if (auto_rebuild) {
		rebuild_mesh();
	}
}

void VoxelScene::set_bake_ao(bool p_bake) {
	if (bake_ao == p_bake) {
		return;
	}
	bake_ao = p_bake;
	if (auto_rebuild) {
		rebuild_mesh();
	}
}

void VoxelScene::set_auto_rebuild(bool p_auto) {
	auto_rebuild = p_auto;
}

// ---- Signal hookup ----

void VoxelScene::_connect_data() {
	if (voxel_data.is_valid() && !_data_signal_connected) {
		voxel_data->connect(SNAME("changed"), callable_mp(this, &VoxelScene::_on_data_changed));
		_data_signal_connected = true;
	}
}
void VoxelScene::_disconnect_data() {
	if (voxel_data.is_valid() && _data_signal_connected) {
		voxel_data->disconnect(SNAME("changed"), callable_mp(this, &VoxelScene::_on_data_changed));
	}
	_data_signal_connected = false;
}
void VoxelScene::_connect_registry() {
	if (block_registry.is_valid() && !_registry_signal_connected) {
		block_registry->connect(SNAME("changed"), callable_mp(this, &VoxelScene::_on_registry_changed));
		_registry_signal_connected = true;
	}
}
void VoxelScene::_disconnect_registry() {
	if (block_registry.is_valid() && _registry_signal_connected) {
		block_registry->disconnect(SNAME("changed"), callable_mp(this, &VoxelScene::_on_registry_changed));
	}
	_registry_signal_connected = false;
}

void VoxelScene::_on_data_changed() {
	if (auto_rebuild) {
		rebuild_mesh();
	}
}
void VoxelScene::_on_registry_changed() {
	if (auto_rebuild) {
		rebuild_mesh();
	}
}

// ---- Mesh management ----

void VoxelScene::_clear_mesh_instance() {
	if (mesh_instance) {
		if (mesh_instance->get_parent() == this) {
			remove_child(mesh_instance);
		}
		memdelete(mesh_instance);
		mesh_instance = nullptr;
	}
}

void VoxelScene::_apply_surfaces(const Vector<VoxelMesher::MeshSurface> &p_surfaces) {
	_clear_mesh_instance();
	if (p_surfaces.is_empty()) {
		return;
	}

	Ref<ArrayMesh> array_mesh;
	array_mesh.instantiate();

	for (int s = 0; s < p_surfaces.size(); s++) {
		uint64_t fmt_flags = p_surfaces[s].custom_format_flags;
		array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, p_surfaces[s].arrays, Array(), Dictionary(), fmt_flags);

		if (p_surfaces[s].texture.is_valid()) {
			Ref<StandardMaterial3D> mat;
			mat.instantiate();
			mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, p_surfaces[s].texture);
			mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			mat->set_texture_filter(texture_filter);
			int btype = p_surfaces[s].block_type;
			if (btype >= 0 && block_registry.is_valid() && block_registry->is_uses_alpha(btype)) {
				mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
				mat->set_alpha_scissor_threshold(0.5f);
			}
			array_mesh->surface_set_material(s, mat);
		} else {
			Ref<StandardMaterial3D> mat;
			mat.instantiate();
			mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			array_mesh->surface_set_material(s, mat);
		}
	}

	mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_mesh(array_mesh);
	add_child(mesh_instance, false, INTERNAL_MODE_FRONT);
}

void VoxelScene::rebuild_mesh() {
	if (voxel_data.is_null()) {
		_clear_mesh_instance();
		return;
	}

	// Make sure the registry is finalized so the cache arrays are populated.
	if (block_registry.is_valid() && !block_registry->is_finalized()) {
		block_registry->finalize();
	}

	VoxelMesher::VolumeMeshOptions opts;
	opts.bake_ao = bake_ao;
	Vector<VoxelMesher::MeshSurface> surfaces = VoxelMesher::build_volume_mesh(
			voxel_data->get_blocks_array(), voxel_data->get_size(), block_size, block_registry, opts);
	_apply_surfaces(surfaces);
}

// ---- Read-only runtime API ----

int VoxelScene::get_block(int x, int y, int z) const {
	return voxel_data.is_valid() ? voxel_data->get_block(x, y, z) : 0;
}

Vector3i VoxelScene::get_size() const {
	return voxel_data.is_valid() ? voxel_data->get_size() : Vector3i();
}

Vector3i VoxelScene::local_to_voxel(const Vector3 &p_local) const {
	float bs = MAX(0.0001f, block_size);
	return Vector3i((int)Math::floor(p_local.x / bs), (int)Math::floor(p_local.y / bs), (int)Math::floor(p_local.z / bs));
}
Vector3 VoxelScene::voxel_to_local(const Vector3i &p_voxel) const {
	return Vector3(p_voxel.x * block_size, p_voxel.y * block_size, p_voxel.z * block_size);
}

PackedStringArray VoxelScene::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();

	if (voxel_data.is_null()) {
		warnings.push_back(RTR("Assign or create a VoxelSceneData resource before editing this VoxelScene."));
	}
	if (block_registry.is_null()) {
		warnings.push_back(RTR("Assign a VoxelBlockRegistry resource to choose and render block types."));
	}

	return warnings;
}

// ---- Notifications ----

void VoxelScene::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_ready_done = true;
			rebuild_mesh();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_clear_mesh_instance();
		} break;
		default:
			break;
	}
}

void VoxelScene::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_voxel_data", "data"), &VoxelScene::set_voxel_data);
	ClassDB::bind_method(D_METHOD("get_voxel_data"), &VoxelScene::get_voxel_data);
	ClassDB::bind_method(D_METHOD("set_block_registry", "registry"), &VoxelScene::set_block_registry);
	ClassDB::bind_method(D_METHOD("get_block_registry"), &VoxelScene::get_block_registry);
	ClassDB::bind_method(D_METHOD("set_block_size", "size"), &VoxelScene::set_block_size);
	ClassDB::bind_method(D_METHOD("get_block_size"), &VoxelScene::get_block_size);
	ClassDB::bind_method(D_METHOD("set_texture_filter", "filter"), &VoxelScene::set_texture_filter);
	ClassDB::bind_method(D_METHOD("get_texture_filter"), &VoxelScene::get_texture_filter);
	ClassDB::bind_method(D_METHOD("set_bake_ao", "bake"), &VoxelScene::set_bake_ao);
	ClassDB::bind_method(D_METHOD("get_bake_ao"), &VoxelScene::get_bake_ao);
	ClassDB::bind_method(D_METHOD("set_auto_rebuild", "enabled"), &VoxelScene::set_auto_rebuild);
	ClassDB::bind_method(D_METHOD("get_auto_rebuild"), &VoxelScene::get_auto_rebuild);

	ClassDB::bind_method(D_METHOD("rebuild_mesh"), &VoxelScene::rebuild_mesh);
	ClassDB::bind_method(D_METHOD("get_block", "x", "y", "z"), &VoxelScene::get_block);
	ClassDB::bind_method(D_METHOD("get_block_v", "pos"), &VoxelScene::get_block_v);
	ClassDB::bind_method(D_METHOD("get_size"), &VoxelScene::get_size);
	ClassDB::bind_method(D_METHOD("local_to_voxel", "local_pos"), &VoxelScene::local_to_voxel);
	ClassDB::bind_method(D_METHOD("voxel_to_local", "voxel_pos"), &VoxelScene::voxel_to_local);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "voxel_data", PROPERTY_HINT_RESOURCE_TYPE, "VoxelSceneData"),
			"set_voxel_data", "get_voxel_data");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "block_registry", PROPERTY_HINT_RESOURCE_TYPE, "VoxelBlockRegistry"),
			"set_block_registry", "get_block_registry");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "block_size", PROPERTY_HINT_RANGE, "0.01,10.0,0.01"),
			"set_block_size", "get_block_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_filter", PROPERTY_HINT_ENUM,
						 "Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Aniso,Linear Mipmap Aniso"),
			"set_texture_filter", "get_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bake_ao"), "set_bake_ao", "get_bake_ao");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_rebuild"), "set_auto_rebuild", "get_auto_rebuild");
}
