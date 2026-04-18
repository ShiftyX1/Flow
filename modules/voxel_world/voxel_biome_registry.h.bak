#pragma once

#include "voxel_biome_data.h"

#include "core/io/resource.h"
#include "core/variant/typed_array.h"

// Registry of biomes used by VoxelTerrainGenerator.
// Assign a populated registry to VoxelWorld.biome_registry before play;
// call setup_defaults() to pre-fill the four built-in biomes.
class VoxelBiomeRegistry : public Resource {
	GDCLASS(VoxelBiomeRegistry, Resource);

	TypedArray<VoxelBiomeData> biomes;

protected:
	static void _bind_methods();

public:
	void add_biome(const Ref<VoxelBiomeData> &p_biome);
	void remove_biome(int p_index);
	Ref<VoxelBiomeData> get_biome(int p_index) const;
	int get_biome_count() const;
	void clear_biomes();

	// Array property for serialization (saves/loads all biomes as a .tres resource).
	void set_biomes(const TypedArray<VoxelBiomeData> &p_biomes);
	TypedArray<VoxelBiomeData> get_biomes() const;

	// Populate with the four built-in biomes (Desert, Meadow, Forest, Mountains).
	void setup_defaults();

	VoxelBiomeRegistry();
};
