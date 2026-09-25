#include "moba_map_3d.h"

#include "core/config/engine.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/os/thread.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/navigation_mesh_source_geometry_data_3d.h"
#include "scene/resources/surface_tool.h"
#include "servers/navigation_3d/navigation_server_3d.h"

MobaMap3D::MobaMap3D() {
	set_notify_transform(true);
	set_process_internal(true);
}

void MobaMap3D::set_map_data(const Ref<MobaMapData> &p_data) {
	if (data == p_data) {
		return;
	}
	if (data.is_valid()) {
		data->disconnect("region_changed", callable_mp(this, &MobaMap3D::_region_changed));
	}
	data = p_data;
	if (data.is_valid()) {
		data->connect("region_changed", callable_mp(this, &MobaMap3D::_region_changed));
		_region_changed(Rect2i(Vector2i(), data->get_size()));
	} else {
		_clear_generated();
		invalidate_navigation();
	}
	update_configuration_warnings();
}

void MobaMap3D::set_palette(const Ref<MobaMapPalette> &p_palette) {
	if (palette == p_palette) {
		return;
	}
	if (palette.is_valid()) {
		palette->disconnect("changed", callable_mp(this, &MobaMap3D::_palette_changed));
	}
	palette = p_palette;
	if (palette.is_valid()) {
		palette->connect("changed", callable_mp(this, &MobaMap3D::_palette_changed));
	}
	_palette_changed();
}

void MobaMap3D::_palette_changed() {
	if (data.is_valid()) {
		_region_changed(Rect2i(Vector2i(), data->get_size()));
	}
}

void MobaMap3D::_clear_generated() {
	chunks.clear();
	dirty_chunks.clear();
	if (generated) {
		remove_child(generated);
		memdelete(generated);
		generated = nullptr;
		navigation = nullptr;
	}
	built_size = Vector2i();
}

void MobaMap3D::_region_changed(Rect2i p_region) {
	invalidate_navigation();
	if (data.is_null()) {
		return;
	}
	// Neighbour chunks own the other face of a modified boundary.
	Rect2i area = p_region.grow(1).intersection(Rect2i(Vector2i(), data->get_size()));
	for (int z = area.position.y / CHUNK_SIZE; z <= (area.get_end().y - 1) / CHUNK_SIZE; ++z) {
		for (int x = area.position.x / CHUNK_SIZE; x <= (area.get_end().x - 1) / CHUNK_SIZE; ++x) {
			dirty_chunks.insert(Vector2i(x, z));
		}
	}
	if (is_inside_tree() && !rebuild_queued) {
		rebuild_queued = true;
		callable_mp(this, &MobaMap3D::rebuild_dirty).call_deferred();
	}
}

static void moba_triangle(const Ref<SurfaceTool> &p_surface, Vector<Vector3> &r_faces, const Vector3 &a, const Vector3 &b, const Vector3 &c, real_t p_uv_scale) {
	Vector3 normal = (c - a).cross(b - a);
	if (normal.length_squared() < 0.0000001) {
		return;
	}
	normal.normalize();
	Vector3 points[3] = { a, b, c };
	for (const Vector3 &v : points) {
		p_surface->set_normal(normal);
		if (Math::abs(normal.y) > 0.5) {
			p_surface->set_uv(Vector2(v.x, v.z) / p_uv_scale);
		} else {
			p_surface->set_uv(Vector2(Math::abs(normal.x) > 0.5 ? v.z : v.x, v.y) / p_uv_scale);
		}
		p_surface->add_vertex(v);
		r_faces.push_back(v);
	}
}

