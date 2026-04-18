#include "voxel_block_registry.h"

#include "voxel_block_data.h"

#include "core/object/class_db.h"

VoxelBlockRegistry::VoxelBlockRegistry() {
}

VoxelBlockRegistry::BlockEntry VoxelBlockRegistry::_get_engine_default_entry(int p_id) {
	BlockEntry def;
	switch (p_id) {
		case VOXEL_BLOCK_AIR:
			def.color = Color(0.0f, 0.0f, 0.0f, 0.0f);
			def.solid = false;
			def.transparent = true;
			def.light_opacity = 0;
			break;
		case VOXEL_BLOCK_GRASS:
			def.color = Color(0.36f, 0.63f, 0.20f);
			break;
		case VOXEL_BLOCK_DIRT:
			def.color = Color(0.55f, 0.37f, 0.24f);
			break;
		case VOXEL_BLOCK_STONE:
			def.color = Color(0.50f, 0.50f, 0.50f);
			break;
		case VOXEL_BLOCK_SAND:
			def.color = Color(0.86f, 0.82f, 0.55f);
			break;
		case VOXEL_BLOCK_WATER:
			def.color = Color(0.24f, 0.46f, 0.72f, 0.7f);
			def.solid = false;
			def.transparent = true;
			def.light_opacity = 2;
			def.mesh_height = 0.875f;
			def.collision_height = 0.875f;
			break;
		case VOXEL_BLOCK_SNOW:
			def.color = Color(0.93f, 0.93f, 0.97f);
			break;
		case VOXEL_BLOCK_WOOD:
			def.color = Color(0.40f, 0.26f, 0.13f);
			break;
		case VOXEL_BLOCK_LEAVES:
			def.color = Color(0.16f, 0.38f, 0.14f);
			def.light_opacity = 1;
			break;
		case VOXEL_BLOCK_BEDROCK:
			def.color = Color(0.20f, 0.20f, 0.20f);
			break;
		case VOXEL_BLOCK_TORCH:
			def.color = Color(1.0f, 0.8f, 0.3f, 1.0f);
			def.solid = false;
			def.transparent = true;
			def.light_opacity = 0;
			def.emission = 14;
			def.light_color = Color(1.0f, 0.6f, 0.2f);
			break;
		default:
			break;
	}
	return def;
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
		} else if (what == "color") {
			set_block_color(idx, p_value);
		} else if (what == "solid") {
			set_block_solid(idx, p_value);
		} else if (what == "transparent") {
			set_block_transparent(idx, p_value);
		} else if (what == "light_opacity") {
			set_block_light_opacity(idx, (int)p_value);
		} else if (what == "emission") {
			set_block_emission(idx, (int)p_value);
		} else if (what == "light_color") {
			set_block_light_color(idx, p_value);
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
	} else if (what == "color") {
		r_ret = get_block_color(idx);
	} else if (what == "solid") {
		r_ret = get_block_solid(idx);
	} else if (what == "transparent") {
		r_ret = get_block_transparent(idx);
	} else if (what == "light_opacity") {
		r_ret = get_block_light_opacity(idx);
	} else if (what == "emission") {
		r_ret = get_block_emission(idx);
	} else if (what == "light_color") {
		r_ret = get_block_light_color(idx);
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
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + "mesh_height", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + "collision_height", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"));

		// Physics/lighting fields: only save when they differ from engine defaults.
		// This prevents stale .tscn files from overwriting correct create_block() defaults
		// for blocks like AIR, WATER, TORCH.
		BlockEntry def = _get_engine_default_entry(E.key);
		const BlockEntry &cur = E.value;

		const uint32_t storage_flag = (PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_STORAGE);
		const uint32_t no_storage_flag = (PROPERTY_USAGE_EDITOR);

		p_list->push_back(PropertyInfo(Variant::COLOR, prefix + "color", PROPERTY_HINT_NONE, "",
				(cur.color != def.color) ? storage_flag : no_storage_flag));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + "solid", PROPERTY_HINT_NONE, "",
				(cur.solid != def.solid) ? storage_flag : no_storage_flag));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + "transparent", PROPERTY_HINT_NONE, "",
				(cur.transparent != def.transparent) ? storage_flag : no_storage_flag));
		p_list->push_back(PropertyInfo(Variant::INT, prefix + "light_opacity", PROPERTY_HINT_RANGE, "0,15,1",
				(cur.light_opacity != def.light_opacity) ? storage_flag : no_storage_flag));
		p_list->push_back(PropertyInfo(Variant::INT, prefix + "emission", PROPERTY_HINT_RANGE, "0,15,1",
				(cur.emission != def.emission) ? storage_flag : no_storage_flag));
		p_list->push_back(PropertyInfo(Variant::COLOR, prefix + "light_color", PROPERTY_HINT_NONE, "",
				(cur.light_color != def.light_color) ? storage_flag : no_storage_flag));
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

	ClassDB::bind_method(D_METHOD("set_block_color", "id", "color"), &VoxelBlockRegistry::set_block_color);
	ClassDB::bind_method(D_METHOD("get_block_color", "id"), &VoxelBlockRegistry::get_block_color);

	ClassDB::bind_method(D_METHOD("set_block_solid", "id", "solid"), &VoxelBlockRegistry::set_block_solid);
	ClassDB::bind_method(D_METHOD("get_block_solid", "id"), &VoxelBlockRegistry::get_block_solid);

	ClassDB::bind_method(D_METHOD("set_block_transparent", "id", "transparent"), &VoxelBlockRegistry::set_block_transparent);
	ClassDB::bind_method(D_METHOD("get_block_transparent", "id"), &VoxelBlockRegistry::get_block_transparent);

	ClassDB::bind_method(D_METHOD("set_block_light_opacity", "id", "opacity"), &VoxelBlockRegistry::set_block_light_opacity);
	ClassDB::bind_method(D_METHOD("get_block_light_opacity", "id"), &VoxelBlockRegistry::get_block_light_opacity);

	ClassDB::bind_method(D_METHOD("set_block_emission", "id", "emission"), &VoxelBlockRegistry::set_block_emission);
	ClassDB::bind_method(D_METHOD("get_block_emission", "id"), &VoxelBlockRegistry::get_block_emission);

	ClassDB::bind_method(D_METHOD("set_block_light_color", "id", "color"), &VoxelBlockRegistry::set_block_light_color);
	ClassDB::bind_method(D_METHOD("get_block_light_color", "id"), &VoxelBlockRegistry::get_block_light_color);

	ClassDB::bind_method(D_METHOD("setup_defaults"), &VoxelBlockRegistry::setup_defaults);
}

