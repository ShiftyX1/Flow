#include "voxel_structure_registry.h"

#include "core/object/class_db.h"
#include "core/variant/array.h"

PackedInt32Array VoxelStructureRegistry::_default_rotations() {
	PackedInt32Array rotations;
	rotations.push_back(0);
	return rotations;
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

static PackedInt32Array _packed_ints_from_variant(const Variant &p_value) {
	if (p_value.get_type() == Variant::PACKED_INT32_ARRAY) {
		return p_value;
	}
	PackedInt32Array out;
	if (p_value.get_type() == Variant::ARRAY) {
		Array array = p_value;
		for (int i = 0; i < array.size(); i++) {
			out.push_back((int)array[i]);
		}
	} else if (p_value.is_num()) {
		out.push_back((int)p_value);
	}
	return out;
}

VoxelStructureRegistry::StructureEntry VoxelStructureRegistry::_entry_from_properties(const String &p_name, const Dictionary &p_properties) {
	StructureEntry entry;
	entry.name = p_name;
	entry.allowed_rotations = _default_rotations();

	if (p_properties.has("voxel_data")) {
		entry.voxel_data = p_properties["voxel_data"];
	}
	if (entry.voxel_data.is_valid()) {
		entry.size = entry.voxel_data->get_size();
	}
	if (p_properties.has("rarity")) {
		entry.rarity = MAX((int)p_properties["rarity"], 0);
	}
	if (p_properties.has("biome_tags")) {
		entry.biome_tags = _packed_strings_from_variant(p_properties["biome_tags"]);
	}
	if (p_properties.has("layer_tags")) {
		entry.layer_tags = _packed_strings_from_variant(p_properties["layer_tags"]);
	}
	if (p_properties.has("size")) {
		entry.size = p_properties["size"];
	}
	if (p_properties.has("anchor")) {
		entry.anchor = p_properties["anchor"];
	}
	if (p_properties.has("allowed_rotations")) {
		entry.allowed_rotations = _packed_ints_from_variant(p_properties["allowed_rotations"]);
		if (entry.allowed_rotations.is_empty()) {
			entry.allowed_rotations = _default_rotations();
		}
	}
	if (p_properties.has("world_object_type")) {
		entry.world_object_type = StringName(String(p_properties["world_object_type"]));
	}
	if (p_properties.has("world_object_state")) {
		entry.world_object_state = p_properties["world_object_state"];
	}
	if (p_properties.has("world_object_blocking")) {
		entry.world_object_blocking = p_properties["world_object_blocking"];
	}
	return entry;
}

Dictionary VoxelStructureRegistry::_entry_to_dictionary(const StructureEntry &p_entry) {
	Dictionary d;
	d["name"] = p_entry.name;
	d["voxel_data"] = p_entry.voxel_data;
	d["rarity"] = p_entry.rarity;
	d["biome_tags"] = p_entry.biome_tags;
	d["layer_tags"] = p_entry.layer_tags;
	d["size"] = p_entry.size;
	d["anchor"] = p_entry.anchor;
	d["allowed_rotations"] = p_entry.allowed_rotations;
	d["world_object_type"] = String(p_entry.world_object_type);
	d["world_object_state"] = p_entry.world_object_state;
	d["world_object_blocking"] = p_entry.world_object_blocking;
	return d;
}

void VoxelStructureRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_structure", "name", "properties"), &VoxelStructureRegistry::add_structure, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("remove_structure", "id"), &VoxelStructureRegistry::remove_structure);
	ClassDB::bind_method(D_METHOD("clear"), &VoxelStructureRegistry::clear);
	ClassDB::bind_method(D_METHOD("setup_defaults"), &VoxelStructureRegistry::setup_defaults);
	ClassDB::bind_method(D_METHOD("get_structure_count"), &VoxelStructureRegistry::get_structure_count);
	ClassDB::bind_method(D_METHOD("has_structure", "id"), &VoxelStructureRegistry::has_structure);
	ClassDB::bind_method(D_METHOD("get_structure_name", "id"), &VoxelStructureRegistry::get_structure_name);
	ClassDB::bind_method(D_METHOD("get_structure", "id"), &VoxelStructureRegistry::get_structure);
	ClassDB::bind_method(D_METHOD("set_structure_voxel_data", "id", "data"), &VoxelStructureRegistry::set_structure_voxel_data);
	ClassDB::bind_method(D_METHOD("get_structure_voxel_data", "id"), &VoxelStructureRegistry::get_structure_voxel_data);
	ClassDB::bind_method(D_METHOD("set_structure_rarity", "id", "rarity"), &VoxelStructureRegistry::set_structure_rarity);
	ClassDB::bind_method(D_METHOD("get_structure_rarity", "id"), &VoxelStructureRegistry::get_structure_rarity);
	ClassDB::bind_method(D_METHOD("set_structure_biome_tags", "id", "tags"), &VoxelStructureRegistry::set_structure_biome_tags);
	ClassDB::bind_method(D_METHOD("get_structure_biome_tags", "id"), &VoxelStructureRegistry::get_structure_biome_tags);
	ClassDB::bind_method(D_METHOD("set_structure_layer_tags", "id", "tags"), &VoxelStructureRegistry::set_structure_layer_tags);
	ClassDB::bind_method(D_METHOD("get_structure_layer_tags", "id"), &VoxelStructureRegistry::get_structure_layer_tags);
	ClassDB::bind_method(D_METHOD("set_structure_anchor", "id", "anchor"), &VoxelStructureRegistry::set_structure_anchor);
	ClassDB::bind_method(D_METHOD("get_structure_anchor", "id"), &VoxelStructureRegistry::get_structure_anchor);
	ClassDB::bind_method(D_METHOD("set_structure_allowed_rotations", "id", "rotations"), &VoxelStructureRegistry::set_structure_allowed_rotations);
	ClassDB::bind_method(D_METHOD("get_structure_allowed_rotations", "id"), &VoxelStructureRegistry::get_structure_allowed_rotations);
	ClassDB::bind_method(D_METHOD("set_structure_world_object_type", "id", "type"), &VoxelStructureRegistry::set_structure_world_object_type);
	ClassDB::bind_method(D_METHOD("get_structure_world_object_type", "id"), &VoxelStructureRegistry::get_structure_world_object_type);
	ClassDB::bind_method(D_METHOD("set_structure_world_object_state", "id", "state"), &VoxelStructureRegistry::set_structure_world_object_state);
	ClassDB::bind_method(D_METHOD("get_structure_world_object_state", "id"), &VoxelStructureRegistry::get_structure_world_object_state);
	ClassDB::bind_method(D_METHOD("set_structure_world_object_blocking", "id", "blocking"), &VoxelStructureRegistry::set_structure_world_object_blocking);
	ClassDB::bind_method(D_METHOD("get_structure_world_object_blocking", "id"), &VoxelStructureRegistry::get_structure_world_object_blocking);
}

