#pragma once

#include "core/io/resource.h"
#include "core/templates/hash_map.h"
#include "scene/resources/material.h"
#include "scene/resources/texture.h"

// Registry for engine-defined block types: per-face textures and optional ShaderMaterial.
// Block logic (solid, transparent, color, name) lives in VoxelBlockData.
// This resource allows assigning textures and shaders to blocks from the editor.
class VoxelBlockRegistry : public Resource {
	GDCLASS(VoxelBlockRegistry, Resource);

public:
	struct BlockEntry {
		Ref<Texture2D> texture_top;
		Ref<Texture2D> texture_side;
		Ref<Texture2D> texture_bottom;
		Ref<ShaderMaterial> shader_material;
		// Normalized fractions of block_size (0.0–1.0). 1.0 = full block height.
		float mesh_height = 1.0f;       // Visual top-surface height.
		float collision_height = 1.0f;  // Physics AABB height.
	};

private:
	HashMap<int, BlockEntry> blocks;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	static void _bind_methods();

public:
	void create_block(int p_id);
	void remove_block(int p_id);
	bool has_block(int p_id) const;
	PackedInt32Array get_block_list() const;
	int get_block_count() const;

	void set_block_texture_top(int p_id, const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_block_texture_top(int p_id) const;

	void set_block_texture_side(int p_id, const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_block_texture_side(int p_id) const;

	void set_block_texture_bottom(int p_id, const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_block_texture_bottom(int p_id) const;

	// Returns texture for a given face direction. Falls back: side->top, bottom->top, top->side etc.
	Ref<Texture2D> get_block_texture_for_face(int p_id, const Vector3 &p_normal) const;

	bool block_has_texture(int p_id) const;

	void set_block_shader_material(int p_id, const Ref<ShaderMaterial> &p_material);
	Ref<ShaderMaterial> get_block_shader_material(int p_id) const;
	bool block_has_shader(int p_id) const;

	void set_block_mesh_height(int p_id, float p_height);
	float get_block_mesh_height(int p_id) const;

	void set_block_collision_height(int p_id, float p_height);
	float get_block_collision_height(int p_id) const;

	const HashMap<int, BlockEntry> &get_blocks_map() const { return blocks; }

	// Populate with entries for all built-in block types (no textures assigned).
	void setup_defaults();

	VoxelBlockRegistry();
};