// --- Implementation ---

void VoxelBlockRegistry::create_block(int p_id) {
	if (blocks.has(p_id)) {
		return;
	}
	blocks[p_id] = _get_engine_default_entry(p_id);
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

void VoxelBlockRegistry::set_block_color(int p_id, const Color &p_color) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].color = p_color;
	emit_changed();
}

Color VoxelBlockRegistry::get_block_color(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), Color(1, 1, 1));
	return blocks[p_id].color;
}

void VoxelBlockRegistry::set_block_solid(int p_id, bool p_solid) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].solid = p_solid;
	emit_changed();
}

bool VoxelBlockRegistry::get_block_solid(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), true);
	return blocks[p_id].solid;
}

void VoxelBlockRegistry::set_block_transparent(int p_id, bool p_transparent) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].transparent = p_transparent;
	emit_changed();
}

bool VoxelBlockRegistry::get_block_transparent(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), false);
	return blocks[p_id].transparent;
}

void VoxelBlockRegistry::set_block_light_opacity(int p_id, int p_opacity) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].light_opacity = (uint8_t)CLAMP(p_opacity, 0, 15);
	emit_changed();
}

int VoxelBlockRegistry::get_block_light_opacity(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), 15);
	return blocks[p_id].light_opacity;
}

void VoxelBlockRegistry::set_block_emission(int p_id, int p_emission) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].emission = (uint8_t)CLAMP(p_emission, 0, 15);
	emit_changed();
}

int VoxelBlockRegistry::get_block_emission(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), 0);
	return blocks[p_id].emission;
}

void VoxelBlockRegistry::set_block_light_color(int p_id, const Color &p_color) {
	ERR_FAIL_COND(!blocks.has(p_id));
	blocks[p_id].light_color = p_color;
	emit_changed();
}

Color VoxelBlockRegistry::get_block_light_color(int p_id) const {
	ERR_FAIL_COND_V(!blocks.has(p_id), Color(1, 1, 1));
	return blocks[p_id].light_color;
}

void VoxelBlockRegistry::setup_defaults() {
	// Ensure all built-in block types have entries.
	// create_block() is a no-op for blocks already loaded from .tres,
	// and applies per-type defaults for newly created blocks.
	for (int i = 0; i < VOXEL_BLOCK_TYPE_MAX; i++) {
		create_block(i);
	}
}
