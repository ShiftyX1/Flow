#include "voxel_biome_data.h"

#include "core/object/class_db.h"

void VoxelBiomeData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_biome_name", "name"), &VoxelBiomeData::set_biome_name);
	ClassDB::bind_method(D_METHOD("get_biome_name"), &VoxelBiomeData::get_biome_name);

	ClassDB::bind_method(D_METHOD("set_temperature_center", "value"), &VoxelBiomeData::set_temperature_center);
	ClassDB::bind_method(D_METHOD("get_temperature_center"), &VoxelBiomeData::get_temperature_center);

	ClassDB::bind_method(D_METHOD("set_humidity_center", "value"), &VoxelBiomeData::set_humidity_center);
	ClassDB::bind_method(D_METHOD("get_humidity_center"), &VoxelBiomeData::get_humidity_center);

	ClassDB::bind_method(D_METHOD("set_height_base", "value"), &VoxelBiomeData::set_height_base);
	ClassDB::bind_method(D_METHOD("get_height_base"), &VoxelBiomeData::get_height_base);

	ClassDB::bind_method(D_METHOD("set_height_scale", "value"), &VoxelBiomeData::set_height_scale);
	ClassDB::bind_method(D_METHOD("get_height_scale"), &VoxelBiomeData::get_height_scale);

	ClassDB::bind_method(D_METHOD("set_detail_scale", "value"), &VoxelBiomeData::set_detail_scale);
	ClassDB::bind_method(D_METHOD("get_detail_scale"), &VoxelBiomeData::get_detail_scale);

	ClassDB::bind_method(D_METHOD("set_density_3d_weight", "value"), &VoxelBiomeData::set_density_3d_weight);
	ClassDB::bind_method(D_METHOD("get_density_3d_weight"), &VoxelBiomeData::get_density_3d_weight);

	ClassDB::bind_method(D_METHOD("set_surface_block", "block_id"), &VoxelBiomeData::set_surface_block);
	ClassDB::bind_method(D_METHOD("get_surface_block"), &VoxelBiomeData::get_surface_block);

	ClassDB::bind_method(D_METHOD("set_subsurface_block", "block_id"), &VoxelBiomeData::set_subsurface_block);
	ClassDB::bind_method(D_METHOD("get_subsurface_block"), &VoxelBiomeData::get_subsurface_block);

	ClassDB::bind_method(D_METHOD("set_tree_density", "density"), &VoxelBiomeData::set_tree_density);
	ClassDB::bind_method(D_METHOD("get_tree_density"), &VoxelBiomeData::get_tree_density);

	ClassDB::bind_method(D_METHOD("set_snow_line", "y_level"), &VoxelBiomeData::set_snow_line);
	ClassDB::bind_method(D_METHOD("get_snow_line"), &VoxelBiomeData::get_snow_line);

	const String block_hint = "Air,Grass,Dirt,Stone,Sand,Water,Snow,Wood,Leaves,Bedrock";

	ADD_GROUP("Identity", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "biome_name"), "set_biome_name", "get_biome_name");

	ADD_GROUP("Climate", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "temperature_center", PROPERTY_HINT_RANGE, "-1.0,1.0,0.01"), "set_temperature_center", "get_temperature_center");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "humidity_center", PROPERTY_HINT_RANGE, "-1.0,1.0,0.01"), "set_humidity_center", "get_humidity_center");

	ADD_GROUP("Terrain Shape", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_base", PROPERTY_HINT_RANGE, "-32.0,32.0,0.1"), "set_height_base", "get_height_base");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_scale", PROPERTY_HINT_RANGE, "0.0,64.0,0.1"), "set_height_scale", "get_height_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "detail_scale", PROPERTY_HINT_RANGE, "0.0,16.0,0.1"), "set_detail_scale", "get_detail_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density_3d_weight", PROPERTY_HINT_RANGE, "0.0,16.0,0.1"), "set_density_3d_weight", "get_density_3d_weight");

	ADD_GROUP("Blocks", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "surface_block", PROPERTY_HINT_ENUM, block_hint), "set_surface_block", "get_surface_block");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "subsurface_block", PROPERTY_HINT_ENUM, block_hint), "set_subsurface_block", "get_subsurface_block");

	ADD_GROUP("Features", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tree_density", PROPERTY_HINT_RANGE, "0,999,1"), "set_tree_density", "get_tree_density");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "snow_line", PROPERTY_HINT_RANGE, "0,63,1"), "set_snow_line", "get_snow_line");
}
