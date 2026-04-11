#include "voxel_block_registry.h"

#include "voxel_block_data.h"

#include "core/object/class_db.h"

VoxelBlockRegistry::VoxelBlockRegistry() {
}

// --- Dynamic property serialization (MeshLibrary pattern) ---

bool VoxelBlockRegistry::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name;
	if (prop_name.begins_with("block/")) {
		int idx = prop_name.get_slicec('/', 1).to_int();
		String what = prop_name.get_slicec('/', 2);
		if (!blocks.has(idx)) {
			create_block(idx);
		}

		if (what == "texture_top") {
			set_block_texture_top(idx, p_value);
		} else if (what == "texture_side") {
			set_block_texture_side(idx, p_value);
		} else if (what == "texture_bottom") {
			set_block_texture_bottom(idx, p_value);
		} else if (what == "shader_material") {
			set_block_shader_material(idx, p_value);
		} else if (what == "mesh_height") {
			set_block_mesh_height(idx, p_value);
		} else if (what == "collision_height") {
			set_block_collision_height(idx, p_value);
		} else {
			return false;
		}
		return true;
	}
	return false;
}

bool VoxelBlockRegistry::_get(const StringName &p_name, Variant &r_ret) const {
	String prop_name = p_name;
	if (!prop_name.begins_with("block/")) {
		return false;
	}
	int idx = prop_name.get_slicec('/', 1).to_int();
	if (!blocks.has(idx)) {
		return false;
	}
	String what = prop_name.get_slicec('/', 2);

	if (what == "texture_top") {
		r_ret = get_block_texture_top(idx);
	} else if (what == "texture_side") {
		r_ret = get_block_texture_side(idx);
	} else if (what == "texture_bottom") {
		r_ret = get_block_texture_bottom(idx);
	} else if (what == "shader_material") {
		r_ret = get_block_shader_material(idx);
	} else if (what == "mesh_height") {
		r_ret = get_block_mesh_height(idx);
	} else if (what == "collision_height") {
		r_ret = get_block_collision_height(idx);
	} else {
		return false;
	}
	return true;
}

void VoxelBlockRegistry::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const KeyValue<int, BlockEntry> &E : blocks) {
		String prefix = vformat("block/%d/", E.key);
		// Show the engine block name as a hint in the property group.
		String block_label;
		if (E.key >= 0 && E.key < VOXEL_BLOCK_TYPE_MAX) {
			block_label = VoxelBlockData::get_block_name((VoxelBlockType)E.key);
		} else {
			block_label = "Custom";
		}
		p_list->push_back(PropertyInfo(Variant::NIL, vformat("Block %d (%s)", E.key, block_label), PROPERTY_HINT_NONE, prefix, PROPERTY_USAGE_GROUP));
		p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "texture_top", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "texture_side", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "texture_bottom", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "shader_material", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + "mesh_height", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + "collision_height", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"));
	}
}

// --- Bind methods ---

void VoxelBlockRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("create_block", "id"), &VoxelBlockRegistry::create_block);
	ClassDB::bind_method(D_METHOD("remove_block", "id"), &VoxelBlockRegistry::remove_block);
	ClassDB::bind_method(D_METHOD("has_block", "id"), &VoxelBlockRegistry::has_block);
	ClassDB::bind_method(D_METHOD("get_block_list"), &VoxelBlockRegistry::get_block_list);
	ClassDB::bind_method(D_METHOD("get_block_count"), &VoxelBlockRegistry::get_block_count);

	ClassDB::bind_method(D_METHOD("set_block_texture_top", "id", "texture"), &VoxelBlockRegistry::set_block_texture_top);
	ClassDB::bind_method(D_METHOD("get_block_texture_top", "id"), &VoxelBlockRegistry::get_block_texture_top);

	ClassDB::bind_method(D_METHOD("set_block_texture_side", "id", "texture"), &VoxelBlockRegistry::set_block_texture_side);
	ClassDB::bind_method(D_METHOD("get_block_texture_side", "id"), &VoxelBlockRegistry::get_block_texture_side);

	ClassDB::bind_method(D_METHOD("set_block_texture_bottom", "id", "texture"), &VoxelBlockRegistry::set_block_texture_bottom);
	ClassDB::bind_method(D_METHOD("get_block_texture_bottom", "id"), &VoxelBlockRegistry::get_block_texture_bottom);

	ClassDB::bind_method(D_METHOD("block_has_texture", "id"), &VoxelBlockRegistry::block_has_texture);

	ClassDB::bind_method(D_METHOD("set_block_shader_material", "id", "material"), &VoxelBlockRegistry::set_block_shader_material);
	ClassDB::bind_method(D_METHOD("get_block_shader_material", "id"), &VoxelBlockRegistry::get_block_shader_material);
	ClassDB::bind_method(D_METHOD("block_has_shader", "id"), &VoxelBlockRegistry::block_has_shader);

	ClassDB::bind_method(D_METHOD("set_block_mesh_height", "id", "height"), &VoxelBlockRegistry::set_block_mesh_height);
	ClassDB::bind_method(D_METHOD("get_block_mesh_height", "id"), &VoxelBlockRegistry::get_block_mesh_height);

	ClassDB::bind_method(D_METHOD("set_block_collision_height", "id", "height"), &VoxelBlockRegistry::set_block_collision_height);
	ClassDB::bind_method(D_METHOD("get_block_collision_height", "id"), &VoxelBlockRegistry::get_block_collision_height);

	ClassDB::bind_method(D_METHOD("setup_defaults"), &VoxelBlockRegistry::setup_defaults);
}

