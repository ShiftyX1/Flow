#pragma once

#include "core/io/resource.h"
#include "core/math/rect2.h"
#include "core/math/vector3.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "scene/resources/mesh.h"

// Geometry of a cube-based rig, parsed from a Minecraft-Bedrock-style `*.geo.json` file
// (as exported by Blockbench). Units inside the file are "pixels"; unit_scale converts them
// to engine units (default 1/16, i.e. 16 pixels per metre).
//
// Supported subset:
//   description: identifier, texture_width, texture_height, plus the non-standard "texture"
//                (res:// path of the default texture) and "unit_scale".
//   bones:       name, parent, pivot, rotation, mirror, inflate, neverRender, locators, cubes,
//                plus the non-standard "texture" / "texture_size" ([w, h]) to give one bone its
//                own texture (UVs of that bone are then normalised by its own texture size).
//   cubes:       origin, size, uv ([u, v] box UV or per-face object), inflate, mirror,
//                pivot, rotation. Per-face entries are { "uv": [u, v], "uv_size": [w, h],
//                "uv_rotation": 0|90|180|270 } (negative sizes flip the face).
//
// The legacy 1.8 layout ("geometry.name": { "bones": [...], "texturewidth": N }) is accepted too.
//
// Rotation convention: the file stores Bedrock angles in degrees. They are converted with
// bedrock_rotation_to_engine() (X and Y negated) and composed in XYZ order.
class RigModelData : public Resource {
	GDCLASS(RigModelData, Resource);

public:
	enum Face {
		FACE_NORTH, // -Z
		FACE_EAST, // +X
		FACE_SOUTH, // +Z
		FACE_WEST, // -X
		FACE_UP, // +Y
		FACE_DOWN, // -Y
		FACE_COUNT,
	};

	struct Cube {
		Vector3 origin;
		Vector3 size;
		Vector3 pivot;
		Vector3 rotation; // degrees, Bedrock convention.
		float inflate = 0.0f;
		bool mirror = false;
		bool has_rotation = false;
		bool per_face_uv = false;
		Vector2 uv; // box UV origin in pixels.
		Rect2 face_uv[FACE_COUNT]; // pixels; only valid when per_face_uv.
		uint8_t face_rotation[FACE_COUNT] = { 0, 0, 0, 0, 0, 0 }; // 90-degree steps of per-face UV rotation.
		bool face_enabled[FACE_COUNT] = { true, true, true, true, true, true };
	};

	struct Locator {
		StringName name;
		Vector3 offset; // pixels, relative to world origin of the model (same space as pivots).
		Vector3 rotation; // degrees, Bedrock convention.
	};

	struct Bone {
		StringName name;
		int parent = -1;
		Vector3 pivot;
		Vector3 rotation; // degrees, Bedrock convention.
		bool never_render = false;
		String texture_path; // Optional per-bone texture (non-standard "texture" key); empty = model default.
		Vector2i texture_size; // Optional per-bone texture size in pixels; (0, 0) = model default.
		Vector<Cube> cubes;
		Vector<Locator> locators;
	};

	// Convert Bedrock-convention Euler degrees to engine radians (XYZ order).
	static Vector3 bedrock_rotation_to_engine(const Vector3 &p_degrees);

private:
	Vector<Bone> bones;
	HashMap<StringName, int> bone_lookup;
	String identifier;
	String texture_path;
	Vector2i texture_size = Vector2i(64, 64);
	float unit_scale = 1.0f / 16.0f;
	uint32_t revision = 0;

	mutable Vector<Ref<ArrayMesh>> mesh_cache;

	Error _parse_geometry(const Dictionary &p_geo, const Dictionary &p_description, const String &p_fallback_id);
	void _build_mesh(int p_bone, Ref<ArrayMesh> &r_mesh) const;

protected:
	static void _bind_methods();

public:
	Error parse_json(const String &p_json_text);
	Error load_from_file(const String &p_path);

	int get_bone_count() const { return bones.size(); }
	const Bone &get_bone(int p_index) const { return bones[p_index]; }
	int find_bone(const StringName &p_name) const {
		const int *i = bone_lookup.getptr(p_name);
		return i ? *i : -1;
	}

	// Shared per-bone mesh (null when the bone has no cubes or is marked neverRender).
	Ref<ArrayMesh> get_bone_mesh(int p_bone) const;

	uint32_t get_revision() const { return revision; }

	// Script API.
	PackedStringArray get_bone_names() const;
	bool has_bone(const StringName &p_name) const { return find_bone(p_name) >= 0; }
	PackedStringArray get_locator_names() const;
	String get_identifier() const { return identifier; }
	void set_texture_path(const String &p_path) { texture_path = p_path; }
	String get_texture_path() const { return texture_path; }
	Vector2i get_texture_size() const { return texture_size; }
	void set_unit_scale(float p_scale);
	float get_unit_scale() const { return unit_scale; }
	Vector3 get_bone_pivot(const StringName &p_bone) const;
	AABB get_bounds() const;
};
