#include "rig_model_data.h"

#include "core/object/class_db.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/math/aabb.h"
#include "core/math/basis.h"
#include "core/variant/typed_array.h"

namespace {

bool read_vec3(const Variant &p_value, Vector3 &r_out) {
	if (p_value.get_type() != Variant::ARRAY) {
		return false;
	}
	const Array a = p_value;
	if (a.size() < 3) {
		return false;
	}
	r_out = Vector3((real_t)(double)a[0], (real_t)(double)a[1], (real_t)(double)a[2]);
	return true;
}

bool read_vec2(const Variant &p_value, Vector2 &r_out) {
	if (p_value.get_type() != Variant::ARRAY) {
		return false;
	}
	const Array a = p_value;
	if (a.size() < 2) {
		return false;
	}
	r_out = Vector2((real_t)(double)a[0], (real_t)(double)a[1]);
	return true;
}

float read_number(const Dictionary &p_dict, const char *p_key, float p_default) {
	if (!p_dict.has(p_key)) {
		return p_default;
	}
	const Variant v = p_dict[p_key];
	if (v.get_type() == Variant::FLOAT || v.get_type() == Variant::INT) {
		return (float)(double)v;
	}
	return p_default;
}

bool read_bool(const Dictionary &p_dict, const char *p_key, bool p_default) {
	if (!p_dict.has(p_key)) {
		return p_default;
	}
	const Variant v = p_dict[p_key];
	if (v.get_type() == Variant::BOOL) {
		return (bool)v;
	}
	if (v.get_type() == Variant::FLOAT || v.get_type() == Variant::INT) {
		return (double)v != 0.0;
	}
	return p_default;
}

const char *FACE_KEYS[RigModelData::FACE_COUNT] = { "north", "east", "south", "west", "up", "down" };

} // namespace

Vector3 RigModelData::bedrock_rotation_to_engine(const Vector3 &p_degrees) {
	return Vector3(Math::deg_to_rad(-p_degrees.x), Math::deg_to_rad(-p_degrees.y), Math::deg_to_rad(p_degrees.z));
}

