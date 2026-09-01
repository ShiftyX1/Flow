#include "voxel_block_registry.h"

#include "core/object/class_db.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

VoxelBlockRegistry::VoxelBlockRegistry() {
}

VoxelBlockRegistry::BlockShape VoxelBlockRegistry::shape_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME) {
		const String value = String(p_value).to_lower();
		if (value == "low_cube") {
			return BLOCK_SHAPE_LOW_CUBE;
		}
		if (value == "cross_plant") {
			return BLOCK_SHAPE_CROSS_PLANT;
		}
		if (value == "pane") {
			return BLOCK_SHAPE_PANE;
		}
		if (value == "fence") {
			return BLOCK_SHAPE_FENCE;
		}
		if (value == "ladder") {
			return BLOCK_SHAPE_LADDER;
		}
		return BLOCK_SHAPE_CUBE;
	}
	const int shape = CLAMP((int)p_value, 0, BLOCK_SHAPE_MAX - 1);
	return (BlockShape)shape;
}

String VoxelBlockRegistry::shape_to_string(BlockShape p_shape) {
	switch (p_shape) {
		case BLOCK_SHAPE_LOW_CUBE:
			return "low_cube";
		case BLOCK_SHAPE_CROSS_PLANT:
			return "cross_plant";
		case BLOCK_SHAPE_PANE:
			return "pane";
		case BLOCK_SHAPE_FENCE:
			return "fence";
		case BLOCK_SHAPE_LADDER:
			return "ladder";
		default:
			return "cube";
	}
}

VoxelBlockRegistry::VisualMode VoxelBlockRegistry::visual_mode_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME) {
		const String value = String(p_value).to_lower();
		if (value == "model_mesh") {
			return VISUAL_MODE_MODEL_MESH;
		}
		if (value == "model_scene") {
			return VISUAL_MODE_MODEL_SCENE;
		}
		return VISUAL_MODE_VOXEL;
	}
	const int mode = CLAMP((int)p_value, 0, VISUAL_MODE_MAX - 1);
	return (VisualMode)mode;
}

String VoxelBlockRegistry::visual_mode_to_string(VisualMode p_mode) {
	switch (p_mode) {
		case VISUAL_MODE_MODEL_MESH:
			return "model_mesh";
		case VISUAL_MODE_MODEL_SCENE:
			return "model_scene";
		default:
			return "voxel";
	}
}

static PackedStringArray _packed_strings_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::PACKED_STRING_ARRAY) {
		return p_value;
	}
	PackedStringArray out;
	if (p_value.get_type() == Variant::ARRAY) {
		Array array = p_value;
		for (int i = 0; i < array.size(); i++) {
			out.push_back(String(array[i]));
		}
	} else if (p_value.get_type() == Variant::STRING || p_value.get_type() == Variant::STRING_NAME) {
		out.push_back(String(p_value));
	}
	return out;
}

static void _apply_block_properties(VoxelBlockRegistry::BlockEntry &r_entry, const Dictionary &p_properties) {
	if (p_properties.has("solid")) {
		r_entry.solid = p_properties["solid"];
	}
	if (p_properties.has("transparent")) {
		r_entry.transparent = p_properties["transparent"];
	}
	if (p_properties.has("uses_alpha")) {
		r_entry.uses_alpha = p_properties["uses_alpha"];
	}
	if (p_properties.has("color")) {
		r_entry.color = p_properties["color"];
	}
	if (p_properties.has("light_opacity")) {
		r_entry.light_opacity = (uint8_t)CLAMP((int)p_properties["light_opacity"], 0, 15);
	}
	if (p_properties.has("emission")) {
		r_entry.emission = (uint8_t)CLAMP((int)p_properties["emission"], 0, 15);
	}
	if (p_properties.has("light_color")) {
		r_entry.light_color = p_properties["light_color"];
	}
	if (p_properties.has("shape")) {
		r_entry.shape = VoxelBlockRegistry::shape_from_variant(p_properties["shape"]);
	}
	if (p_properties.has("visual_mode")) {
		r_entry.visual_mode = VoxelBlockRegistry::visual_mode_from_variant(p_properties["visual_mode"]);
	}
	if (p_properties.has("model_mesh")) {
		r_entry.model_mesh = p_properties["model_mesh"];
	}
	if (p_properties.has("model_scene")) {
		r_entry.model_scene = p_properties["model_scene"];
	}
	if (p_properties.has("model_offset")) {
		r_entry.model_offset = p_properties["model_offset"];
	}
	if (p_properties.has("model_rotation_y")) {
		r_entry.model_rotation_y = p_properties["model_rotation_y"];
	}
	if (p_properties.has("model_scale")) {
		r_entry.model_scale = p_properties["model_scale"];
	}
	if (p_properties.has("animation_state_map")) {
		r_entry.animation_state_map = p_properties["animation_state_map"];
	}
	if (p_properties.has("mesh_height")) {
		r_entry.mesh_height = CLAMP((float)p_properties["mesh_height"], 0.0f, 1.0f);
	}
	if (p_properties.has("collision_height")) {
		r_entry.collision_height = CLAMP((float)p_properties["collision_height"], 0.0f, 1.0f);
	}
	if (p_properties.has("tags")) {
		r_entry.tags = _packed_strings_from_variant(p_properties["tags"]);
	}
	if (p_properties.has("resource_id")) {
		r_entry.resource_id = StringName(String(p_properties["resource_id"]));
	}
	if (p_properties.has("hazard_id")) {
		r_entry.hazard_id = StringName(String(p_properties["hazard_id"]));
	}
	if (p_properties.has("hazard_strength")) {
		r_entry.hazard_strength = MAX((float)p_properties["hazard_strength"], 0.0f);
	}
	if (p_properties.has("is_fluid")) {
		r_entry.is_fluid = p_properties["is_fluid"];
	}
	if (p_properties.has("replaceable")) {
		r_entry.replaceable = p_properties["replaceable"];
	}
}

// --- Registration API ---

int VoxelBlockRegistry::register_block(const String &p_name, const Dictionary &p_properties) {
	ERR_FAIL_COND_V_MSG(finalized, -1, "Cannot register blocks after finalize().");
	ERR_FAIL_COND_V_MSG(name_to_id.has(p_name), name_to_id[p_name], "Block '" + p_name + "' already registered.");

	int id = blocks.size();
	BlockEntry entry;
	entry.name = p_name;

	_apply_block_properties(entry, p_properties);

	blocks.push_back(entry);
	name_to_id[p_name] = id;
	emit_changed();
	return id;
}

int VoxelBlockRegistry::register_block_at(const String &p_name, int p_id, const Dictionary &p_properties) {
	ERR_FAIL_COND_V_MSG(finalized, -1, "Cannot register blocks after finalize().");
	ERR_FAIL_COND_V_MSG(p_id < 0, -1, "Block ID must be >= 0.");
	ERR_FAIL_COND_V_MSG(name_to_id.has(p_name), name_to_id[p_name], "Block '" + p_name + "' already registered.");

	// Extend vector to fit the requested ID.
	while (blocks.size() <= p_id) {
		BlockEntry placeholder;
		placeholder.name = "";
		blocks.push_back(placeholder);
	}

	ERR_FAIL_COND_V_MSG(!blocks[p_id].name.is_empty(), -1, "Block ID " + itos(p_id) + " already occupied by '" + blocks[p_id].name + "'.");

	BlockEntry entry;
	entry.name = p_name;

	_apply_block_properties(entry, p_properties);

	blocks.write[p_id] = entry;
	name_to_id[p_name] = p_id;
	emit_changed();
	return p_id;
}

