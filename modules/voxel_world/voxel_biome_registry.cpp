#include "voxel_biome_registry.h"

#include "voxel_block_registry.h" // For VOXEL_BLOCK_* constants.

#include "core/object/class_db.h"
#include "core/variant/variant.h"

// ===========================================================================
// Feature parsing helpers
// ===========================================================================

VoxelBiomeRegistry::FeatureConfig VoxelBiomeRegistry::_parse_feature(const Dictionary &p_dict) {
	FeatureConfig fc;

	String type_str = p_dict.get("type", "tree");
	if (type_str == "scatter") {
		fc.type = FeatureConfig::FEATURE_SCATTER;
		fc.block = p_dict.get("block", 0);
		fc.density = p_dict.get("density", 0);
		fc.min_y = p_dict.get("min_y", 0);
		fc.max_y = p_dict.get("max_y", 192);
		fc.surface_only = p_dict.get("surface_only", true);
	} else {
		// "tree" or default.
		fc.type = FeatureConfig::FEATURE_TREE;
		fc.trunk_block = p_dict.get("trunk_block", VOXEL_BLOCK_WOOD);
		fc.canopy_block = p_dict.get("canopy_block", VOXEL_BLOCK_LEAVES);
		fc.density = p_dict.get("density", 0);
		fc.min_trunk = p_dict.get("min_trunk", 4);
		fc.max_trunk = p_dict.get("max_trunk", 6);
		fc.canopy_radius = p_dict.get("canopy_radius", 2);

		String shape_str = p_dict.get("canopy_shape", "sphere");
		if (shape_str == "cone") {
			fc.canopy_shape = CANOPY_CONE;
		} else if (shape_str == "bush") {
			fc.canopy_shape = CANOPY_BUSH;
		} else {
			fc.canopy_shape = CANOPY_SPHERE;
		}
	}

	return fc;
}

Dictionary VoxelBiomeRegistry::_feature_to_dict(const FeatureConfig &p_feature) {
	Dictionary d;

	if (p_feature.type == FeatureConfig::FEATURE_SCATTER) {
		d["type"] = "scatter";
		d["block"] = p_feature.block;
		d["density"] = p_feature.density;
		d["min_y"] = p_feature.min_y;
		d["max_y"] = p_feature.max_y;
		d["surface_only"] = p_feature.surface_only;
	} else {
		d["type"] = "tree";
		d["trunk_block"] = p_feature.trunk_block;
		d["canopy_block"] = p_feature.canopy_block;
		d["density"] = p_feature.density;
		d["min_trunk"] = p_feature.min_trunk;
		d["max_trunk"] = p_feature.max_trunk;
		d["canopy_radius"] = p_feature.canopy_radius;

		switch (p_feature.canopy_shape) {
			case CANOPY_CONE:
				d["canopy_shape"] = "cone";
				break;
			case CANOPY_BUSH:
				d["canopy_shape"] = "bush";
				break;
			default:
				d["canopy_shape"] = "sphere";
				break;
		}
	}

	return d;
}

// ===========================================================================
// Registration API
// ===========================================================================