// --- Implementation ---

void VoxelBlockRegistry::create_block(int p_id) {
	if (blocks.has(p_id)) {
		return;
	}
	blocks[p_id] = BlockEntry();
	emit_changed();
}

void VoxelBlockRegistry::remove_block(int p_id) {
	if (!blocks.has(p_id)) {
		return;
	}
	blocks.erase(p_id);
	emit_changed();
}

bool VoxelBlockRegistry::has_block(int p_id) const {
	return blocks.has(p_id);
}

PackedInt32Array VoxelBlockRegistry::get_block_list() const {
	PackedInt32Array list;
	for (const KeyValue<int, BlockEntry> &E : blocks) {
		list.push_back(E.key);
	}
	return list;
}

int VoxelBlockRegistry::get_block_count() const {
	return blocks.size();
}

void VoxelBlockRegistry::set_block_texture_top(int p_id, const Ref<Texture2D> &p_texture) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].texture_top = p_texture;
	emit_changed();
}

Ref<Texture2D> VoxelBlockRegistry::get_block_texture_top(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), Ref<Texture2D>());
	return blocks[p_id].texture_top;
}

void VoxelBlockRegistry::set_block_texture_side(int p_id, const Ref<Texture2D> &p_texture) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].texture_side = p_texture;
	emit_changed();
}

Ref<Texture2D> VoxelBlockRegistry::get_block_texture_side(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), Ref<Texture2D>());
	return blocks[p_id].texture_side;
}

void VoxelBlockRegistry::set_block_texture_bottom(int p_id, const Ref<Texture2D> &p_texture) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].texture_bottom = p_texture;
	emit_changed();
}

Ref<Texture2D> VoxelBlockRegistry::get_block_texture_bottom(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), Ref<Texture2D>());
	return blocks[p_id].texture_bottom;
}

Ref<Texture2D> VoxelBlockRegistry::get_block_texture_for_face(int p_id, const Vector3 &p_normal) const {
	if (!blocks.has(p_id)) {
		return Ref<Texture2D>();
	}
	const BlockEntry &entry = blocks[p_id];

	if (p_normal.y > 0.5f) {
		// Top face.
		return entry.texture_top;
	} else if (p_normal.y < -0.5f) {
		// Bottom face.
		return entry.texture_bottom;
	} else {
		// Side face.
		return entry.texture_side;
	}
	return Ref<Texture2D>();
}

bool VoxelBlockRegistry::block_has_texture(int p_id) const {
	if (!blocks.has(p_id)) {
		return false;
	}
	const BlockEntry &entry = blocks[p_id];
	return entry.texture_top.is_valid() || entry.texture_side.is_valid() || entry.texture_bottom.is_valid();
}

void VoxelBlockRegistry::set_block_shader_material(int p_id, const Ref<ShaderMaterial> &p_material) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].shader_material = p_material;
	emit_changed();
}

Ref<ShaderMaterial> VoxelBlockRegistry::get_block_shader_material(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), Ref<ShaderMaterial>());
	return blocks[p_id].shader_material;
}

bool VoxelBlockRegistry::block_has_shader(int p_id) const {
	if (!blocks.has(p_id)) {
		return false;
	}
	return blocks[p_id].shader_material.is_valid();
}

void VoxelBlockRegistry::set_block_mesh_height(int p_id, float p_height) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].mesh_height = CLAMP(p_height, 0.0f, 1.0f);
	emit_changed();
}

float VoxelBlockRegistry::get_block_mesh_height(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), 1.0f);
	return blocks[p_id].mesh_height;
}

void VoxelBlockRegistry::set_block_collision_height(int p_id, float p_height) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].collision_height = CLAMP(p_height, 0.0f, 1.0f);
	emit_changed();
}

float VoxelBlockRegistry::get_block_collision_height(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), 1.0f);
	return blocks[p_id].collision_height;
}

void VoxelBlockRegistry::setup_defaults() {
	// Create entries for all engine-defined block types (texture slots empty).
	for (int i = 0; i < VOXEL_BLOCK_TYPE_MAX; i++) {
		create_block(i);
	}
	// Water sits at 87.5% height so shader wave displacement stays below adjacent solid blocks.
	blocks[VOXEL_BLOCK_WATER].mesh_height = 0.875f;
	blocks[VOXEL_BLOCK_WATER].collision_height = 0.875f;
}