void MobaMap3D::_rebuild_chunk(Vector2i p_chunk) {
	if (!chunks.has(p_chunk)) {
		Chunk chunk;
		chunk.mesh = memnew(MeshInstance3D);
		generated->add_child(chunk.mesh);
		StaticBody3D *body = memnew(StaticBody3D);
		generated->add_child(body);
		chunk.collision = memnew(CollisionShape3D);
		body->add_child(chunk.collision);
		chunks.insert(p_chunk, chunk);
	}
	Chunk &chunk = chunks[p_chunk];
	HashMap<int, Ref<SurfaceTool>> surfaces;
	Vector<Vector3> faces;
	Vector2i start = p_chunk * CHUNK_SIZE;
	Vector2i end(MIN(start.x + CHUNK_SIZE, data->get_size().x), MIN(start.y + CHUNK_SIZE, data->get_size().y));
	for (int z = start.y; z < end.y; ++z) {
		for (int x = start.x; x < end.x; ++x) {
			Vector2i cell(x, z);
			int material = data->get_cell(cell).y;
			if (!surfaces.has(material)) {
				Ref<SurfaceTool> surface;
				surface.instantiate();
				surface->begin(Mesh::PRIMITIVE_TRIANGLES);
				if (palette.is_valid()) {
					surface->set_material(palette->get_material(material));
				}
				surfaces.insert(material, surface);
			}
			Ref<SurfaceTool> surface = surfaces[material];
			Vector3 v[4];
			data->get_corners(cell, v);
			moba_triangle(surface, faces, v[0], v[1], v[2], data->get_cell_size());
			moba_triangle(surface, faces, v[0], v[2], v[3], data->get_cell_size());
			for (int e = 0; e < 4; ++e) {
				Vector3 a = v[e];
				Vector3 b = v[(e + 1) % 4];
				Vector3 low_a = a;
				Vector3 low_b = b;
				Vector2i neighbour = cell + MobaMapData::ramp_direction(e + 1);
				if (data->contains(neighbour)) {
					Vector3 n[4];
					data->get_corners(neighbour, n);
					low_a.y = n[(e + 3) % 4].y;
					low_b.y = n[(e + 2) % 4].y;
				} else {
					low_a.y = low_b.y = MIN((real_t)0, MIN(a.y, b.y)) - data->get_height_step();
				}
				real_t da = a.y - low_a.y;
				real_t db = b.y - low_b.y;
				if (da <= 0 && db <= 0) {
					continue;
				}
				// Opposing ramps can meet halfway along an edge. Split the wall there.
				if (da < 0 || db < 0) {
					real_t t = da / (da - db);
					Vector3 intersection = a.lerp(b, t);
					if (da < 0) { a = low_a = intersection; }
					else { b = low_b = intersection; }
				}
				moba_triangle(surface, faces, a, low_a, low_b, data->get_cell_size());
				moba_triangle(surface, faces, a, low_b, b, data->get_cell_size());
			}
		}
	}
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	for (const KeyValue<int, Ref<SurfaceTool>> &entry : surfaces) {
		entry.value->commit(mesh);
	}
	chunk.mesh->set_mesh(mesh);
	chunk.triangles = mesh->generate_triangle_mesh();
	Ref<ConcavePolygonShape3D> shape;
	shape.instantiate();
	shape->set_faces(faces);
	chunk.collision->set_shape(shape);
}

void MobaMap3D::rebuild_dirty() {
	rebuild_queued = false;
	if (!is_inside_tree() || data.is_null()) {
		return;
	}
	if (built_size != data->get_size() || !generated) {
		_clear_generated();
		generated = memnew(Node3D);
		generated->set_name("_MobaGenerated");
		add_child(generated, false, INTERNAL_MODE_BACK);
		navigation = memnew(NavigationRegion3D);
		generated->add_child(navigation);
		built_size = data->get_size();
		for (int z = 0; z < built_size.y; z += CHUNK_SIZE) {
			for (int x = 0; x < built_size.x; x += CHUNK_SIZE) {
				dirty_chunks.insert(Vector2i(x / CHUNK_SIZE, z / CHUNK_SIZE));
			}
		}
	}
	for (const Vector2i &key : dirty_chunks) {
		_rebuild_chunk(key);
	}
	dirty_chunks.clear();
	_resnap_objects();
	object_hash = _objects_hash();
	update_gizmos();
}