int VoxelStructureRegistry::add_structure(const String &p_name, const Dictionary &p_properties) {
	StructureEntry entry = _entry_from_properties(p_name, p_properties);
	structures.push_back(entry);
	emit_changed();
	return structures.size() - 1;
}

void VoxelStructureRegistry::remove_structure(int p_id) {
	ERR_FAIL_INDEX(p_id, structures.size());
	structures.remove_at(p_id);
	emit_changed();
}

void VoxelStructureRegistry::clear() {
	structures.clear();
	emit_changed();
}

void VoxelStructureRegistry::setup_defaults() {
	if (!structures.is_empty()) {
		return;
	}

	Ref<VoxelSceneData> beacon;
	beacon.instantiate();
	beacon->set_size(Vector3i(7, 5, 7));
	beacon->clear();

	for (int x = 1; x <= 5; x++) {
		for (int z = 1; z <= 5; z++) {
			if (x == 1 || x == 5 || z == 1 || z == 5 || ((x + z) % 3 == 0)) {
				beacon->set_block(x, 0, z, VOXEL_BLOCK_STONE);
			}
		}
	}

	beacon->set_block(2, 1, 1, VOXEL_BLOCK_RUSTED_PANEL);
	beacon->set_block(4, 1, 1, VOXEL_BLOCK_RUSTED_PANEL);
	beacon->set_block(1, 1, 4, VOXEL_BLOCK_RUSTED_PANEL);
	beacon->set_block(5, 1, 3, VOXEL_BLOCK_RUSTED_PANEL);
	beacon->set_block(3, 1, 3, VOXEL_BLOCK_BEACON_CORE);
	beacon->set_block(3, 2, 3, VOXEL_BLOCK_BEACON_CORE);
	beacon->set_block(2, 1, 3, VOXEL_BLOCK_BIO_RESIN);
	beacon->set_block(4, 1, 3, VOXEL_BLOCK_BIO_RESIN);
	beacon->set_block(1, 1, 5, VOXEL_BLOCK_BIOLUMEN_PLANT);
	beacon->set_block(5, 1, 1, VOXEL_BLOCK_BIOLUMEN_PLANT);
	beacon->set_block(5, 1, 5, VOXEL_BLOCK_BIOLUMEN_PLANT);

	PackedStringArray biome_tags;
	biome_tags.push_back("restored_forest");
	biome_tags.push_back("meadow");

	PackedStringArray layer_tags;
	layer_tags.push_back("new_biosphere");
	layer_tags.push_back("ruins");
	layer_tags.push_back("signals");

	PackedInt32Array rotations;
	rotations.push_back(0);
	rotations.push_back(90);
	rotations.push_back(180);
	rotations.push_back(270);

	Dictionary state;
	state["active"] = true;
	state["locked"] = false;
	state["signal"] = "distress";
	state["archive_hint"] = "restored_forest_entry";

	Dictionary props;
	props["voxel_data"] = beacon;
	props["rarity"] = 18;
	props["biome_tags"] = biome_tags;
	props["layer_tags"] = layer_tags;
	props["anchor"] = Vector3i(3, 1, 3);
	props["allowed_rotations"] = rotations;
	props["world_object_type"] = "emergency_beacon";
	props["world_object_state"] = state;
	props["world_object_blocking"] = false;
	add_structure("restored_forest_emergency_beacon", props);
}