int VoxelBiomeRegistry::add_biome(const String &p_name, const Dictionary &p_properties) {
	ERR_FAIL_COND_V_MSG(p_name.is_empty(), -1, "Biome name cannot be empty.");
	ERR_FAIL_COND_V_MSG(name_to_id.has(p_name), -1, "Biome '" + p_name + "' already registered.");

	BiomeEntry entry;
	entry.name = p_name;

	// Terrain.
	if (p_properties.has("height_base")) {
		entry.height_base = p_properties["height_base"];
	}
	if (p_properties.has("height_scale")) {
		entry.height_scale = p_properties["height_scale"];
	}
	if (p_properties.has("detail_scale")) {
		entry.detail_scale = p_properties["detail_scale"];
	}
	if (p_properties.has("density_3d_weight")) {
		entry.density_3d_weight = p_properties["density_3d_weight"];
	}

	// Climate.
	if (p_properties.has("temperature")) {
		entry.temperature = p_properties["temperature"];
	}
	if (p_properties.has("humidity")) {
		entry.humidity = p_properties["humidity"];
	}

	// Blocks.
	if (p_properties.has("surface_block")) {
		entry.surface_block = p_properties["surface_block"];
	}
	if (p_properties.has("subsurface_block")) {
		entry.subsurface_block = p_properties["subsurface_block"];
	}
	if (p_properties.has("shore_block")) {
		entry.shore_block = p_properties["shore_block"];
	}
	if (p_properties.has("snow_block")) {
		entry.snow_block = p_properties["snow_block"];
	}
	if (p_properties.has("snow_line")) {
		entry.snow_line = p_properties["snow_line"];
	}

	// Features.
	if (p_properties.has("features")) {
		Array features_arr = p_properties["features"];
		for (int i = 0; i < features_arr.size(); i++) {
			Dictionary fd = features_arr[i];
			entry.features.push_back(_parse_feature(fd));
		}
	}

	int id = biomes.size();
	biomes.push_back(entry);
	name_to_id[p_name] = id;
	return id;
}

void VoxelBiomeRegistry::remove_biome(int p_id) {
	ERR_FAIL_INDEX(p_id, biomes.size());

	String name = biomes[p_id].name;
	biomes.remove_at(p_id);
	name_to_id.erase(name);

	// Rebuild name_to_id for shifted entries.
	for (int i = p_id; i < biomes.size(); i++) {
		name_to_id[biomes[i].name] = i;
	}
}

void VoxelBiomeRegistry::clear() {
	biomes.clear();
	name_to_id.clear();
}

void VoxelBiomeRegistry::setup_defaults() {
	if (biomes.size() > 0) {
		return; // Already populated.
	}

	// Single default biome: Meadow.
	{
		Dictionary props;
		props["height_base"] = 0.0f;
		props["height_scale"] = 15.0f;
		props["detail_scale"] = 3.0f;
		props["density_3d_weight"] = 0.5f;
		props["temperature"] = 0.0f;
		props["humidity"] = 0.0f;
		props["surface_block"] = VOXEL_BLOCK_GRASS;
		props["subsurface_block"] = VOXEL_BLOCK_DIRT;
		props["shore_block"] = VOXEL_BLOCK_SAND;
		props["snow_block"] = VOXEL_BLOCK_SNOW;
		props["snow_line"] = 165;

		Array features;
		Dictionary tree;
		tree["type"] = "tree";
		tree["trunk_block"] = VOXEL_BLOCK_WOOD;
		tree["canopy_block"] = VOXEL_BLOCK_LEAVES;
		tree["density"] = 400;
		tree["canopy_shape"] = "sphere";
		features.push_back(tree);
		props["features"] = features;

		add_biome("meadow", props);
	}
}

// ===========================================================================
// Query
// ===========================================================================

int VoxelBiomeRegistry::get_biome_count() const {
	return biomes.size();
}

int VoxelBiomeRegistry::get_biome_id(const String &p_name) const {
	const int *id = name_to_id.getptr(p_name);
	return id ? *id : -1;
}

String VoxelBiomeRegistry::get_biome_name(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, biomes.size(), String());
	return biomes[p_id].name;
}

bool VoxelBiomeRegistry::has_biome(int p_id) const {
	return p_id >= 0 && p_id < biomes.size();
}

// ===========================================================================
// Per-biome property accessors
// ===========================================================================

#define BIOME_SETTER(prop, type)                                   \
	void VoxelBiomeRegistry::set_biome_##prop(int p_id, type p_value) { \
		ERR_FAIL_INDEX(p_id, biomes.size());                       \
		biomes.write[p_id].prop = p_value;                         \
	}