void MobaMap3D::_resnap_objects() {
	for (int i = 0; i < get_child_count(); ++i) {
		Node3D *object = Object::cast_to<Node3D>(get_child(i));
		if (!object || !bool(object->get_meta("moba_grounded", false))) {
			continue;
		}
		Vector3 position = object->get_position();
		Vector2i cell((int)Math::floor(position.x / data->get_cell_size()), (int)Math::floor(position.z / data->get_cell_size()));
		if (data->contains(cell)) {
			position.y = get_surface_height(Vector2(position.x, position.z)) + real_t(object->get_meta("moba_height_offset", 0.0));
			object->set_position(position);
		}
	}
}

static uint32_t moba_hash_node(const Node *p_node, uint32_t p_hash) {
	const Node3D *spatial = Object::cast_to<Node3D>(p_node);
	if (spatial) {
		p_hash = hash_murmur3_one_32(Variant(spatial->get_transform()).hash(), p_hash);
	}
	const CollisionShape3D *collision = Object::cast_to<CollisionShape3D>(p_node);
	if (collision) {
		p_hash = hash_murmur3_one_32(collision->is_disabled(), p_hash);
		Ref<Shape3D> shape = collision->get_shape();
		if (shape.is_valid()) {
			p_hash = hash_murmur3_one_32(Variant(shape->get_debug_mesh()->get_aabb()).hash(), p_hash);
		}
	}
	for (int i = 0; i < p_node->get_child_count(); ++i) {
		p_hash = moba_hash_node(p_node->get_child(i), p_hash);
	}
	return p_hash;
}

uint32_t MobaMap3D::_objects_hash() const {
	uint32_t hash = 5381;
	for (int i = 0; i < get_child_count(); ++i) {
		hash = moba_hash_node(get_child(i), hash);
	}
	return hash;
}

void MobaMap3D::invalidate_navigation() {
	++revision;
	navigation_dirty = true;
	if (navigation) {
		navigation->set_enabled(false);
	}
	update_gizmos();
}

void MobaMap3D::bake_navigation() {
	if (!is_inside_tree() || data.is_null()) {
		emit_signal(SNAME("navigation_baked"), false);
		return;
	}
	if (baking) {
		rebake_requested = true;
		return;
	}
	rebuild_dirty();
	navigation_dirty = true;
	navigation->set_enabled(false);
	Ref<NavigationMesh> mesh;
	mesh.instantiate();
	mesh->set_parsed_geometry_type(NavigationMesh::PARSED_GEOMETRY_STATIC_COLLIDERS);
	mesh->set_cell_size(0.25);
	mesh->set_cell_height(0.1);
	mesh->set_agent_radius(0.4);
	mesh->set_agent_height(1.7);
	mesh->set_agent_max_climb(0.1);
	mesh->set_agent_max_slope(50.0);
	mesh->set_region_min_size(0.0);
	Ref<NavigationMeshSourceGeometryData3D> source;
	source.instantiate();
	NavigationServer3D *server = NavigationServer3D::get_singleton();
	server->parse_source_geometry_data(mesh, source, this);
	// Internal geometry is intentionally absent from scene serialization and parser traversal.
	for (const KeyValue<Vector2i, Chunk> &entry : chunks) {
		source->add_mesh(entry.value.mesh->get_mesh(), get_global_transform());
	}
	baking = true;
	rebake_requested = false;
	server->bake_from_source_geometry_data_async(mesh, source, callable_mp(this, &MobaMap3D::_navigation_baked).bind(mesh, revision, _objects_hash()));
}