void VoxelBlockRegistry::finalize() {
	if (finalized) {
		return;
	}
	_build_cache();
	finalized = true;
}

void VoxelBlockRegistry::_build_cache() {
	int count = blocks.size();
	cache_colors.resize(count);
	cache_solid.resize(count);
	cache_transparent.resize(count);
	cache_uses_alpha.resize(count);
	cache_emission.resize(count);
	cache_light_opacity.resize(count);
	cache_light_color.resize(count);
	cache_shape.resize(count);
	cache_visual_mode.resize(count);
	cache_is_fluid.resize(count);
	cache_replaceable.resize(count);

	for (int i = 0; i < count; i++) {
		const BlockEntry &e = blocks[i];
		cache_colors.write[i] = e.color;
		cache_solid.write[i] = e.solid;
		cache_transparent.write[i] = e.transparent;
		cache_uses_alpha.write[i] = e.uses_alpha;
		cache_emission.write[i] = e.emission;
		cache_light_opacity.write[i] = e.light_opacity;
		cache_light_color.write[i] = e.light_color;
		cache_shape.write[i] = (uint8_t)e.shape;
		cache_visual_mode.write[i] = (uint8_t)e.visual_mode;
		cache_is_fluid.write[i] = e.is_fluid;
		cache_replaceable.write[i] = e.replaceable;
	}
}

// --- Query API ---

int VoxelBlockRegistry::get_block_id(const String &p_name) const {
	const int *id = name_to_id.getptr(p_name);
	return id ? *id : -1;
}

int VoxelBlockRegistry::get_block_count() const {
	return blocks.size();
}

String VoxelBlockRegistry::get_block_name(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, blocks.size(), "Unknown");
	return blocks[p_id].name.is_empty() ? "Unknown" : blocks[p_id].name;
}

bool VoxelBlockRegistry::has_block(int p_id) const {
	return p_id >= 0 && p_id < blocks.size() && !blocks[p_id].name.is_empty();
}

PackedInt32Array VoxelBlockRegistry::get_block_list() const {
	PackedInt32Array list;
	for (int i = 0; i < blocks.size(); i++) {
		if (!blocks[i].name.is_empty()) {
			list.push_back(i);
		}
	}
	return list;
}

// --- Dynamic property serialization (backward compatible with old .tres format) ---

bool VoxelBlockRegistry::_set(const StringName &p_name, const Variant &p_value) {
	String prop_name = p_name;
	if (!prop_name.begins_with("block/")) {
		return false;
	}

	int idx = prop_name.get_slicec('/', 1).to_int();
	String what = prop_name.get_slicec('/', 2);

	// Grow vector to fit.
	while (blocks.size() <= idx) {
		BlockEntry placeholder;
		blocks.push_back(placeholder);
	}

	if (what == "name") {
		String bname = p_value;
		blocks.write[idx].name = bname;
		if (!bname.is_empty()) {
			name_to_id[bname] = idx;
		}
	} else if (what == "texture_top") {
		set_block_texture_top(idx, p_value);
	} else if (what == "texture_side") {
		set_block_texture_side(idx, p_value);
	} else if (what == "texture_bottom") {
		set_block_texture_bottom(idx, p_value);
	} else if (what == "shader_material") {
		set_block_shader_material(idx, p_value);
	} else if (what == "shape") {
		set_block_shape(idx, shape_from_variant(p_value));
	} else if (what == "visual_mode") {
		set_block_visual_mode(idx, visual_mode_from_variant(p_value));
	} else if (what == "model_mesh") {
		set_block_model_mesh(idx, p_value);
	} else if (what == "model_scene") {
		set_block_model_scene(idx, p_value);
	} else if (what == "model_offset") {
		set_block_model_offset(idx, p_value);
	} else if (what == "model_rotation_y") {
		set_block_model_rotation_y(idx, p_value);
	} else if (what == "model_scale") {
		set_block_model_scale(idx, p_value);
	} else if (what == "animation_state_map") {
		set_block_animation_state_map(idx, p_value);
	} else if (what == "mesh_height") {
		set_block_mesh_height(idx, p_value);
	} else if (what == "collision_height") {
		set_block_collision_height(idx, p_value);
	} else if (what == "tags") {
		set_block_tags(idx, p_value);
	} else if (what == "resource_id") {
		set_block_resource_id(idx, StringName(String(p_value)));
	} else if (what == "hazard_id") {
		set_block_hazard_id(idx, StringName(String(p_value)));
	} else if (what == "hazard_strength") {
		set_block_hazard_strength(idx, p_value);
	} else if (what == "is_fluid") {
		set_block_is_fluid(idx, p_value);
	} else if (what == "replaceable") {
		set_block_replaceable(idx, p_value);
	} else if (what == "color") {
		set_block_color(idx, p_value);
	} else if (what == "solid") {
		set_block_solid(idx, p_value);
	} else if (what == "transparent") {
		set_block_transparent(idx, p_value);
	} else if (what == "uses_alpha") {
		set_block_uses_alpha(idx, p_value);
	} else if (what == "light_opacity") {
		set_block_light_opacity(idx, (int)p_value);
	} else if (what == "emission") {
		set_block_emission(idx, (int)p_value);
	} else if (what == "light_color") {
		set_block_light_color(idx, p_value);
	} else if (what == "breakable") {
		set_block_breakable(idx, p_value);
	} else if (what == "requires_tool") {
		set_block_requires_tool(idx, p_value);
	} else if (what == "hand_break_time") {
		set_block_hand_break_time(idx, p_value);
	} else if (what == "tool_entry_count") {
		// Resize tool_entries vector to match serialised count.
		int count = (int)p_value;
		blocks.write[idx].tool_entries.resize(count);
	} else if (what == "tool_entry") {
		// Path: block/{id}/tool_entry/{j}/{field}
		int j = prop_name.get_slicec('/', 3).to_int();
		String field = prop_name.get_slicec('/', 4);
		if (j < 0 || j >= blocks[idx].tool_entries.size()) {
			return false;
		}
		if (field == "tool_type") {
			blocks.write[idx].tool_entries.write[j].tool_type = StringName(String(p_value));
		} else if (field == "min_tier") {
			blocks.write[idx].tool_entries.write[j].min_tier = (int)p_value;
		} else if (field == "break_time") {
			blocks.write[idx].tool_entries.write[j].break_time = (float)p_value;
		} else {
			return false;
		}
	} else {
		return false;
	}
	return true;
}

