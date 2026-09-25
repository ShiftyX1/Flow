#pragma once

#include "core/io/resource.h"
#include "core/variant/typed_array.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"

class MobaMapPalette : public Resource {
	GDCLASS(MobaMapPalette, Resource);
	TypedArray<Material> materials;
	TypedArray<PackedScene> objects;

protected:
	static void _bind_methods();

public:
	void set_materials(const TypedArray<Material> &p_value);
	TypedArray<Material> get_materials() const { return materials; }
	void set_objects(const TypedArray<PackedScene> &p_value);
	TypedArray<PackedScene> get_objects() const { return objects; }
	Ref<Material> get_material(int p_index) const;
	Ref<PackedScene> get_object(int p_index) const;
	void create_defaults();
};