void MobaMap3D::_navigation_baked(Ref<NavigationMesh> p_mesh, uint64_t p_revision, uint32_t p_objects) {
	if (!Thread::is_main_thread()) {
		callable_mp(this, &MobaMap3D::_navigation_baked).call_deferred(p_mesh, p_revision, p_objects);
		return;
	}
	baking = false;
	bool current = is_inside_tree() && navigation && p_revision == revision && p_objects == _objects_hash();
	if (current && p_mesh->get_polygon_count() > 0) {
		navigation->set_navigation_mesh(p_mesh);
		navigation->set_enabled(true);
		navigation_dirty = false;
	}
	update_gizmos();
	if (is_inside_tree() && (rebake_requested || !current)) {
		callable_mp(this, &MobaMap3D::bake_navigation).call_deferred();
		return;
	}
	emit_signal(SNAME("navigation_baked"), is_navigation_ready());
}

Ref<NavigationMesh> MobaMap3D::get_navigation_mesh() const {
	return navigation ? navigation->get_navigation_mesh() : Ref<NavigationMesh>();
}

real_t MobaMap3D::get_surface_height(Vector2 p_local_xz) const {
	return data.is_valid() ? data->get_surface_height(p_local_xz) : 0.0;
}

int MobaMap3D::get_plateau_level(Vector2 p_local_xz) const {
	return data.is_valid() ? data->get_plateau_level(p_local_xz) : 0;
}

TypedArray<MobaMapMarker3D> MobaMap3D::get_markers() const {
	TypedArray<MobaMapMarker3D> result;
	for (int i = 0; i < get_child_count(); ++i) {
		MobaMapMarker3D *marker = Object::cast_to<MobaMapMarker3D>(get_child(i));
		if (marker) {
			result.push_back(marker);
		}
	}
	return result;
}

bool MobaMap3D::raycast(const Vector3 &p_origin, const Vector3 &p_direction, Vector3 &r_hit) const {
	Transform3D inverse = get_global_transform().affine_inverse();
	Vector3 from = inverse.xform(p_origin);
	Vector3 dir = inverse.basis.xform(p_direction).normalized();
	real_t distance = Math::INF;
	bool found = false;
	for (const KeyValue<Vector2i, Chunk> &entry : chunks) {
		Vector3 point, normal;
		if (entry.value.triangles.is_valid() && entry.value.triangles->intersect_ray(from, dir, point, normal)) {
			real_t d = point.distance_squared_to(from);
			if (d < distance) {
				distance = d;
				r_hit = get_global_transform().xform(point);
				found = true;
			}
		}
	}
	return found;
}

Dictionary MobaMap3D::raycast_surface(Vector3 p_world_origin, Vector3 p_world_direction) const {
	Dictionary result;
	Vector3 point;
	if (raycast(p_world_origin, p_world_direction, point)) {
		result["position"] = point;
		result["local_position"] = to_local(point);
	}
	return result;
}

void MobaMap3D::attach_object(Node3D *p_object, Node *p_owner) {
	ERR_FAIL_NULL(p_object);
	ERR_FAIL_COND(p_object->get_parent());
	add_child(p_object, true);
	if (p_owner && p_owner->is_ancestor_of(p_object)) {
		adopt_object_owner(p_object, p_owner);
	}
	if (data.is_valid()) {
		_resnap_objects();
	}
	invalidate_navigation();
}

void MobaMap3D::adopt_object_owner(Node *p_object, Node *p_owner) {
	p_object->set_owner(p_owner);
	String scene_path = p_object->get_scene_file_path();
	if (!scene_path.is_empty() && !scene_path.contains("::")) {
		// File-backed prefabs keep their own child ownership and instance overrides.
		for (int i = 0; i < p_object->get_child_count(); ++i) {
			Node *child = p_object->get_child(i);
			if (!child->get_owner()) {
				adopt_object_owner(child, p_owner);
			}
		}
		return;
	}
	// An unsaved/embedded PackedScene cannot be reloaded by filename. Keep its
	// ordinary nodes owned by the edited scene so they survive save and redo.
	p_object->set_scene_file_path(String());
	p_object->set_scene_instance_state(Ref<SceneState>());
	for (int i = 0; i < p_object->get_child_count(); ++i) {
		adopt_object_owner(p_object->get_child(i), p_owner);
	}
}