Error RigModelData::_parse_geometry(const Dictionary &p_geo, const Dictionary &p_description, const String &p_fallback_id) {
	identifier = p_description.has("identifier") ? String(p_description["identifier"]) : p_fallback_id;
	if (p_description.has("texture")) {
		texture_path = String(p_description["texture"]);
	}
	const float tw = p_description.has("texture_width") ? read_number(p_description, "texture_width", 64) : read_number(p_description, "texturewidth", 64);
	const float th = p_description.has("texture_height") ? read_number(p_description, "texture_height", 64) : read_number(p_description, "textureheight", 64);
	texture_size = Vector2i(MAX(1, (int)tw), MAX(1, (int)th));
	if (p_description.has("unit_scale")) {
		unit_scale = MAX(0.00001f, read_number(p_description, "unit_scale", 1.0f / 16.0f));
	}

	ERR_FAIL_COND_V_MSG(!p_geo.has("bones") || p_geo["bones"].get_type() != Variant::ARRAY, ERR_PARSE_ERROR, "RigModelData: geometry has no 'bones' array.");
	const Array bone_array = p_geo["bones"];

	Vector<String> parent_names;
	for (int bi = 0; bi < bone_array.size(); bi++) {
		ERR_CONTINUE_MSG(bone_array[bi].get_type() != Variant::DICTIONARY, "RigModelData: bone entry is not an object.");
		const Dictionary bd = bone_array[bi];
		ERR_CONTINUE_MSG(!bd.has("name"), "RigModelData: bone without a name.");

		Bone bone;
		bone.name = StringName(String(bd["name"]));
		ERR_CONTINUE_MSG(bone_lookup.has(bone.name), vformat("RigModelData: duplicate bone '%s'.", bone.name));
		read_vec3(bd.get("pivot", Variant()), bone.pivot);
		if (!read_vec3(bd.get("rotation", Variant()), bone.rotation)) {
			read_vec3(bd.get("bind_pose_rotation", Variant()), bone.rotation);
		}
		bone.never_render = read_bool(bd, "neverRender", false);
		if (bd.has("texture")) {
			bone.texture_path = String(bd["texture"]);
		}
		Vector2 bone_tex_size;
		if (read_vec2(bd.get("texture_size", Variant()), bone_tex_size)) {
			bone.texture_size = Vector2i(MAX(0, (int)bone_tex_size.x), MAX(0, (int)bone_tex_size.y));
		}
		const bool bone_mirror = read_bool(bd, "mirror", false);
		const float bone_inflate = read_number(bd, "inflate", 0.0f);
		parent_names.push_back(bd.has("parent") ? String(bd["parent"]) : String());

		if (bd.has("cubes") && bd["cubes"].get_type() == Variant::ARRAY) {
			const Array cube_array = bd["cubes"];
			for (int ci = 0; ci < cube_array.size(); ci++) {
				ERR_CONTINUE(cube_array[ci].get_type() != Variant::DICTIONARY);
				const Dictionary cd = cube_array[ci];
				Cube cube;
				ERR_CONTINUE_MSG(!read_vec3(cd.get("origin", Variant()), cube.origin) || !read_vec3(cd.get("size", Variant()), cube.size),
						vformat("RigModelData: cube %d of bone '%s' needs 'origin' and 'size'.", ci, bone.name));
				cube.inflate = read_number(cd, "inflate", bone_inflate);
				cube.mirror = read_bool(cd, "mirror", bone_mirror);
				cube.pivot = cube.origin + cube.size * 0.5f;
				if (read_vec3(cd.get("pivot", Variant()), cube.pivot) || cd.has("rotation")) {
					cube.has_rotation = read_vec3(cd.get("rotation", Variant()), cube.rotation) && !cube.rotation.is_zero_approx();
				}
				const Variant uv = cd.get("uv", Variant());
				if (uv.get_type() == Variant::ARRAY) {
					read_vec2(uv, cube.uv);
				} else if (uv.get_type() == Variant::DICTIONARY) {
					cube.per_face_uv = true;
					const Dictionary fd = uv;
					for (int f = 0; f < FACE_COUNT; f++) {
						cube.face_enabled[f] = false;
						if (!fd.has(FACE_KEYS[f]) || fd[FACE_KEYS[f]].get_type() != Variant::DICTIONARY) {
							continue;
						}
						const Dictionary one = fd[FACE_KEYS[f]];
						Vector2 fuv;
						Vector2 fsize;
						if (!read_vec2(one.get("uv", Variant()), fuv) || !read_vec2(one.get("uv_size", Variant()), fsize)) {
							continue;
						}
						cube.face_uv[f] = Rect2(fuv, fsize);
						cube.face_rotation[f] = (uint8_t)(((int)Math::round(read_number(one, "uv_rotation", 0.0f) / 90.0f) % 4 + 4) % 4);
						cube.face_enabled[f] = true;
					}
				}
				bone.cubes.push_back(cube);
			}
		}

		if (bd.has("locators") && bd["locators"].get_type() == Variant::DICTIONARY) {
			const Dictionary ld = bd["locators"];
			const Array keys = ld.keys();
			for (int li = 0; li < keys.size(); li++) {
				Locator loc;
				loc.name = StringName(String(keys[li]));
				const Variant value = ld[keys[li]];
				if (value.get_type() == Variant::DICTIONARY) {
					const Dictionary lv = value;
					read_vec3(lv.get("offset", Variant()), loc.offset);
					read_vec3(lv.get("rotation", Variant()), loc.rotation);
				} else if (!read_vec3(value, loc.offset)) {
					continue;
				}
				bone.locators.push_back(loc);
			}
		}

		bone_lookup[bone.name] = bones.size();
		bones.push_back(bone);
	}

	// Resolve parents by name and reject cycles.
	for (int i = 0; i < bones.size(); i++) {
		const String &pn = parent_names[i];
		if (pn.is_empty()) {
			continue;
		}
		const int *pi = bone_lookup.getptr(StringName(pn));
		if (!pi) {
			WARN_PRINT(vformat("RigModelData: bone '%s' references unknown parent '%s'.", bones[i].name, pn));
			continue;
		}
		bones.write[i].parent = *pi;
	}
	for (int i = 0; i < bones.size(); i++) {
		int cursor = bones[i].parent;
		int guard = 0;
		while (cursor >= 0) {
			ERR_FAIL_COND_V_MSG(cursor == i || ++guard > bones.size(), ERR_PARSE_ERROR, vformat("RigModelData: bone hierarchy cycle at '%s'.", bones[i].name));
			cursor = bones[cursor].parent;
		}
	}
	return OK;
}