int VoxelStructureRegistry::get_structure_count() const {
	return structures.size();
}

bool VoxelStructureRegistry::has_structure(int p_id) const {
	return p_id >= 0 && p_id < structures.size();
}

String VoxelStructureRegistry::get_structure_name(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), String());
	return structures[p_id].name;
}

Dictionary VoxelStructureRegistry::get_structure(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), Dictionary());
	return _entry_to_dictionary(structures[p_id]);
}

void VoxelStructureRegistry::set_structure_voxel_data(int p_id, const Ref<VoxelSceneData> &p_data) {
	ERR_FAIL_INDEX(p_id, structures.size());
	structures.write[p_id].voxel_data = p_data;
	if (p_data.is_valid()) {
		structures.write[p_id].size = p_data->get_size();
	}
	emit_changed();
}

Ref<VoxelSceneData> VoxelStructureRegistry::get_structure_voxel_data(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), Ref<VoxelSceneData>());
	return structures[p_id].voxel_data;
}

void VoxelStructureRegistry::set_structure_rarity(int p_id, int p_rarity) {
	ERR_FAIL_INDEX(p_id, structures.size());
	structures.write[p_id].rarity = MAX(p_rarity, 0);
	emit_changed();
}

int VoxelStructureRegistry::get_structure_rarity(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), 0);
	return structures[p_id].rarity;
}

void VoxelStructureRegistry::set_structure_biome_tags(int p_id, const PackedStringArray &p_tags) {
	ERR_FAIL_INDEX(p_id, structures.size());
	structures.write[p_id].biome_tags = p_tags;
	emit_changed();
}

PackedStringArray VoxelStructureRegistry::get_structure_biome_tags(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), PackedStringArray());
	return structures[p_id].biome_tags;
}

void VoxelStructureRegistry::set_structure_layer_tags(int p_id, const PackedStringArray &p_tags) {
	ERR_FAIL_INDEX(p_id, structures.size());
	structures.write[p_id].layer_tags = p_tags;
	emit_changed();
}

PackedStringArray VoxelStructureRegistry::get_structure_layer_tags(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), PackedStringArray());
	return structures[p_id].layer_tags;
}

void VoxelStructureRegistry::set_structure_anchor(int p_id, const Vector3i &p_anchor) {
	ERR_FAIL_INDEX(p_id, structures.size());
	structures.write[p_id].anchor = p_anchor;
	emit_changed();
}

Vector3i VoxelStructureRegistry::get_structure_anchor(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), Vector3i());
	return structures[p_id].anchor;
}

void VoxelStructureRegistry::set_structure_allowed_rotations(int p_id, const PackedInt32Array &p_rotations) {
	ERR_FAIL_INDEX(p_id, structures.size());
	structures.write[p_id].allowed_rotations = p_rotations.is_empty() ? _default_rotations() : p_rotations;
	emit_changed();
}

PackedInt32Array VoxelStructureRegistry::get_structure_allowed_rotations(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), PackedInt32Array());
	return structures[p_id].allowed_rotations;
}

void VoxelStructureRegistry::set_structure_world_object_type(int p_id, const StringName &p_type) {
	ERR_FAIL_INDEX(p_id, structures.size());
	structures.write[p_id].world_object_type = p_type;
	emit_changed();
}

StringName VoxelStructureRegistry::get_structure_world_object_type(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), StringName());
	return structures[p_id].world_object_type;
}

void VoxelStructureRegistry::set_structure_world_object_state(int p_id, const Dictionary &p_state) {
	ERR_FAIL_INDEX(p_id, structures.size());
	structures.write[p_id].world_object_state = p_state;
	emit_changed();
}

Dictionary VoxelStructureRegistry::get_structure_world_object_state(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), Dictionary());
	return structures[p_id].world_object_state;
}

void VoxelStructureRegistry::set_structure_world_object_blocking(int p_id, bool p_blocking) {
	ERR_FAIL_INDEX(p_id, structures.size());
	structures.write[p_id].world_object_blocking = p_blocking;
	emit_changed();
}

bool VoxelStructureRegistry::get_structure_world_object_blocking(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, structures.size(), false);
	return structures[p_id].world_object_blocking;
}
