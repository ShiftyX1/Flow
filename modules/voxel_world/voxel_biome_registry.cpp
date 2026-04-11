#include "voxel_biome_registry.h"

#include "core/object/class_db.h"

VoxelBiomeRegistry::VoxelBiomeRegistry() {}

void VoxelBiomeRegistry::add_biome(const Ref<VoxelBiomeData> &p_biome) {
	ERR_FAIL_COND(p_biome.is_null());
	biomes.push_back(p_biome);
	emit_changed();
}

void VoxelBiomeRegistry::remove_biome(int p_index) {
	ERR_FAIL_INDEX(p_index, biomes.size());
	biomes.remove_at(p_index);
	emit_changed();
}

Ref<VoxelBiomeData> VoxelBiomeRegistry::get_biome(int p_index) const {
	ERR_FAIL_INDEX_V(p_index, biomes.size(), Ref<VoxelBiomeData>());
	return biomes[p_index];
}

int VoxelBiomeRegistry::get_biome_count() const {
	return biomes.size();
}

void VoxelBiomeRegistry::clear_biomes() {
	biomes.clear();
	emit_changed();
}

void VoxelBiomeRegistry::set_biomes(const TypedArray<VoxelBiomeData> &p_biomes) {
	biomes = p_biomes;
	emit_changed();
}

TypedArray<VoxelBiomeData> VoxelBiomeRegistry::get_biomes() const {
	return biomes;
}

void VoxelBiomeRegistry::setup_defaults() {
	biomes.clear();

	struct DefaultBiome {
		const char *name;
		float temp, humid;
		float height_base, height_scale, detail_scale, density_3d_weight;
		int surface_block, subsurface_block;
		int tree_density, snow_line;
	};

	static const DefaultBiome defaults[] = {
		{ "Desert", 0.8f, -0.6f, 0.0f, 8.0f, 1.0f, 3.0f,
				VOXEL_BLOCK_SAND, VOXEL_BLOCK_SAND, 0, 999 },
		{ "Meadow", 0.2f, 0.0f, 0.0f, 15.0f, 3.0f, 5.0f,
				VOXEL_BLOCK_GRASS, VOXEL_BLOCK_DIRT, 120, 55 },
		{ "Forest", 0.0f, 0.6f, 2.0f, 18.0f, 4.0f, 5.0f,
				VOXEL_BLOCK_GRASS, VOXEL_BLOCK_DIRT, 25, 55 },
		{ "Mountains", -0.6f, 0.0f, 10.0f, 30.0f, 8.0f, 8.0f,
				VOXEL_BLOCK_STONE, VOXEL_BLOCK_STONE, 300, 40 },
	};

	for (int i = 0; i < 4; i++) {
		Ref<VoxelBiomeData> b;
		b.instantiate();
		b->set_biome_name(defaults[i].name);
		b->set_temperature_center(defaults[i].temp);
		b->set_humidity_center(defaults[i].humid);
		b->set_height_base(defaults[i].height_base);
		b->set_height_scale(defaults[i].height_scale);
		b->set_detail_scale(defaults[i].detail_scale);
		b->set_density_3d_weight(defaults[i].density_3d_weight);
		b->set_surface_block(defaults[i].surface_block);
		b->set_subsurface_block(defaults[i].subsurface_block);
		b->set_tree_density(defaults[i].tree_density);
		b->set_snow_line(defaults[i].snow_line);
		biomes.push_back(b);
	}

	emit_changed();
}

void VoxelBiomeRegistry::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_biome", "biome"), &VoxelBiomeRegistry::add_biome);
	ClassDB::bind_method(D_METHOD("remove_biome", "index"), &VoxelBiomeRegistry::remove_biome);
	ClassDB::bind_method(D_METHOD("get_biome", "index"), &VoxelBiomeRegistry::get_biome);
	ClassDB::bind_method(D_METHOD("get_biome_count"), &VoxelBiomeRegistry::get_biome_count);
	ClassDB::bind_method(D_METHOD("clear_biomes"), &VoxelBiomeRegistry::clear_biomes);
	ClassDB::bind_method(D_METHOD("set_biomes", "biomes"), &VoxelBiomeRegistry::set_biomes);
	ClassDB::bind_method(D_METHOD("get_biomes"), &VoxelBiomeRegistry::get_biomes);
	ClassDB::bind_method(D_METHOD("setup_defaults"), &VoxelBiomeRegistry::setup_defaults);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "biomes", PROPERTY_HINT_ARRAY_TYPE, "VoxelBiomeData"),
			"set_biomes", "get_biomes");
}
