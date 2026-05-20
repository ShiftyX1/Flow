#pragma once

#include "voxel_block_registry.h"
#include "voxel_mesher.h"
#include "voxel_scene_data.h"

#include "core/object/ref_counted.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/material.h"

// VoxelScene — finite voxel volume rendered as a single mesh.
//
// Designed for cinematics, main menus, hand-crafted decorations, etc.
// Intentionally simpler than VoxelWorld: no streaming, no chunks, no BFS sunlight,
// no procedural generation, no runtime mutation API. AO is baked into vertex colors.
//
// Editing happens via VoxelSceneEditorPlugin (3D viewport tools + palette dock).
class VoxelScene : public Node3D {
	GDCLASS(VoxelScene, Node3D);

public:
	using TextureFilter = BaseMaterial3D::TextureFilter;

private:
	Ref<VoxelSceneData> voxel_data;
	Ref<VoxelBlockRegistry> block_registry;
	float block_size = 1.0f;
	TextureFilter texture_filter = BaseMaterial3D::TEXTURE_FILTER_NEAREST;
	bool bake_ao = true;
	bool auto_rebuild = true;

	MeshInstance3D *mesh_instance = nullptr;
	bool _data_signal_connected = false;
	bool _registry_signal_connected = false;
	bool _ready_done = false;

	void _on_data_changed();
	void _on_registry_changed();
	void _connect_data();
	void _disconnect_data();
	void _connect_registry();
	void _disconnect_registry();
	void _apply_surfaces(const Vector<VoxelMesher::MeshSurface> &p_surfaces);
	void _clear_mesh_instance();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void set_voxel_data(const Ref<VoxelSceneData> &p_data);
	Ref<VoxelSceneData> get_voxel_data() const { return voxel_data; }

	void set_block_registry(const Ref<VoxelBlockRegistry> &p_registry);
	Ref<VoxelBlockRegistry> get_block_registry() const { return block_registry; }

	void set_block_size(float p_size);
	float get_block_size() const { return block_size; }

	void set_texture_filter(TextureFilter p_filter);
	TextureFilter get_texture_filter() const { return texture_filter; }

	void set_bake_ao(bool p_bake);
	bool get_bake_ao() const { return bake_ao; }

	void set_auto_rebuild(bool p_auto);
	bool get_auto_rebuild() const { return auto_rebuild; }

	// Build (or rebuild) the mesh from voxel_data + block_registry.
	void rebuild_mesh();

	// Read-only runtime API.
	int get_block(int x, int y, int z) const;
	int get_block_v(const Vector3i &p_pos) const { return get_block(p_pos.x, p_pos.y, p_pos.z); }
	Vector3i get_size() const;

	// Convert local (Node3D-relative) point to voxel cell coordinates.
	Vector3i local_to_voxel(const Vector3 &p_local) const;
	Vector3 voxel_to_local(const Vector3i &p_voxel) const;

	PackedStringArray get_configuration_warnings() const override;

	VoxelScene();
	~VoxelScene();
};