bool VoxelBlockRegistry::_get(const StringName &p_name, Variant &r_ret) const {
	String prop_name = p_name;
	if (!prop_name.begins_with("block/")) {
		return false;
	}
	int idx = prop_name.get_slicec('/', 1).to_int();
	if (idx < 0 || idx >= blocks.size()) {
		return false;
	}
	String what = prop_name.get_slicec('/', 2);

	if (what == "name") {
		r_ret = blocks[idx].name;
	} else if (what == "texture_top") {
		r_ret = get_block_texture_top(idx);
	} else if (what == "texture_side") {
		r_ret = get_block_texture_side(idx);
	} else if (what == "texture_bottom") {
		r_ret = get_block_texture_bottom(idx);
	} else if (what == "shader_material") {
		r_ret = get_block_shader_material(idx);
	} else if (what == "shape") {
		r_ret = shape_to_string(get_block_shape(idx));
	} else if (what == "visual_mode") {
		r_ret = visual_mode_to_string(get_block_visual_mode(idx));
	} else if (what == "model_mesh") {
		r_ret = get_block_model_mesh(idx);
	} else if (what == "model_scene") {
		r_ret = get_block_model_scene(idx);
	} else if (what == "model_offset") {
		r_ret = get_block_model_offset(idx);
	} else if (what == "model_rotation_y") {
		r_ret = get_block_model_rotation_y(idx);
	} else if (what == "model_scale") {
		r_ret = get_block_model_scale(idx);
	} else if (what == "animation_state_map") {
		r_ret = get_block_animation_state_map(idx);
	} else if (what == "mesh_height") {
		r_ret = get_block_mesh_height(idx);
	} else if (what == "collision_height") {
		r_ret = get_block_collision_height(idx);
	} else if (what == "tags") {
		r_ret = get_block_tags(idx);
	} else if (what == "resource_id") {
		r_ret = String(get_block_resource_id(idx));
	} else if (what == "hazard_id") {
		r_ret = String(get_block_hazard_id(idx));
	} else if (what == "hazard_strength") {
		r_ret = get_block_hazard_strength(idx);
	} else if (what == "is_fluid") {
		r_ret = get_block_is_fluid(idx);
	} else if (what == "replaceable") {
		r_ret = get_block_replaceable(idx);
	} else if (what == "color") {
		r_ret = get_block_color(idx);
	} else if (what == "solid") {
		r_ret = get_block_solid(idx);
	} else if (what == "transparent") {
		r_ret = get_block_transparent(idx);
	} else if (what == "uses_alpha") {
		r_ret = get_block_uses_alpha(idx);
	} else if (what == "light_opacity") {
		r_ret = get_block_light_opacity(idx);
	} else if (what == "emission") {
		r_ret = get_block_emission(idx);
	} else if (what == "light_color") {
		r_ret = get_block_light_color(idx);
	} else if (what == "breakable") {
		r_ret = get_block_breakable(idx);
	} else if (what == "requires_tool") {
		r_ret = get_block_requires_tool(idx);
	} else if (what == "hand_break_time") {
		r_ret = get_block_hand_break_time(idx);
	} else if (what == "tool_entry_count") {
		r_ret = get_block_tool_entry_count(idx);
	} else if (what == "tool_entry") {
		// Path: block/{id}/tool_entry/{j}/{field}
		int j = prop_name.get_slicec('/', 3).to_int();
		String field = prop_name.get_slicec('/', 4);
		if (j < 0 || j >= blocks[idx].tool_entries.size()) {
			return false;
		}
		const BlockToolEntry &te = blocks[idx].tool_entries[j];
		if (field == "tool_type") {
			r_ret = String(te.tool_type);
		} else if (field == "min_tier") {
			r_ret = te.min_tier;
		} else if (field == "break_time") {
			r_ret = te.break_time;
		} else {
			return false;
		}
	} else {
		return false;
	}
	return true;
}