void MobaMap3D::detach_object(Node3D *p_object) {
	ERR_FAIL_NULL(p_object);
	ERR_FAIL_COND(p_object->get_parent() != this);
	remove_child(p_object);
	invalidate_navigation();
}

PackedStringArray MobaMap3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	if (data.is_null()) {
		warnings.push_back("Use Initialize map in the MOBA toolbar or assign MobaMapData.");
	}
	if (!get_transform().basis.is_equal_approx(Basis())) {
		warnings.push_back("Keep the map rotation and scale at their defaults. Navigation and the preview hero use Y-up world units.");
	}
	if (data.is_valid() && Math::atan(data->get_height_step() / data->get_cell_size()) > Math::deg_to_rad(50.0)) {
		warnings.push_back("Ramps are steeper than the preview agent's 50 degree limit. Increase cell size or reduce height step.");
	}
	return warnings;
}

void MobaMap3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE && data.is_valid()) {
		_region_changed(Rect2i(Vector2i(), data->get_size()));
		if (!Engine::get_singleton()->is_editor_hint()) {
			callable_mp(this, &MobaMap3D::bake_navigation).call_deferred();
		}
	} else if (p_what == NOTIFICATION_TRANSFORM_CHANGED) {
		invalidate_navigation();
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		invalidate_navigation();
	} else if (p_what == NOTIFICATION_INTERNAL_PROCESS && data.is_valid()) {
		object_poll += get_process_delta_time();
		if (object_poll >= 0.25) {
			object_poll = 0.0;
			uint32_t current_hash = _objects_hash();
			if (current_hash != object_hash) {
				_resnap_objects();
				object_hash = _objects_hash();
				invalidate_navigation();
			}
		}
	}
}

void MobaMap3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_map_data", "data"), &MobaMap3D::set_map_data);
	ClassDB::bind_method(D_METHOD("get_map_data"), &MobaMap3D::get_map_data);
	ClassDB::bind_method(D_METHOD("set_palette", "palette"), &MobaMap3D::set_palette);
	ClassDB::bind_method(D_METHOD("get_palette"), &MobaMap3D::get_palette);
	ClassDB::bind_method(D_METHOD("get_surface_height", "local_xz"), &MobaMap3D::get_surface_height);
	ClassDB::bind_method(D_METHOD("get_plateau_level", "local_xz"), &MobaMap3D::get_plateau_level);
	ClassDB::bind_method(D_METHOD("get_markers"), &MobaMap3D::get_markers);
	ClassDB::bind_method(D_METHOD("raycast_surface", "world_origin", "world_direction"), &MobaMap3D::raycast_surface);
	ClassDB::bind_method(D_METHOD("bake_navigation"), &MobaMap3D::bake_navigation);
	ClassDB::bind_method(D_METHOD("is_navigation_ready"), &MobaMap3D::is_navigation_ready);
	ClassDB::bind_method(D_METHOD("get_navigation_mesh"), &MobaMap3D::get_navigation_mesh);
	ClassDB::bind_method(D_METHOD("attach_object", "object", "owner"), &MobaMap3D::attach_object);
	ClassDB::bind_method(D_METHOD("detach_object", "object"), &MobaMap3D::detach_object);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "map_data", PROPERTY_HINT_RESOURCE_TYPE, "MobaMapData"), "set_map_data", "get_map_data");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "palette", PROPERTY_HINT_RESOURCE_TYPE, "MobaMapPalette"), "set_palette", "get_palette");
	ADD_SIGNAL(MethodInfo("navigation_baked", PropertyInfo(Variant::BOOL, "success")));
}
