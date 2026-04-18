#pragma once

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"

class VoxelBiomeRegistry : public RefCounted {
	GDCLASS(VoxelBiomeRegistry, RefCounted);

public:
	// Canopy shape types for tree features.
	enum CanopyShape {
		CANOPY_SPHERE = 0, // Current default: sphere with clipped corners.
		CANOPY_CONE, // Spruce/pine: tapered cone, wider at bottom.
		CANOPY_BUSH, // Low/wide bush: large radius, short.
		CANOPY_MAX
	};

	// A single feature to place in a biome (tree, scatter block, etc.).
	struct FeatureConfig {
		enum Type {
			FEATURE_TREE = 0,
			FEATURE_SCATTER,
		};

		Type type = FEATURE_TREE;

		// Tree params.
		int trunk_block = 7; // VOXEL_BLOCK_WOOD
		int canopy_block = 8; // VOXEL_BLOCK_LEAVES
		int density = 0; // Hash modulus (lower = denser, 0 = none).
		int min_trunk = 4;
		int max_trunk = 6;
		int canopy_radius = 2;
		CanopyShape canopy_shape = CANOPY_SPHERE;

		// Scatter params.
		int block = 0;
		int min_y = 0;
		int max_y = 192;
		bool surface_only = true;
	};

	struct BiomeEntry {
		String name;

		// Terrain shape.
		float height_base = 0.0f;
		float height_scale = 15.0f;
		float detail_scale = 3.0f;
		float density_3d_weight = 0.5f;

		// Climate position in (temperature, humidity) space [-1, 1].
		float temperature = 0.0f;
		float humidity = 0.0f;

		// Block overrides.
		int surface_block = 1; // VOXEL_BLOCK_GRASS
		int subsurface_block = 2; // VOXEL_BLOCK_DIRT
		int shore_block = 4; // VOXEL_BLOCK_SAND
		int snow_block = 6; // VOXEL_BLOCK_SNOW

		int snow_line = 165;

		// Features (trees, scatter, etc.).
		Vector<FeatureConfig> features;
	};

private:
	Vector<BiomeEntry> biomes;
	HashMap<String, int> name_to_id;

	static FeatureConfig _parse_feature(const Dictionary &p_dict);
	static Dictionary _feature_to_dict(const FeatureConfig &p_feature);

protected:
	static void _bind_methods();

public:
	// Register a biome with a Dictionary of properties. Returns the biome ID.
	int add_biome(const String &p_name, const Dictionary &p_properties = Dictionary());

	// Remove a biome by ID. Shifts subsequent IDs down.
	void remove_biome(int p_id);

	// Remove all biomes.
	void clear();

	// Populate with the 4 built-in biomes (desert, meadow, forest, mountains).
	void setup_defaults();

	// Query.
	int get_biome_count() const;
	int get_biome_id(const String &p_name) const;
	String get_biome_name(int p_id) const;
	bool has_biome(int p_id) const;

	// Per-biome property accessors.
	void set_biome_height_base(int p_id, float p_value);
	float get_biome_height_base(int p_id) const;

	void set_biome_height_scale(int p_id, float p_value);
	float get_biome_height_scale(int p_id) const;

	void set_biome_detail_scale(int p_id, float p_value);
	float get_biome_detail_scale(int p_id) const;

	void set_biome_density_3d_weight(int p_id, float p_value);
	float get_biome_density_3d_weight(int p_id) const;

	void set_biome_temperature(int p_id, float p_value);
	float get_biome_temperature(int p_id) const;

	void set_biome_humidity(int p_id, float p_value);
	float get_biome_humidity(int p_id) const;

	void set_biome_surface_block(int p_id, int p_block);
	int get_biome_surface_block(int p_id) const;

	void set_biome_subsurface_block(int p_id, int p_block);
	int get_biome_subsurface_block(int p_id) const;

	void set_biome_shore_block(int p_id, int p_block);
	int get_biome_shore_block(int p_id) const;

	void set_biome_snow_block(int p_id, int p_block);
	int get_biome_snow_block(int p_id) const;

	void set_biome_snow_line(int p_id, int p_value);
	int get_biome_snow_line(int p_id) const;

	// Feature management.
	void add_biome_feature(int p_id, const Dictionary &p_feature);
	int get_biome_feature_count(int p_id) const;
	Dictionary get_biome_feature(int p_id, int p_feature_index) const;
	void clear_biome_features(int p_id);

	// Direct access for C++ (generator integration).
	const Vector<BiomeEntry> &get_biomes() const { return biomes; }
};

VARIANT_ENUM_CAST(VoxelBiomeRegistry::CanopyShape);