void VoxelBlockRegistry::_get_property_list(List<PropertyInfo> *p_list) const {
	for (int i = 0; i < blocks.size(); i++) {
		if (blocks[i].name.is_empty()) {
			continue;
		}
		String prefix = vformat("block/%d/", i);
		String block_label = blocks[i].name;

		p_list->push_back(PropertyInfo(Variant::NIL, vformat("Block %d (%s)", i, block_label), PROPERTY_HINT_NONE, prefix, PROPERTY_USAGE_GROUP));
		p_list->push_back(PropertyInfo(Variant::STRING, prefix + "name"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "texture_top", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "texture_side", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "texture_bottom", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "shader_material", PROPERTY_HINT_RESOURCE_TYPE, "ShaderMaterial"));
		p_list->push_back(PropertyInfo(Variant::STRING, prefix + "shape", PROPERTY_HINT_ENUM, "cube,low_cube,cross_plant,pane,fence,ladder"));
		p_list->push_back(PropertyInfo(Variant::STRING, prefix + "visual_mode", PROPERTY_HINT_ENUM, "voxel,model_mesh,model_scene"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "model_mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"));
		p_list->push_back(PropertyInfo(Variant::OBJECT, prefix + "model_scene", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"));
		p_list->push_back(PropertyInfo(Variant::VECTOR3, prefix + "model_offset"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + "model_rotation_y", PROPERTY_HINT_RANGE, "-360.0,360.0,0.1,degrees"));
		p_list->push_back(PropertyInfo(Variant::VECTOR3, prefix + "model_scale"));
		p_list->push_back(PropertyInfo(Variant::DICTIONARY, prefix + "animation_state_map"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + "mesh_height", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + "collision_height", PROPERTY_HINT_RANGE, "0.0,1.0,0.01"));
		p_list->push_back(PropertyInfo(Variant::PACKED_STRING_ARRAY, prefix + "tags"));
		p_list->push_back(PropertyInfo(Variant::STRING, prefix + "resource_id"));
		p_list->push_back(PropertyInfo(Variant::STRING, prefix + "hazard_id"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + "hazard_strength", PROPERTY_HINT_RANGE, "0.0,1000.0,0.01"));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + "is_fluid"));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + "replaceable"));
		p_list->push_back(PropertyInfo(Variant::COLOR, prefix + "color"));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + "solid"));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + "transparent"));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + "uses_alpha"));
		p_list->push_back(PropertyInfo(Variant::INT, prefix + "light_opacity", PROPERTY_HINT_RANGE, "0,15,1"));
		p_list->push_back(PropertyInfo(Variant::INT, prefix + "emission", PROPERTY_HINT_RANGE, "0,15,1"));
		p_list->push_back(PropertyInfo(Variant::COLOR, prefix + "light_color"));
		// Mining / survival
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + "breakable"));
		p_list->push_back(PropertyInfo(Variant::BOOL, prefix + "requires_tool"));
		p_list->push_back(PropertyInfo(Variant::FLOAT, prefix + "hand_break_time", PROPERTY_HINT_RANGE, "0.0,300.0,0.1"));
		p_list->push_back(PropertyInfo(Variant::INT, prefix + "tool_entry_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
		const Vector<BlockToolEntry> &entries = blocks[i].tool_entries;
		for (int j = 0; j < entries.size(); j++) {
			String ep = vformat("block/%d/tool_entry/%d/", i, j);
			p_list->push_back(PropertyInfo(Variant::STRING, ep + "tool_type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
			p_list->push_back(PropertyInfo(Variant::INT, ep + "min_tier", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
			p_list->push_back(PropertyInfo(Variant::FLOAT, ep + "break_time", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
		}
	}
}

// --- Bind methods ---

void VoxelBlockRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("register_block", "name", "properties"), &VoxelBlockRegistry::register_block, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("register_block_at", "name", "id", "properties"), &VoxelBlockRegistry::register_block_at, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("finalize"), &VoxelBlockRegistry::finalize);
	ClassDB::bind_method(D_METHOD("is_finalized"), &VoxelBlockRegistry::is_finalized);

	ClassDB::bind_method(D_METHOD("get_block_id", "name"), &VoxelBlockRegistry::get_block_id);
	ClassDB::bind_method(D_METHOD("get_block_count"), &VoxelBlockRegistry::get_block_count);
	ClassDB::bind_method(D_METHOD("get_block_name", "id"), &VoxelBlockRegistry::get_block_name);
	ClassDB::bind_method(D_METHOD("has_block", "id"), &VoxelBlockRegistry::has_block);
	ClassDB::bind_method(D_METHOD("get_block_list"), &VoxelBlockRegistry::get_block_list);

	ClassDB::bind_method(D_METHOD("create_block", "id"), &VoxelBlockRegistry::create_block);
	ClassDB::bind_method(D_METHOD("remove_block", "id"), &VoxelBlockRegistry::remove_block);

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

	ClassDB::bind_method(D_METHOD("set_block_shape", "id", "shape"), &VoxelBlockRegistry::set_block_shape);
	ClassDB::bind_method(D_METHOD("get_block_shape", "id"), &VoxelBlockRegistry::get_block_shape);

	ClassDB::bind_method(D_METHOD("set_block_visual_mode", "id", "mode"), &VoxelBlockRegistry::set_block_visual_mode);
	ClassDB::bind_method(D_METHOD("get_block_visual_mode", "id"), &VoxelBlockRegistry::get_block_visual_mode);
	ClassDB::bind_method(D_METHOD("block_has_model_visual", "id"), &VoxelBlockRegistry::block_has_model_visual);
	ClassDB::bind_method(D_METHOD("set_block_model_mesh", "id", "mesh"), &VoxelBlockRegistry::set_block_model_mesh);
	ClassDB::bind_method(D_METHOD("get_block_model_mesh", "id"), &VoxelBlockRegistry::get_block_model_mesh);
	ClassDB::bind_method(D_METHOD("set_block_model_scene", "id", "scene"), &VoxelBlockRegistry::set_block_model_scene);
	ClassDB::bind_method(D_METHOD("get_block_model_scene", "id"), &VoxelBlockRegistry::get_block_model_scene);
	ClassDB::bind_method(D_METHOD("set_block_model_offset", "id", "offset"), &VoxelBlockRegistry::set_block_model_offset);
	ClassDB::bind_method(D_METHOD("get_block_model_offset", "id"), &VoxelBlockRegistry::get_block_model_offset);
	ClassDB::bind_method(D_METHOD("set_block_model_rotation_y", "id", "degrees"), &VoxelBlockRegistry::set_block_model_rotation_y);
	ClassDB::bind_method(D_METHOD("get_block_model_rotation_y", "id"), &VoxelBlockRegistry::get_block_model_rotation_y);
	ClassDB::bind_method(D_METHOD("set_block_model_scale", "id", "scale"), &VoxelBlockRegistry::set_block_model_scale);
	ClassDB::bind_method(D_METHOD("get_block_model_scale", "id"), &VoxelBlockRegistry::get_block_model_scale);
	ClassDB::bind_method(D_METHOD("set_block_animation_state_map", "id", "state_map"), &VoxelBlockRegistry::set_block_animation_state_map);
	ClassDB::bind_method(D_METHOD("get_block_animation_state_map", "id"), &VoxelBlockRegistry::get_block_animation_state_map);

	ClassDB::bind_method(D_METHOD("set_block_mesh_height", "id", "height"), &VoxelBlockRegistry::set_block_mesh_height);
	ClassDB::bind_method(D_METHOD("get_block_mesh_height", "id"), &VoxelBlockRegistry::get_block_mesh_height);
	ClassDB::bind_method(D_METHOD("set_block_collision_height", "id", "height"), &VoxelBlockRegistry::set_block_collision_height);
	ClassDB::bind_method(D_METHOD("get_block_collision_height", "id"), &VoxelBlockRegistry::get_block_collision_height);

	ClassDB::bind_method(D_METHOD("set_block_tags", "id", "tags"), &VoxelBlockRegistry::set_block_tags);
	ClassDB::bind_method(D_METHOD("get_block_tags", "id"), &VoxelBlockRegistry::get_block_tags);
	ClassDB::bind_method(D_METHOD("block_has_tag", "id", "tag"), &VoxelBlockRegistry::block_has_tag);
	ClassDB::bind_method(D_METHOD("set_block_resource_id", "id", "resource_id"), &VoxelBlockRegistry::set_block_resource_id);
	ClassDB::bind_method(D_METHOD("get_block_resource_id", "id"), &VoxelBlockRegistry::get_block_resource_id);
	ClassDB::bind_method(D_METHOD("set_block_hazard_id", "id", "hazard_id"), &VoxelBlockRegistry::set_block_hazard_id);
	ClassDB::bind_method(D_METHOD("get_block_hazard_id", "id"), &VoxelBlockRegistry::get_block_hazard_id);
	ClassDB::bind_method(D_METHOD("set_block_hazard_strength", "id", "strength"), &VoxelBlockRegistry::set_block_hazard_strength);
	ClassDB::bind_method(D_METHOD("get_block_hazard_strength", "id"), &VoxelBlockRegistry::get_block_hazard_strength);
	ClassDB::bind_method(D_METHOD("set_block_is_fluid", "id", "is_fluid"), &VoxelBlockRegistry::set_block_is_fluid);
	ClassDB::bind_method(D_METHOD("get_block_is_fluid", "id"), &VoxelBlockRegistry::get_block_is_fluid);
	ClassDB::bind_method(D_METHOD("set_block_replaceable", "id", "replaceable"), &VoxelBlockRegistry::set_block_replaceable);
	ClassDB::bind_method(D_METHOD("get_block_replaceable", "id"), &VoxelBlockRegistry::get_block_replaceable);

	ClassDB::bind_method(D_METHOD("set_block_color", "id", "color"), &VoxelBlockRegistry::set_block_color);
	ClassDB::bind_method(D_METHOD("get_block_color", "id"), &VoxelBlockRegistry::get_block_color);

	ClassDB::bind_method(D_METHOD("set_block_solid", "id", "solid"), &VoxelBlockRegistry::set_block_solid);
	ClassDB::bind_method(D_METHOD("get_block_solid", "id"), &VoxelBlockRegistry::get_block_solid);
	ClassDB::bind_method(D_METHOD("set_block_transparent", "id", "transparent"), &VoxelBlockRegistry::set_block_transparent);
	ClassDB::bind_method(D_METHOD("get_block_transparent", "id"), &VoxelBlockRegistry::get_block_transparent);
	ClassDB::bind_method(D_METHOD("set_block_uses_alpha", "id", "uses_alpha"), &VoxelBlockRegistry::set_block_uses_alpha);
	ClassDB::bind_method(D_METHOD("get_block_uses_alpha", "id"), &VoxelBlockRegistry::get_block_uses_alpha);

	ClassDB::bind_method(D_METHOD("set_block_light_opacity", "id", "opacity"), &VoxelBlockRegistry::set_block_light_opacity);
	ClassDB::bind_method(D_METHOD("get_block_light_opacity", "id"), &VoxelBlockRegistry::get_block_light_opacity);
	ClassDB::bind_method(D_METHOD("set_block_emission", "id", "emission"), &VoxelBlockRegistry::set_block_emission);
	ClassDB::bind_method(D_METHOD("get_block_emission", "id"), &VoxelBlockRegistry::get_block_emission);
	ClassDB::bind_method(D_METHOD("set_block_light_color", "id", "color"), &VoxelBlockRegistry::set_block_light_color);
	ClassDB::bind_method(D_METHOD("get_block_light_color", "id"), &VoxelBlockRegistry::get_block_light_color);

	ClassDB::bind_method(D_METHOD("set_block_name", "id", "name"), &VoxelBlockRegistry::set_block_name);

	ClassDB::bind_method(D_METHOD("set_block_breakable", "id", "breakable"), &VoxelBlockRegistry::set_block_breakable);
	ClassDB::bind_method(D_METHOD("get_block_breakable", "id"), &VoxelBlockRegistry::get_block_breakable);
	ClassDB::bind_method(D_METHOD("set_block_requires_tool", "id", "requires_tool"), &VoxelBlockRegistry::set_block_requires_tool);
	ClassDB::bind_method(D_METHOD("get_block_requires_tool", "id"), &VoxelBlockRegistry::get_block_requires_tool);
	ClassDB::bind_method(D_METHOD("set_block_hand_break_time", "id", "time"), &VoxelBlockRegistry::set_block_hand_break_time);
	ClassDB::bind_method(D_METHOD("get_block_hand_break_time", "id"), &VoxelBlockRegistry::get_block_hand_break_time);
	ClassDB::bind_method(D_METHOD("add_block_tool_entry", "id", "tool_type", "min_tier", "break_time"), &VoxelBlockRegistry::add_block_tool_entry);
	ClassDB::bind_method(D_METHOD("remove_block_tool_entry", "id", "entry_idx"), &VoxelBlockRegistry::remove_block_tool_entry);
	ClassDB::bind_method(D_METHOD("get_block_tool_entry_count", "id"), &VoxelBlockRegistry::get_block_tool_entry_count);
	ClassDB::bind_method(D_METHOD("get_block_tool_entries", "id"), &VoxelBlockRegistry::get_block_tool_entries);
	ClassDB::bind_method(D_METHOD("get_block_break_time_for_tool", "id", "tool_type", "tool_tier"), &VoxelBlockRegistry::get_block_break_time_for_tool);

	ClassDB::bind_method(D_METHOD("setup_defaults"), &VoxelBlockRegistry::setup_defaults);

	BIND_ENUM_CONSTANT(BLOCK_SHAPE_CUBE);
	BIND_ENUM_CONSTANT(BLOCK_SHAPE_LOW_CUBE);
	BIND_ENUM_CONSTANT(BLOCK_SHAPE_CROSS_PLANT);
	BIND_ENUM_CONSTANT(BLOCK_SHAPE_PANE);
	BIND_ENUM_CONSTANT(BLOCK_SHAPE_FENCE);
	BIND_ENUM_CONSTANT(BLOCK_SHAPE_LADDER);

	BIND_ENUM_CONSTANT(VISUAL_MODE_VOXEL);
	BIND_ENUM_CONSTANT(VISUAL_MODE_MODEL_MESH);
	BIND_ENUM_CONSTANT(VISUAL_MODE_MODEL_SCENE);
}

// --- Per-property implementations ---

#define REGISTRY_ENSURE_BLOCK(p_id) \
	ERR_FAIL_INDEX(p_id, blocks.size());

#define REGISTRY_ENSURE_BLOCK_V(p_id, ret) \
	ERR_FAIL_INDEX_V(p_id, blocks.size(), ret);

void VoxelBlockRegistry::set_block_texture_top(int p_id, const Ref<Texture2D> &p_texture) {
	REGISTRY_ENSURE_BLOCK(p_id);
	// If meshing workers can read finalized caches, this write needs a publication boundary; immutable only in good weather is not a contract.
	blocks.write[p_id].texture_top = p_texture;
	emit_changed();
}
Ref<Texture2D> VoxelBlockRegistry::get_block_texture_top(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Ref<Texture2D>());
	return blocks[p_id].texture_top;
}

void VoxelBlockRegistry::set_block_texture_side(int p_id, const Ref<Texture2D> &p_texture) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].texture_side = p_texture;
	emit_changed();
}
Ref<Texture2D> VoxelBlockRegistry::get_block_texture_side(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Ref<Texture2D>());
	return blocks[p_id].texture_side;
}

