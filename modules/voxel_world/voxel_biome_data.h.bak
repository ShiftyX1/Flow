#pragma once

#include "voxel_block_data.h"

#include "core/io/resource.h"

// Defines the parameters of a single biome: its position in temperature/humidity
// climate-space and all terrain-generation parameters.
// Used by VoxelBiomeRegistry to drive VoxelTerrainGenerator.
class VoxelBiomeData : public Resource {
	GDCLASS(VoxelBiomeData, Resource);

	String biome_name;

	// Climate-space position in [-1, 1]. Used for biome blending weight.
	float temperature_center = 0.0f;
	float humidity_center = 0.0f;

	// Terrain shape parameters (fed directly into VoxelTerrainGenerator).
	float height_base = 0.0f;
	float height_scale = 15.0f;
	float detail_scale = 3.0f;
	float density_3d_weight = 5.0f;

	// Block types placed at the surface and one-to-four blocks below it.
	int surface_block = VOXEL_BLOCK_GRASS;
	int subsurface_block = VOXEL_BLOCK_DIRT;

	// Tree placement: hash modulus (0 = no trees, lower = denser).
	int tree_density = 0;
	// Y-level above which snow replaces the surface block.
	int snow_line = 55;

protected:
	static void _bind_methods();

public:
	void set_biome_name(const String &p_name) { biome_name = p_name; }
	String get_biome_name() const { return biome_name; }

	void set_temperature_center(float p_val) { temperature_center = p_val; }
	float get_temperature_center() const { return temperature_center; }

	void set_humidity_center(float p_val) { humidity_center = p_val; }
	float get_humidity_center() const { return humidity_center; }

	void set_height_base(float p_val) { height_base = p_val; }
	float get_height_base() const { return height_base; }

	void set_height_scale(float p_val) { height_scale = p_val; }
	float get_height_scale() const { return height_scale; }

	void set_detail_scale(float p_val) { detail_scale = p_val; }
	float get_detail_scale() const { return detail_scale; }

	void set_density_3d_weight(float p_val) { density_3d_weight = p_val; }
	float get_density_3d_weight() const { return density_3d_weight; }

	void set_surface_block(int p_val) { surface_block = p_val; }
	int get_surface_block() const { return surface_block; }

	void set_subsurface_block(int p_val) { subsurface_block = p_val; }
	int get_subsurface_block() const { return subsurface_block; }

	void set_tree_density(int p_val) { tree_density = p_val; }
	int get_tree_density() const { return tree_density; }

	void set_snow_line(int p_val) { snow_line = p_val; }
	int get_snow_line() const { return snow_line; }

	VoxelBiomeData() {}
};