#define BIOME_GETTER(prop, type, default_val)                            \
	type VoxelBiomeRegistry::get_biome_##prop(int p_id) const {          \
		ERR_FAIL_INDEX_V(p_id, biomes.size(), default_val);              \
		return biomes[p_id].prop;                                        \
	}

BIOME_SETTER(height_base, float)
BIOME_GETTER(height_base, float, 0.0f)

BIOME_SETTER(height_scale, float)
BIOME_GETTER(height_scale, float, 15.0f)

BIOME_SETTER(detail_scale, float)
BIOME_GETTER(detail_scale, float, 3.0f)

BIOME_SETTER(density_3d_weight, float)
BIOME_GETTER(density_3d_weight, float, 0.5f)

BIOME_SETTER(temperature, float)
BIOME_GETTER(temperature, float, 0.0f)

BIOME_SETTER(humidity, float)
BIOME_GETTER(humidity, float, 0.0f)

BIOME_SETTER(surface_block, int)
BIOME_GETTER(surface_block, int, 1)

BIOME_SETTER(subsurface_block, int)
BIOME_GETTER(subsurface_block, int, 2)

BIOME_SETTER(shore_block, int)
BIOME_GETTER(shore_block, int, 4)

BIOME_SETTER(snow_block, int)
BIOME_GETTER(snow_block, int, 6)

BIOME_SETTER(snow_line, int)
BIOME_GETTER(snow_line, int, 165)

#undef BIOME_SETTER
#undef BIOME_GETTER

// ===========================================================================
// Feature management
// ===========================================================================

void VoxelBiomeRegistry::add_biome_feature(int p_id, const Dictionary &p_feature) {
	ERR_FAIL_INDEX(p_id, biomes.size());
	biomes.write[p_id].features.push_back(_parse_feature(p_feature));
}

int VoxelBiomeRegistry::get_biome_feature_count(int p_id) const {
	ERR_FAIL_INDEX_V(p_id, biomes.size(), 0);
	return biomes[p_id].features.size();
}

Dictionary VoxelBiomeRegistry::get_biome_feature(int p_id, int p_feature_index) const {
	ERR_FAIL_INDEX_V(p_id, biomes.size(), Dictionary());
	ERR_FAIL_INDEX_V(p_feature_index, biomes[p_id].features.size(), Dictionary());
	return _feature_to_dict(biomes[p_id].features[p_feature_index]);
}

void VoxelBiomeRegistry::clear_biome_features(int p_id) {
	ERR_FAIL_INDEX(p_id, biomes.size());
	biomes.write[p_id].features.clear();
}

// ===========================================================================
// Bind methods
// ===========================================================================

void VoxelBiomeRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_biome", "name", "properties"), &VoxelBiomeRegistry::add_biome, DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("remove_biome", "id"), &VoxelBiomeRegistry::remove_biome);
	ClassDB::bind_method(D_METHOD("clear"), &VoxelBiomeRegistry::clear);
	ClassDB::bind_method(D_METHOD("setup_defaults"), &VoxelBiomeRegistry::setup_defaults);

	ClassDB::bind_method(D_METHOD("get_biome_count"), &VoxelBiomeRegistry::get_biome_count);
	ClassDB::bind_method(D_METHOD("get_biome_id", "name"), &VoxelBiomeRegistry::get_biome_id);
	ClassDB::bind_method(D_METHOD("get_biome_name", "id"), &VoxelBiomeRegistry::get_biome_name);
	ClassDB::bind_method(D_METHOD("has_biome", "id"), &VoxelBiomeRegistry::has_biome);

	// Per-biome property accessors.
	ClassDB::bind_method(D_METHOD("set_biome_height_base", "id", "value"), &VoxelBiomeRegistry::set_biome_height_base);
	ClassDB::bind_method(D_METHOD("get_biome_height_base", "id"), &VoxelBiomeRegistry::get_biome_height_base);

	ClassDB::bind_method(D_METHOD("set_biome_height_scale", "id", "value"), &VoxelBiomeRegistry::set_biome_height_scale);
	ClassDB::bind_method(D_METHOD("get_biome_height_scale", "id"), &VoxelBiomeRegistry::get_biome_height_scale);

	ClassDB::bind_method(D_METHOD("set_biome_detail_scale", "id", "value"), &VoxelBiomeRegistry::set_biome_detail_scale);
	ClassDB::bind_method(D_METHOD("get_biome_detail_scale", "id"), &VoxelBiomeRegistry::get_biome_detail_scale);

	ClassDB::bind_method(D_METHOD("set_biome_density_3d_weight", "id", "value"), &VoxelBiomeRegistry::set_biome_density_3d_weight);
	ClassDB::bind_method(D_METHOD("get_biome_density_3d_weight", "id"), &VoxelBiomeRegistry::get_biome_density_3d_weight);

	ClassDB::bind_method(D_METHOD("set_biome_temperature", "id", "value"), &VoxelBiomeRegistry::set_biome_temperature);
	ClassDB::bind_method(D_METHOD("get_biome_temperature", "id"), &VoxelBiomeRegistry::get_biome_temperature);

	ClassDB::bind_method(D_METHOD("set_biome_humidity", "id", "value"), &VoxelBiomeRegistry::set_biome_humidity);
	ClassDB::bind_method(D_METHOD("get_biome_humidity", "id"), &VoxelBiomeRegistry::get_biome_humidity);

	ClassDB::bind_method(D_METHOD("set_biome_surface_block", "id", "block"), &VoxelBiomeRegistry::set_biome_surface_block);
	ClassDB::bind_method(D_METHOD("get_biome_surface_block", "id"), &VoxelBiomeRegistry::get_biome_surface_block);

	ClassDB::bind_method(D_METHOD("set_biome_subsurface_block", "id", "block"), &VoxelBiomeRegistry::set_biome_subsurface_block);
	ClassDB::bind_method(D_METHOD("get_biome_subsurface_block", "id"), &VoxelBiomeRegistry::get_biome_subsurface_block);

	ClassDB::bind_method(D_METHOD("set_biome_shore_block", "id", "block"), &VoxelBiomeRegistry::set_biome_shore_block);
	ClassDB::bind_method(D_METHOD("get_biome_shore_block", "id"), &VoxelBiomeRegistry::get_biome_shore_block);

	ClassDB::bind_method(D_METHOD("set_biome_snow_block", "id", "block"), &VoxelBiomeRegistry::set_biome_snow_block);
	ClassDB::bind_method(D_METHOD("get_biome_snow_block", "id"), &VoxelBiomeRegistry::get_biome_snow_block);

	ClassDB::bind_method(D_METHOD("set_biome_snow_line", "id", "value"), &VoxelBiomeRegistry::set_biome_snow_line);
	ClassDB::bind_method(D_METHOD("get_biome_snow_line", "id"), &VoxelBiomeRegistry::get_biome_snow_line);

	// Feature management.
	ClassDB::bind_method(D_METHOD("add_biome_feature", "id", "feature"), &VoxelBiomeRegistry::add_biome_feature);
	ClassDB::bind_method(D_METHOD("get_biome_feature_count", "id"), &VoxelBiomeRegistry::get_biome_feature_count);
	ClassDB::bind_method(D_METHOD("get_biome_feature", "id", "feature_index"), &VoxelBiomeRegistry::get_biome_feature);
	ClassDB::bind_method(D_METHOD("clear_biome_features", "id"), &VoxelBiomeRegistry::clear_biome_features);

	// Canopy shape enum.
	BIND_ENUM_CONSTANT(CANOPY_SPHERE);
	BIND_ENUM_CONSTANT(CANOPY_CONE);
	BIND_ENUM_CONSTANT(CANOPY_BUSH);
}