void VoxelBlockRegistry::set_block_texture_bottom(int p_id, const Ref<Texture2D> &p_texture) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].texture_bottom = p_texture;
	emit_changed();
}
Ref<Texture2D> VoxelBlockRegistry::get_block_texture_bottom(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Ref<Texture2D>());
	return blocks[p_id].texture_bottom;
}

Ref<Texture2D> VoxelBlockRegistry::get_block_texture_for_face(int p_id, const Vector3 &p_normal) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Ref<Texture2D>());
	const BlockEntry &entry = blocks[p_id];
	if (p_normal.y > 0.5f) {
		return entry.texture_top;
	} else if (p_normal.y < -0.5f) {
		return entry.texture_bottom;
	} else {
		return entry.texture_side;
	}
}

bool VoxelBlockRegistry::block_has_texture(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, false);
	const BlockEntry &entry = blocks[p_id];
	return entry.texture_top.is_valid() || entry.texture_side.is_valid() || entry.texture_bottom.is_valid();
}

void VoxelBlockRegistry::set_block_shader_material(int p_id, const Ref<ShaderMaterial> &p_material) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].shader_material = p_material;
	emit_changed();
}
Ref<ShaderMaterial> VoxelBlockRegistry::get_block_shader_material(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Ref<ShaderMaterial>());
	return blocks[p_id].shader_material;
}
bool VoxelBlockRegistry::block_has_shader(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, false);
	return blocks[p_id].shader_material.is_valid();
}

void VoxelBlockRegistry::set_block_shape(int p_id, BlockShape p_shape) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].shape = (BlockShape)CLAMP((int)p_shape, 0, BLOCK_SHAPE_MAX - 1);
	emit_changed();
}
VoxelBlockRegistry::BlockShape VoxelBlockRegistry::get_block_shape(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, BLOCK_SHAPE_CUBE);
	return blocks[p_id].shape;
}

void VoxelBlockRegistry::set_block_visual_mode(int p_id, VisualMode p_mode) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].visual_mode = (VisualMode)CLAMP((int)p_mode, 0, VISUAL_MODE_MAX - 1);
	emit_changed();
}
VoxelBlockRegistry::VisualMode VoxelBlockRegistry::get_block_visual_mode(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, VISUAL_MODE_VOXEL);
	return blocks[p_id].visual_mode;
}
bool VoxelBlockRegistry::block_has_model_visual(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, false);
	const BlockEntry &entry = blocks[p_id];
	return (entry.visual_mode == VISUAL_MODE_MODEL_MESH && entry.model_mesh.is_valid()) ||
			(entry.visual_mode == VISUAL_MODE_MODEL_SCENE && entry.model_scene.is_valid());
}

void VoxelBlockRegistry::set_block_model_mesh(int p_id, const Ref<Mesh> &p_mesh) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].model_mesh = p_mesh;
	emit_changed();
}
Ref<Mesh> VoxelBlockRegistry::get_block_model_mesh(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Ref<Mesh>());
	return blocks[p_id].model_mesh;
}