Error RigModelData::parse_json(const String &p_json_text) {
	bones.clear();
	bone_lookup.clear();
	mesh_cache.clear();
	identifier = String();
	revision++;

	Ref<JSON> json;
	json.instantiate();
	const Error err = json->parse(p_json_text);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("RigModelData: JSON error at line %d: %s", json->get_error_line(), json->get_error_message()));
	const Variant data = json->get_data();
	ERR_FAIL_COND_V_MSG(data.get_type() != Variant::DICTIONARY, ERR_PARSE_ERROR, "RigModelData: root of the geometry file must be an object.");
	const Dictionary root = data;

	if (root.has("minecraft:geometry")) {
		ERR_FAIL_COND_V_MSG(root["minecraft:geometry"].get_type() != Variant::ARRAY, ERR_PARSE_ERROR, "RigModelData: 'minecraft:geometry' must be an array.");
		const Array geos = root["minecraft:geometry"];
		ERR_FAIL_COND_V_MSG(geos.is_empty() || geos[0].get_type() != Variant::DICTIONARY, ERR_PARSE_ERROR, "RigModelData: 'minecraft:geometry' is empty.");
		const Dictionary geo = geos[0];
		Dictionary desc;
		if (geo.has("description") && geo["description"].get_type() == Variant::DICTIONARY) {
			desc = geo["description"];
		}
		return _parse_geometry(geo, desc, String());
	}

	const Array keys = root.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String key = keys[i];
		if (key.begins_with("geometry.") && root[key].get_type() == Variant::DICTIONARY) {
			const Dictionary geo = root[key];
			return _parse_geometry(geo, geo, key.get_slicec(':', 0));
		}
	}
	ERR_FAIL_V_MSG(ERR_PARSE_ERROR, "RigModelData: no 'minecraft:geometry' entry found.");
}

Error RigModelData::load_from_file(const String &p_path) {
	Error err;
	const String text = FileAccess::get_file_as_string(p_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("RigModelData: cannot read '%s'.", p_path));
	return parse_json(text);
}

void RigModelData::set_unit_scale(float p_scale) {
	unit_scale = MAX(0.00001f, p_scale);
	mesh_cache.clear();
	revision++;
}

Ref<ArrayMesh> RigModelData::get_bone_mesh(int p_bone) const {
	ERR_FAIL_INDEX_V(p_bone, bones.size(), Ref<ArrayMesh>());
	if (mesh_cache.size() != bones.size()) {
		mesh_cache.resize(bones.size());
	}
	if (mesh_cache[p_bone].is_null()) {
		Ref<ArrayMesh> mesh;
		_build_mesh(p_bone, mesh);
		mesh_cache.write[p_bone] = mesh;
	}
	return mesh_cache[p_bone];
}

