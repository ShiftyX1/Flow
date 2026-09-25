#include "moba_map_palette.h"

#include "core/object/class_db.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/cylinder_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"

static Ref<StandardMaterial3D> moba_material(const String &p_name, Color p_color) {
	Ref<StandardMaterial3D> result;
	result.instantiate();
	result->set_name(p_name);
	result->set_albedo(p_color);
	result->set_roughness(0.9);
	return result;
}

void MobaMapPalette::set_materials(const TypedArray<Material> &p_value) {
	ERR_FAIL_COND(p_value.size() > 256);
	materials = p_value;
	emit_changed();
}

void MobaMapPalette::set_objects(const TypedArray<PackedScene> &p_value) {
	objects = p_value;
	emit_changed();
}

Ref<Material> MobaMapPalette::get_material(int p_index) const {
	if (p_index >= 0 && p_index < materials.size()) {
		return materials[p_index];
	}
	return materials.is_empty() ? Ref<Material>() : Ref<Material>(materials[0]);
}

Ref<PackedScene> MobaMapPalette::get_object(int p_index) const {
	return p_index >= 0 && p_index < objects.size() ? Ref<PackedScene>(objects[p_index]) : Ref<PackedScene>();
}

void MobaMapPalette::create_defaults() {
	materials.clear();
	objects.clear();
	materials.push_back(moba_material("Grass", Color(0.22, 0.39, 0.19)));
	materials.push_back(moba_material("Path", Color(0.49, 0.39, 0.25)));
	materials.push_back(moba_material("Stone", Color(0.38, 0.41, 0.44)));

	for (int kind = 0; kind < 2; ++kind) {
		StaticBody3D *root = memnew(StaticBody3D);
		root->set_name(kind == 0 ? "Tree" : "Rock");
		CollisionShape3D *collision = memnew(CollisionShape3D);
		root->add_child(collision);
		collision->set_owner(root);
		MeshInstance3D *base = memnew(MeshInstance3D);
		root->add_child(base);
		base->set_owner(root);
		if (kind == 0) {
			Ref<CylinderMesh> trunk;
			trunk.instantiate();
			trunk->set_top_radius(0.3);
			trunk->set_bottom_radius(0.45);
			trunk->set_height(3.0);
			base->set_mesh(trunk);
			base->set_position(Vector3(0, 1.5, 0));
			base->set_material_override(moba_material("Bark", Color(0.3, 0.19, 0.1)));
			MeshInstance3D *crown = memnew(MeshInstance3D);
			Ref<CylinderMesh> cone;
			cone.instantiate();
			cone->set_top_radius(0.0);
			cone->set_bottom_radius(1.5);
			cone->set_height(3.5);
			crown->set_mesh(cone);
			crown->set_position(Vector3(0, 3.0, 0));
			crown->set_material_override(moba_material("Leaves", Color(0.10, 0.28, 0.16)));
			root->add_child(crown);
			crown->set_owner(root);
			Ref<CylinderShape3D> shape;
			shape.instantiate();
			shape->set_radius(0.65);
			shape->set_height(4.5);
			collision->set_shape(shape);
			collision->set_position(Vector3(0, 2.25, 0));
		} else {
			Ref<BoxMesh> mesh;
			mesh.instantiate();
			mesh->set_size(Vector3(2, 1.7, 1.8));
			base->set_mesh(mesh);
			base->set_position(Vector3(0, 0.85, 0));
			base->set_material_override(materials[2]);
			Ref<BoxShape3D> shape;
			shape.instantiate();
			shape->set_size(mesh->get_size());
			collision->set_shape(shape);
			collision->set_position(base->get_position());
		}
		Ref<PackedScene> scene;
		scene.instantiate();
		scene->pack(root);
		scene->set_name(kind == 0 ? "Tree" : "Rock");
		objects.push_back(scene);
		memdelete(root);
	}
	emit_changed();
}

void MobaMapPalette::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_materials", "materials"), &MobaMapPalette::set_materials);
	ClassDB::bind_method(D_METHOD("get_materials"), &MobaMapPalette::get_materials);
	ClassDB::bind_method(D_METHOD("set_objects", "objects"), &MobaMapPalette::set_objects);
	ClassDB::bind_method(D_METHOD("get_objects"), &MobaMapPalette::get_objects);
	ClassDB::bind_method(D_METHOD("get_material", "index"), &MobaMapPalette::get_material);
	ClassDB::bind_method(D_METHOD("get_object", "index"), &MobaMapPalette::get_object);
	ClassDB::bind_method(D_METHOD("create_defaults"), &MobaMapPalette::create_defaults);
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "materials", PROPERTY_HINT_ARRAY_TYPE, "Material"), "set_materials", "get_materials");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "objects", PROPERTY_HINT_ARRAY_TYPE, "PackedScene"), "set_objects", "get_objects");
}