void VoxelBlockRegistry::set_block_model_scene(int p_id, const Ref<PackedScene> &p_scene) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].model_scene = p_scene;
	emit_changed();
}
Ref<PackedScene> VoxelBlockRegistry::get_block_model_scene(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Ref<PackedScene>());
	return blocks[p_id].model_scene;
}

void VoxelBlockRegistry::set_block_model_offset(int p_id, const Vector3 &p_offset) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].model_offset = p_offset;
	emit_changed();
}
Vector3 VoxelBlockRegistry::get_block_model_offset(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Vector3());
	return blocks[p_id].model_offset;
}

void VoxelBlockRegistry::set_block_model_rotation_y(int p_id, float p_degrees) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].model_rotation_y = p_degrees;
	emit_changed();
}
float VoxelBlockRegistry::get_block_model_rotation_y(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, 0.0f);
	return blocks[p_id].model_rotation_y;
}

void VoxelBlockRegistry::set_block_model_scale(int p_id, const Vector3 &p_scale) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].model_scale = p_scale;
	emit_changed();
}
Vector3 VoxelBlockRegistry::get_block_model_scale(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Vector3(1, 1, 1));
	return blocks[p_id].model_scale;
}

void VoxelBlockRegistry::set_block_animation_state_map(int p_id, const Dictionary &p_map) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].animation_state_map = p_map;
	emit_changed();
}
Dictionary VoxelBlockRegistry::get_block_animation_state_map(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Dictionary());
	return blocks[p_id].animation_state_map;
}

void VoxelBlockRegistry::set_block_mesh_height(int p_id, float p_height) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].mesh_height = CLAMP(p_height, 0.0f, 1.0f);
	emit_changed();
}
float VoxelBlockRegistry::get_block_mesh_height(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, 1.0f);
	return blocks[p_id].mesh_height;
}

void VoxelBlockRegistry::set_block_collision_height(int p_id, float p_height) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].collision_height = CLAMP(p_height, 0.0f, 1.0f);
	emit_changed();
}
float VoxelBlockRegistry::get_block_collision_height(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, 1.0f);
	return blocks[p_id].collision_height;
}

void VoxelBlockRegistry::set_block_tags(int p_id, const PackedStringArray &p_tags) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].tags = p_tags;
	emit_changed();
}
PackedStringArray VoxelBlockRegistry::get_block_tags(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, PackedStringArray());
	return blocks[p_id].tags;
}
bool VoxelBlockRegistry::block_has_tag(int p_id, const StringName &p_tag) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, false);
	const String tag = String(p_tag);
	const PackedStringArray &tags = blocks[p_id].tags;
	for (int i = 0; i < tags.size(); i++) {
		if (tags[i] == tag) {
			return true;
		}
	}
	return false;
}

void VoxelBlockRegistry::set_block_resource_id(int p_id, const StringName &p_resource_id) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].resource_id = p_resource_id;
	emit_changed();
}
StringName VoxelBlockRegistry::get_block_resource_id(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, StringName());
	return blocks[p_id].resource_id;
}

void VoxelBlockRegistry::set_block_hazard_id(int p_id, const StringName &p_hazard_id) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].hazard_id = p_hazard_id;
	emit_changed();
}
StringName VoxelBlockRegistry::get_block_hazard_id(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, StringName());
	return blocks[p_id].hazard_id;
}

void VoxelBlockRegistry::set_block_hazard_strength(int p_id, float p_strength) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].hazard_strength = MAX(p_strength, 0.0f);
	emit_changed();
}
float VoxelBlockRegistry::get_block_hazard_strength(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, 0.0f);
	return blocks[p_id].hazard_strength;
}

void VoxelBlockRegistry::set_block_is_fluid(int p_id, bool p_is_fluid) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].is_fluid = p_is_fluid;
	emit_changed();
}
bool VoxelBlockRegistry::get_block_is_fluid(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, false);
	return blocks[p_id].is_fluid;
}

void VoxelBlockRegistry::set_block_replaceable(int p_id, bool p_replaceable) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].replaceable = p_replaceable;
	emit_changed();
}
bool VoxelBlockRegistry::get_block_replaceable(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, false);
	return blocks[p_id].replaceable;
}

void VoxelBlockRegistry::set_block_color(int p_id, const Color &p_color) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].color = p_color;
	emit_changed();
}
Color VoxelBlockRegistry::get_block_color(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Color(1, 1, 1));
	return blocks[p_id].color;
}

void VoxelBlockRegistry::set_block_solid(int p_id, bool p_solid) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].solid = p_solid;
	emit_changed();
}
bool VoxelBlockRegistry::get_block_solid(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, true);
	return blocks[p_id].solid;
}

void VoxelBlockRegistry::set_block_transparent(int p_id, bool p_transparent) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].transparent = p_transparent;
	emit_changed();
}
bool VoxelBlockRegistry::get_block_transparent(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, false);
	return blocks[p_id].transparent;
}

void VoxelBlockRegistry::set_block_uses_alpha(int p_id, bool p_uses_alpha) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].uses_alpha = p_uses_alpha;
	emit_changed();
}
bool VoxelBlockRegistry::get_block_uses_alpha(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, false);
	return blocks[p_id].uses_alpha;
}

void VoxelBlockRegistry::set_block_light_opacity(int p_id, int p_opacity) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].light_opacity = (uint8_t)CLAMP(p_opacity, 0, 15);
	emit_changed();
}
int VoxelBlockRegistry::get_block_light_opacity(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, 15);
	return blocks[p_id].light_opacity;
}

void VoxelBlockRegistry::set_block_emission(int p_id, int p_emission) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].emission = (uint8_t)CLAMP(p_emission, 0, 15);
	emit_changed();
}
int VoxelBlockRegistry::get_block_emission(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, 0);
	return blocks[p_id].emission;
}

void VoxelBlockRegistry::set_block_light_color(int p_id, const Color &p_color) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].light_color = p_color;
	emit_changed();
}
Color VoxelBlockRegistry::get_block_light_color(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Color(1, 1, 1));
	return blocks[p_id].light_color;
}

void VoxelBlockRegistry::set_block_name(int p_id, const String &p_name) {
	REGISTRY_ENSURE_BLOCK(p_id);
	String old_name = blocks[p_id].name;
	if (!old_name.is_empty()) {
		name_to_id.erase(old_name);
	}
	blocks.write[p_id].name = p_name;
	if (!p_name.is_empty()) {
		name_to_id[p_name] = p_id;
	}
	emit_changed();
}

// --- Legacy compat ---

void VoxelBlockRegistry::create_block(int p_id) {
	if (p_id >= 0 && p_id < blocks.size() && !blocks[p_id].name.is_empty()) {
		return; // Already exists.
	}
	// Grow to fit.
	while (blocks.size() <= p_id) {
		BlockEntry placeholder;
		blocks.push_back(placeholder);
	}
	// Assign a default name if empty.
	if (blocks[p_id].name.is_empty()) {
		blocks.write[p_id].name = "block_" + itos(p_id);
		name_to_id[blocks[p_id].name] = p_id;
	}
	emit_changed();
}