void RigModelData::_build_mesh(int p_bone, Ref<ArrayMesh> &r_mesh) const {
	const Bone &bone = bones[p_bone];
	if (bone.never_render || bone.cubes.is_empty()) {
		return;
	}

	Vector<Vector3> verts;
	Vector<Vector3> normals;
	Vector<Vector2> uvs;
	Vector<int> indices;
	const Vector2i tex_size = (bone.texture_size.x > 0 && bone.texture_size.y > 0) ? bone.texture_size : texture_size;
	const float inv_w = 1.0f / (float)tex_size.x;
	const float inv_h = 1.0f / (float)tex_size.y;

	for (int ci = 0; ci < bone.cubes.size(); ci++) {
		const Cube &cube = bone.cubes[ci];

		const Vector3 mn = cube.origin - Vector3(cube.inflate, cube.inflate, cube.inflate);
		const Vector3 mx = cube.origin + cube.size + Vector3(cube.inflate, cube.inflate, cube.inflate);
		const Vector3 ext = mx - mn;

		Basis cube_basis;
		if (cube.has_rotation) {
			cube_basis = Basis::from_euler(bedrock_rotation_to_engine(cube.rotation), EulerOrder::XYZ);
		}

		// Face table: top-left corner, image-right axis extent, image-down axis extent, outward normal.
		struct FaceDef {
			Vector3 tl;
			Vector3 right;
			Vector3 down;
			Vector3 normal;
		};
		FaceDef defs[FACE_COUNT];
		defs[FACE_NORTH] = { Vector3(mx.x, mx.y, mn.z), Vector3(-ext.x, 0, 0), Vector3(0, -ext.y, 0), Vector3(0, 0, -1) };
		defs[FACE_SOUTH] = { Vector3(mn.x, mx.y, mx.z), Vector3(ext.x, 0, 0), Vector3(0, -ext.y, 0), Vector3(0, 0, 1) };
		defs[FACE_EAST] = { Vector3(mx.x, mx.y, mx.z), Vector3(0, 0, -ext.z), Vector3(0, -ext.y, 0), Vector3(1, 0, 0) };
		defs[FACE_WEST] = { Vector3(mn.x, mx.y, mn.z), Vector3(0, 0, ext.z), Vector3(0, -ext.y, 0), Vector3(-1, 0, 0) };
		defs[FACE_UP] = { Vector3(mn.x, mx.y, mn.z), Vector3(ext.x, 0, 0), Vector3(0, 0, ext.z), Vector3(0, 1, 0) };
		defs[FACE_DOWN] = { Vector3(mn.x, mn.y, mx.z), Vector3(ext.x, 0, 0), Vector3(0, 0, -ext.z), Vector3(0, -1, 0) };

		Rect2 rects[FACE_COUNT];
		bool enabled[FACE_COUNT];
		if (cube.per_face_uv) {
			for (int f = 0; f < FACE_COUNT; f++) {
				rects[f] = cube.face_uv[f];
				enabled[f] = cube.face_enabled[f];
			}
		} else {
			const float u = cube.uv.x;
			const float v = cube.uv.y;
			const float w = cube.size.x;
			const float h = cube.size.y;
			const float d = cube.size.z;
			rects[FACE_NORTH] = Rect2(u + d, v + d, w, h);
			rects[FACE_EAST] = Rect2(u, v + d, d, h);
			rects[FACE_SOUTH] = Rect2(u + 2 * d + w, v + d, w, h);
			rects[FACE_WEST] = Rect2(u + d + w, v + d, d, h);
			rects[FACE_UP] = Rect2(u + d, v, w, d);
			rects[FACE_DOWN] = Rect2(u + d + w, v, w, d);
			for (int f = 0; f < FACE_COUNT; f++) {
				enabled[f] = true;
			}
			if (cube.mirror) {
				SWAP(rects[FACE_EAST], rects[FACE_WEST]);
			}
		}
		const bool flip_u = cube.mirror && !cube.per_face_uv;

		for (int f = 0; f < FACE_COUNT; f++) {
			if (!enabled[f]) {
				continue;
			}
			const FaceDef &fd = defs[f];
			Vector3 corners[4] = { fd.tl, fd.tl + fd.right, fd.tl + fd.right + fd.down, fd.tl + fd.down };
			Vector3 normal = fd.normal;
			for (int k = 0; k < 4; k++) {
				Vector3 p = corners[k];
				if (cube.has_rotation) {
					p = cube.pivot + cube_basis.xform(p - cube.pivot);
				}
				corners[k] = (p - bone.pivot) * unit_scale;
			}
			if (cube.has_rotation) {
				normal = cube_basis.xform(normal);
			}

			float u0 = rects[f].position.x * inv_w;
			float u1 = (rects[f].position.x + rects[f].size.x) * inv_w;
			const float v0 = rects[f].position.y * inv_h;
			const float v1 = (rects[f].position.y + rects[f].size.y) * inv_h;
			if (flip_u) {
				SWAP(u0, u1);
			}
			const Vector2 base_uvs[4] = { Vector2(u0, v0), Vector2(u1, v0), Vector2(u1, v1), Vector2(u0, v1) };
			const int rot = cube.per_face_uv ? cube.face_rotation[f] : 0;
			Vector2 face_uvs[4];
			for (int k = 0; k < 4; k++) {
				face_uvs[k] = base_uvs[(k + rot) & 3];
			}

			const int base = verts.size();
			for (int k = 0; k < 4; k++) {
				verts.push_back(corners[k]);
				normals.push_back(normal);
				uvs.push_back(face_uvs[k]);
			}
			indices.push_back(base);
			indices.push_back(base + 1);
			indices.push_back(base + 2);
			indices.push_back(base);
			indices.push_back(base + 2);
			indices.push_back(base + 3);
		}
	}

	if (verts.is_empty()) {
		return;
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = verts;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;
	r_mesh.instantiate();
	r_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	r_mesh->set_name(bone.name);
}

PackedStringArray RigModelData::get_bone_names() const {
	PackedStringArray out;
	for (int i = 0; i < bones.size(); i++) {
		out.push_back(String(bones[i].name));
	}
	return out;
}

PackedStringArray RigModelData::get_locator_names() const {
	PackedStringArray out;
	for (int i = 0; i < bones.size(); i++) {
		for (int l = 0; l < bones[i].locators.size(); l++) {
			out.push_back(String(bones[i].locators[l].name));
		}
	}
	return out;
}

Vector3 RigModelData::get_bone_pivot(const StringName &p_bone) const {
	const int i = find_bone(p_bone);
	ERR_FAIL_COND_V_MSG(i < 0, Vector3(), vformat("RigModelData: unknown bone '%s'.", p_bone));
	return bones[i].pivot * unit_scale;
}

AABB RigModelData::get_bounds() const {
	bool first = true;
	AABB box;
	for (int i = 0; i < bones.size(); i++) {
		for (int c = 0; c < bones[i].cubes.size(); c++) {
			const Cube &cube = bones[i].cubes[c];
			const Vector3 inf(cube.inflate, cube.inflate, cube.inflate);
			const AABB cb((cube.origin - inf) * unit_scale, (cube.size + inf * 2.0f) * unit_scale);
			if (first) {
				box = cb;
				first = false;
			} else {
				box.merge_with(cb);
			}
		}
	}
	return box;
}

void RigModelData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse_json", "json_text"), &RigModelData::parse_json);
	ClassDB::bind_method(D_METHOD("load_from_file", "path"), &RigModelData::load_from_file);
	ClassDB::bind_method(D_METHOD("get_bone_count"), &RigModelData::get_bone_count);
	ClassDB::bind_method(D_METHOD("get_bone_names"), &RigModelData::get_bone_names);
	ClassDB::bind_method(D_METHOD("has_bone", "name"), &RigModelData::has_bone);
	ClassDB::bind_method(D_METHOD("get_locator_names"), &RigModelData::get_locator_names);
	ClassDB::bind_method(D_METHOD("get_identifier"), &RigModelData::get_identifier);
	ClassDB::bind_method(D_METHOD("get_texture_size"), &RigModelData::get_texture_size);
	ClassDB::bind_method(D_METHOD("get_bone_pivot", "bone"), &RigModelData::get_bone_pivot);
	ClassDB::bind_method(D_METHOD("get_bounds"), &RigModelData::get_bounds);
	ClassDB::bind_method(D_METHOD("set_texture_path", "path"), &RigModelData::set_texture_path);
	ClassDB::bind_method(D_METHOD("get_texture_path"), &RigModelData::get_texture_path);
	ClassDB::bind_method(D_METHOD("set_unit_scale", "scale"), &RigModelData::set_unit_scale);
	ClassDB::bind_method(D_METHOD("get_unit_scale"), &RigModelData::get_unit_scale);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture_path", PROPERTY_HINT_FILE, "*.png,*.jpg,*.jpeg,*.webp"), "set_texture_path", "get_texture_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "unit_scale", PROPERTY_HINT_RANGE, "0.0001,10,0.0001,or_greater"), "set_unit_scale", "get_unit_scale");
}
