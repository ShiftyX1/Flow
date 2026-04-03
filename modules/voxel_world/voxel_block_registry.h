#pragma once

#include "core/io/resource.h"
#include "core/templates/hash_map.h"
#include "scene/resources/texture.h"

// Texture-only registry for engine-defined block types.
// Block logic (solid, transparent, color, name) lives in VoxelBlockData.
// This resource allows assigning textures to blocks from the editor.
class VoxelBlockRegistry : public Resource {
	GDCLASS(VoxelBlockRegistry, Resource);

public:
	struct BlockEntry {
		Ref<Texture2D> texture_top;
		Ref<Texture2D> texture_side;
		Ref<Texture2D> texture_bottom;
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

	const HashMap<int, BlockEntry> &get_blocks_map() const { return blocks; }

	// Populate with entries for all built-in block types (no textures assigned).
	void setup_defaults();

	VoxelBlockRegistry();
};