void VoxelBlockRegistry::remove_block(int p_id) {
	ERR_FAIL_INDEX(p_id, blocks.size());
	String old_name = blocks[p_id].name;
	if (!old_name.is_empty()) {
		name_to_id.erase(old_name);
	}
	blocks.write[p_id] = BlockEntry(); // Clear to empty placeholder.
	emit_changed();
}

// --- setup_defaults: register 11 built-in blocks ---

void VoxelBlockRegistry::setup_defaults() {
	// Merge built-in block defaults into the registry.
	// If a slot already has a name (fully registered), skip entirely.
	// If a slot has data from .tscn (textures, etc.) but no name (old format),
	// set the name and merge physics/lighting defaults WITHOUT overwriting
	// textures or visual properties loaded from the scene file.
	auto reg = [&](const String &p_name, int p_id, const Dictionary &p_props) {
		// Grow vector if needed.
		while (blocks.size() <= p_id) {
			BlockEntry placeholder;
			blocks.push_back(placeholder);
		}

		BlockEntry &entry = blocks.write[p_id];

		// Already fully registered (has a name from .tres or API call).
		if (!entry.name.is_empty()) {
			return;
		}

		// Set the name (was not in old .tscn format).
		entry.name = p_name;
		name_to_id[p_name] = p_id;

		// Merge physics/lighting defaults onto the existing entry.
		// Textures, shader_material, mesh_height, collision_height, and uses_alpha
		// may already be set from the .tscn — only apply them if still at defaults.
		if (p_props.has("solid")) {
			entry.solid = p_props["solid"];
		}
		if (p_props.has("transparent")) {
			entry.transparent = p_props["transparent"];
		}
		if (p_props.has("color")) {
			entry.color = p_props["color"];
		}
		if (p_props.has("light_opacity")) {
			entry.light_opacity = (uint8_t)CLAMP((int)p_props["light_opacity"], 0, 15);
		}
		if (p_props.has("emission")) {
			entry.emission = (uint8_t)CLAMP((int)p_props["emission"], 0, 15);
		}
		if (p_props.has("light_color") && entry.light_color == Color(1, 1, 1)) {
			entry.light_color = p_props["light_color"];
		}
		if (p_props.has("shape") && entry.shape == BLOCK_SHAPE_CUBE) {
			entry.shape = shape_from_variant(p_props["shape"]);
		}
		if (p_props.has("visual_mode") && entry.visual_mode == VISUAL_MODE_VOXEL) {
			entry.visual_mode = visual_mode_from_variant(p_props["visual_mode"]);
		}
		if (p_props.has("model_mesh") && entry.model_mesh.is_null()) {
			entry.model_mesh = p_props["model_mesh"];
		}
		if (p_props.has("model_scene") && entry.model_scene.is_null()) {
			entry.model_scene = p_props["model_scene"];
		}
		if (p_props.has("model_offset") && entry.model_offset == Vector3()) {
			entry.model_offset = p_props["model_offset"];
		}
		if (p_props.has("model_rotation_y") && entry.model_rotation_y == 0.0f) {
			entry.model_rotation_y = p_props["model_rotation_y"];
		}
		if (p_props.has("model_scale") && entry.model_scale == Vector3(1, 1, 1)) {
			entry.model_scale = p_props["model_scale"];
		}
		if (p_props.has("animation_state_map") && entry.animation_state_map.is_empty()) {
			entry.animation_state_map = p_props["animation_state_map"];
		}
		// Only apply mesh_height/collision_height/uses_alpha if still at BlockEntry defaults.
		if (p_props.has("mesh_height") && entry.mesh_height == 1.0f) {
			entry.mesh_height = CLAMP((float)p_props["mesh_height"], 0.0f, 1.0f);
		}
		if (p_props.has("collision_height") && entry.collision_height == 1.0f) {
			entry.collision_height = CLAMP((float)p_props["collision_height"], 0.0f, 1.0f);
		}
		if (p_props.has("uses_alpha") && !entry.uses_alpha) {
			entry.uses_alpha = p_props["uses_alpha"];
		}
		if (p_props.has("tags") && entry.tags.is_empty()) {
			entry.tags = p_props["tags"];
		}
		if (p_props.has("resource_id") && entry.resource_id == StringName()) {
			entry.resource_id = StringName(String(p_props["resource_id"]));
		}
		if (p_props.has("hazard_id") && entry.hazard_id == StringName()) {
			entry.hazard_id = StringName(String(p_props["hazard_id"]));
		}
		if (p_props.has("hazard_strength") && entry.hazard_strength == 0.0f) {
			entry.hazard_strength = MAX((float)p_props["hazard_strength"], 0.0f);
		}
		if (p_props.has("is_fluid") && !entry.is_fluid) {
			entry.is_fluid = p_props["is_fluid"];
		}
		if (p_props.has("replaceable") && !entry.replaceable) {
			entry.replaceable = p_props["replaceable"];
		}
	};

	Dictionary d;

	// 0: AIR
	d.clear();
	d["solid"] = false;
	d["transparent"] = true;
	d["light_opacity"] = 0;
	d["color"] = Color(0.0f, 0.0f, 0.0f, 0.0f);
	d["replaceable"] = true;
	reg("air", 0, d);

	// 1: GRASS
	d.clear();
	d["color"] = Color(0.36f, 0.63f, 0.20f);
	reg("grass", 1, d);

	// 2: DIRT
	d.clear();
	d["color"] = Color(0.55f, 0.37f, 0.24f);
	reg("dirt", 2, d);

	// 3: STONE
	d.clear();
	d["color"] = Color(0.50f, 0.50f, 0.50f);
	reg("stone", 3, d);

	// 4: SAND
	d.clear();
	d["color"] = Color(0.86f, 0.82f, 0.55f);
	reg("sand", 4, d);

	// 5: WATER
	d.clear();
	d["color"] = Color(0.24f, 0.46f, 0.72f, 0.7f);
	d["solid"] = false;
	d["transparent"] = true;
	d["light_opacity"] = 2;
	d["mesh_height"] = 0.875f;
	d["collision_height"] = 0.875f;
	d["shape"] = "low_cube";
	d["is_fluid"] = true;
	d["replaceable"] = true;
	reg("water", 5, d);

	// 6: SNOW
	d.clear();
	d["color"] = Color(0.93f, 0.93f, 0.97f);
	reg("snow", 6, d);

	// 7: WOOD
	d.clear();
	d["color"] = Color(0.40f, 0.26f, 0.13f);
	reg("wood", 7, d);

	// 8: LEAVES
	d.clear();
	d["color"] = Color(0.16f, 0.38f, 0.14f);
	d["light_opacity"] = 1;
	d["uses_alpha"] = true;
	PackedStringArray leaves_tags;
	leaves_tags.push_back("organic");
	leaves_tags.push_back("foliage");
	d["tags"] = leaves_tags;
	reg("leaves", 8, d);

	// 9: BEDROCK
	d.clear();
	d["color"] = Color(0.20f, 0.20f, 0.20f);
	reg("bedrock", 9, d);

	// 10: TORCH
	d.clear();
	d["color"] = Color(1.0f, 0.8f, 0.3f, 1.0f);
	d["solid"] = false;
	d["transparent"] = true;
	d["light_opacity"] = 0;
	d["emission"] = 14;
	d["light_color"] = Color(1.0f, 0.6f, 0.2f);
	d["shape"] = "cross_plant";
	reg("torch", 10, d);

	// 11: BIOLUMEN PLANT
	d.clear();
	d["color"] = Color(0.34f, 0.92f, 0.72f, 0.92f);
	d["solid"] = false;
	d["transparent"] = true;
	d["uses_alpha"] = true;
	d["light_opacity"] = 0;
	d["emission"] = 5;
	d["light_color"] = Color(0.25f, 0.95f, 0.72f);
	d["shape"] = "cross_plant";
	d["resource_id"] = "medicinal_fiber";
	d["replaceable"] = true;
	PackedStringArray biolumen_tags;
	biolumen_tags.push_back("organic");
	biolumen_tags.push_back("flora");
	biolumen_tags.push_back("restored_forest");
	d["tags"] = biolumen_tags;
	reg("biolumen_plant", 11, d);

	// 12: BIO RESIN
	d.clear();
	d["color"] = Color(0.58f, 0.82f, 0.50f, 0.88f);
	d["transparent"] = true;
	d["light_opacity"] = 2;
	d["mesh_height"] = 0.35f;
	d["collision_height"] = 0.35f;
	d["shape"] = "low_cube";
	d["resource_id"] = "bio_resin";
	PackedStringArray bio_resin_tags;
	bio_resin_tags.push_back("organic");
	bio_resin_tags.push_back("resource");
	bio_resin_tags.push_back("restored_forest");
	d["tags"] = bio_resin_tags;
	reg("bio_resin", 12, d);

	// 13: RUSTED PANEL
	d.clear();
	d["color"] = Color(0.45f, 0.32f, 0.25f);
	d["light_opacity"] = 15;
	d["shape"] = "pane";
	d["resource_id"] = "scrap_metal";
	PackedStringArray rusted_panel_tags;
	rusted_panel_tags.push_back("ruin");
	rusted_panel_tags.push_back("metal");
	rusted_panel_tags.push_back("old_world");
	d["tags"] = rusted_panel_tags;
	reg("rusted_panel", 13, d);

	// 14: BEACON CORE
	d.clear();
	d["color"] = Color(0.55f, 0.80f, 1.0f);
	d["transparent"] = true;
	d["light_opacity"] = 2;
	d["emission"] = 12;
	d["light_color"] = Color(0.35f, 0.70f, 1.0f);
	d["resource_id"] = "archive_module";
	PackedStringArray beacon_core_tags;
	beacon_core_tags.push_back("signal");
	beacon_core_tags.push_back("null_infrastructure");
	beacon_core_tags.push_back("interactable_anchor");
	d["tags"] = beacon_core_tags;
	reg("beacon_core", 14, d);

	// 15: IRON ORE
	d.clear();
	d["color"] = Color(0.48f, 0.42f, 0.38f);
	d["resource_id"] = "iron_ore";
	d["hand_break_time"] = 7.0f;
	PackedStringArray iron_ore_tags;
	iron_ore_tags.push_back("ore");
	iron_ore_tags.push_back("metal");
	d["tags"] = iron_ore_tags;
	reg("iron_ore", 15, d);

	// 16: COPPER ORE
	d.clear();
	d["color"] = Color(0.62f, 0.36f, 0.24f);
	d["resource_id"] = "copper_ore";
	d["hand_break_time"] = 6.5f;
	PackedStringArray copper_ore_tags;
	copper_ore_tags.push_back("ore");
	copper_ore_tags.push_back("metal");
	d["tags"] = copper_ore_tags;
	reg("copper_ore", 16, d);
}

