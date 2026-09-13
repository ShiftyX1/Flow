#pragma once

#include "voxel_scene_data.h"

#include "core/io/resource.h"
#include "core/math/vector3i.h"
#include "core/string/string_name.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

class VoxelStructureRegistry : public Resource {
	GDCLASS(VoxelStructureRegistry, Resource);

public:
	struct StructureEntry {
		String name;
		Ref<VoxelSceneData> voxel_data;
		int rarity = 0; // 0 = disabled, N = approximately one candidate per N anchor chunks.
		PackedStringArray biome_tags;
		PackedStringArray layer_tags;
		Vector3i size;
		Vector3i anchor;
		PackedInt32Array allowed_rotations;
		StringName world_object_type;
		Dictionary world_object_state;
		bool world_object_blocking = false;
	};

private:
	Vector<StructureEntry> structures;

	static PackedInt32Array _default_rotations();
	static StructureEntry _entry_from_properties(const String &p_name, const Dictionary &p_properties);
	static Dictionary _entry_to_dictionary(const StructureEntry &p_entry);

protected:
	static void _bind_methods();

public:
	int add_structure(const String &p_name, const Dictionary &p_properties = Dictionary());
	void remove_structure(int p_id);
	void clear();
	void setup_defaults();

	int get_structure_count() const;
	bool has_structure(int p_id) const;
	String get_structure_name(int p_id) const;
	Dictionary get_structure(int p_id) const;

	void set_structure_voxel_data(int p_id, const Ref<VoxelSceneData> &p_data);
	Ref<VoxelSceneData> get_structure_voxel_data(int p_id) const;

	void set_structure_rarity(int p_id, int p_rarity);
	int get_structure_rarity(int p_id) const;

	void set_structure_biome_tags(int p_id, const PackedStringArray &p_tags);
	PackedStringArray get_structure_biome_tags(int p_id) const;

	void set_structure_layer_tags(int p_id, const PackedStringArray &p_tags);
	PackedStringArray get_structure_layer_tags(int p_id) const;

	void set_structure_anchor(int p_id, const Vector3i &p_anchor);
	Vector3i get_structure_anchor(int p_id) const;

	void set_structure_allowed_rotations(int p_id, const PackedInt32Array &p_rotations);
	PackedInt32Array get_structure_allowed_rotations(int p_id) const;

	void set_structure_world_object_type(int p_id, const StringName &p_type);
	StringName get_structure_world_object_type(int p_id) const;

	void set_structure_world_object_state(int p_id, const Dictionary &p_state);
	Dictionary get_structure_world_object_state(int p_id) const;

	void set_structure_world_object_blocking(int p_id, bool p_blocking);
	bool get_structure_world_object_blocking(int p_id) const;

	const Vector<StructureEntry> &get_structures() const { return structures; }
};