// --- Mining / survival implementations ---

void VoxelBlockRegistry::set_block_breakable(int p_id, bool p_breakable) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].breakable = p_breakable;
	emit_changed();
}
bool VoxelBlockRegistry::get_block_breakable(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, false);
	return blocks[p_id].breakable;
}

void VoxelBlockRegistry::set_block_requires_tool(int p_id, bool p_requires_tool) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].requires_tool = p_requires_tool;
	emit_changed();
}
bool VoxelBlockRegistry::get_block_requires_tool(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, false);
	return blocks[p_id].requires_tool;
}

void VoxelBlockRegistry::set_block_hand_break_time(int p_id, float p_time) {
	REGISTRY_ENSURE_BLOCK(p_id);
	blocks.write[p_id].hand_break_time = MAX(p_time, 0.0f);
	emit_changed();
}
float VoxelBlockRegistry::get_block_hand_break_time(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, 5.0f);
	return blocks[p_id].hand_break_time;
}

int VoxelBlockRegistry::add_block_tool_entry(int p_id, const StringName &p_tool_type, int p_min_tier, float p_break_time) {
	REGISTRY_ENSURE_BLOCK_V(p_id, -1);
	BlockToolEntry entry;
	entry.tool_type = p_tool_type;
	entry.min_tier = MAX(p_min_tier, 0);
	entry.break_time = MAX(p_break_time, 0.0f);
	blocks.write[p_id].tool_entries.push_back(entry);
	emit_changed();
	return blocks[p_id].tool_entries.size() - 1;
}

void VoxelBlockRegistry::remove_block_tool_entry(int p_id, int p_entry_idx) {
	REGISTRY_ENSURE_BLOCK(p_id);
	ERR_FAIL_INDEX(p_entry_idx, blocks[p_id].tool_entries.size());
	blocks.write[p_id].tool_entries.remove_at(p_entry_idx);
	emit_changed();
}

int VoxelBlockRegistry::get_block_tool_entry_count(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, 0);
	return blocks[p_id].tool_entries.size();
}

Array VoxelBlockRegistry::get_block_tool_entries(int p_id) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, Array());
	Array result;
	for (const BlockToolEntry &te : blocks[p_id].tool_entries) {
		Dictionary d;
		d["tool_type"] = String(te.tool_type);
		d["min_tier"] = te.min_tier;
		d["break_time"] = te.break_time;
		result.push_back(d);
	}
	return result;
}

float VoxelBlockRegistry::get_block_break_time_for_tool(int p_id, const StringName &p_tool_type, int p_tool_tier) const {
	REGISTRY_ENSURE_BLOCK_V(p_id, VOXEL_BREAK_IMPOSSIBLE);
	const BlockEntry &entry = blocks[p_id];

	if (!entry.breakable) {
		return VOXEL_BREAK_IMPOSSIBLE;
	}

	// Find the best matching tool entry: type matches (or wildcard) AND min_tier <= tool_tier.
	// Among all matches pick the one with the highest min_tier (most specific).
	int best_tier = -1;
	float best_time = VOXEL_BREAK_IMPOSSIBLE;

	for (const BlockToolEntry &te : entry.tool_entries) {
		bool type_match = te.tool_type.is_empty() || te.tool_type == p_tool_type;
		if (!type_match) {
			continue;
		}
		if (te.min_tier > p_tool_tier) {
			continue; // Tool tier too low for this entry.
		}
		if (te.min_tier > best_tier) {
			best_tier = te.min_tier;
			best_time = te.break_time;
		}
	}

	if (best_tier >= 0) {
		return best_time;
	}

	// No matching tool entry.
	if (entry.requires_tool) {
		return VOXEL_BREAK_IMPOSSIBLE;
	}
	return entry.hand_break_time;
}
