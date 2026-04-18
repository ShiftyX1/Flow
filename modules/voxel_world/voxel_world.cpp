#include "voxel_world.h"

#include "core/config/engine.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/object/worker_thread_pool.h"
#include "core/string/print_string.h"
#include "scene/3d/camera_3d.h"
#include "scene/main/viewport.h"

void VoxelWorld::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &VoxelWorld::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &VoxelWorld::get_seed);

	ClassDB::bind_method(D_METHOD("set_chunk_load_radius", "radius"), &VoxelWorld::set_chunk_load_radius);
	ClassDB::bind_method(D_METHOD("get_chunk_load_radius"), &VoxelWorld::get_chunk_load_radius);

	ClassDB::bind_method(D_METHOD("set_block_size", "size"), &VoxelWorld::set_block_size);
	ClassDB::bind_method(D_METHOD("get_block_size"), &VoxelWorld::get_block_size);

	ClassDB::bind_method(D_METHOD("set_sea_level", "level"), &VoxelWorld::set_sea_level);
	ClassDB::bind_method(D_METHOD("get_sea_level"), &VoxelWorld::get_sea_level);

	ClassDB::bind_method(D_METHOD("set_texture_filter", "filter"), &VoxelWorld::set_texture_filter);
	ClassDB::bind_method(D_METHOD("get_texture_filter"), &VoxelWorld::get_texture_filter);

	ClassDB::bind_method(D_METHOD("set_block_registry", "registry"), &VoxelWorld::set_block_registry);
	ClassDB::bind_method(D_METHOD("get_block_registry"), &VoxelWorld::get_block_registry);

	ClassDB::bind_method(D_METHOD("set_verbose_logging", "enabled"), &VoxelWorld::set_verbose_logging);
	ClassDB::bind_method(D_METHOD("get_verbose_logging"), &VoxelWorld::get_verbose_logging);

	ClassDB::bind_method(D_METHOD("set_show_chunk_borders", "show"), &VoxelWorld::set_show_chunk_borders);
	ClassDB::bind_method(D_METHOD("get_show_chunk_borders"), &VoxelWorld::get_show_chunk_borders);
	ClassDB::bind_method(D_METHOD("set_chunk_border_color", "color"), &VoxelWorld::set_chunk_border_color);
	ClassDB::bind_method(D_METHOD("get_chunk_border_color"), &VoxelWorld::get_chunk_border_color);

	ClassDB::bind_method(D_METHOD("set_time_of_day", "time"), &VoxelWorld::set_time_of_day);
	ClassDB::bind_method(D_METHOD("get_time_of_day"), &VoxelWorld::get_time_of_day);
	ClassDB::bind_method(D_METHOD("set_start_time_of_day", "time"), &VoxelWorld::set_start_time_of_day);
	ClassDB::bind_method(D_METHOD("get_start_time_of_day"), &VoxelWorld::get_start_time_of_day);
	ClassDB::bind_method(D_METHOD("set_day_length_seconds", "seconds"), &VoxelWorld::set_day_length_seconds);
	ClassDB::bind_method(D_METHOD("get_day_length_seconds"), &VoxelWorld::get_day_length_seconds);
	ClassDB::bind_method(D_METHOD("set_auto_advance_time", "enabled"), &VoxelWorld::set_auto_advance_time);
	ClassDB::bind_method(D_METHOD("get_auto_advance_time"), &VoxelWorld::get_auto_advance_time);
	ClassDB::bind_method(D_METHOD("set_time_speed", "speed"), &VoxelWorld::set_time_speed);
	ClassDB::bind_method(D_METHOD("get_time_speed"), &VoxelWorld::get_time_speed);
	ClassDB::bind_method(D_METHOD("advance_time", "hours"), &VoxelWorld::advance_time);
	ClassDB::bind_method(D_METHOD("is_daytime"), &VoxelWorld::is_daytime);
	ClassDB::bind_method(D_METHOD("is_nighttime"), &VoxelWorld::is_nighttime);
	ClassDB::bind_method(D_METHOD("get_day_factor"), &VoxelWorld::get_day_factor);
	ClassDB::bind_method(D_METHOD("set_fog_enabled", "enabled"), &VoxelWorld::set_fog_enabled);
	ClassDB::bind_method(D_METHOD("get_fog_enabled"), &VoxelWorld::get_fog_enabled);
	ClassDB::bind_method(D_METHOD("set_fog_distance_ratio", "ratio"), &VoxelWorld::set_fog_distance_ratio);
	ClassDB::bind_method(D_METHOD("get_fog_distance_ratio"), &VoxelWorld::get_fog_distance_ratio);
	ClassDB::bind_method(D_METHOD("set_sun_path", "path"), &VoxelWorld::set_sun_path);
	ClassDB::bind_method(D_METHOD("get_sun_path"), &VoxelWorld::get_sun_path);
	ClassDB::bind_method(D_METHOD("set_moon_path", "path"), &VoxelWorld::set_moon_path);
	ClassDB::bind_method(D_METHOD("get_moon_path"), &VoxelWorld::get_moon_path);
	ClassDB::bind_method(D_METHOD("set_environment_path", "path"), &VoxelWorld::set_environment_path);
	ClassDB::bind_method(D_METHOD("get_environment_path"), &VoxelWorld::get_environment_path);

	ClassDB::bind_method(D_METHOD("get_block_at", "world_pos"), &VoxelWorld::get_block_at);
	ClassDB::bind_method(D_METHOD("set_block_at", "world_pos", "block_id"), &VoxelWorld::set_block_at);
	ClassDB::bind_method(D_METHOD("get_block_name_at", "world_pos"), &VoxelWorld::get_block_name_at);
	ClassDB::bind_method(D_METHOD("get_biome_at", "world_pos"), &VoxelWorld::get_biome_at);
	ClassDB::bind_method(D_METHOD("get_biome_name_at", "world_pos"), &VoxelWorld::get_biome_name_at);
	ClassDB::bind_method(D_METHOD("world_to_block_pos", "world_pos"), &VoxelWorld::world_to_block_pos);
	ClassDB::bind_method(D_METHOD("block_to_world_pos", "block_pos"), &VoxelWorld::block_to_world_pos);
	ClassDB::bind_method(D_METHOD("raycast_block", "origin", "direction", "max_distance"), &VoxelWorld::raycast_block, DEFVAL(10.0f));
	ClassDB::bind_method(D_METHOD("move_body", "body", "velocity", "delta"), &VoxelWorld::move_body);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed", PROPERTY_HINT_RANGE, "-1,2147483647,1"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_load_radius", PROPERTY_HINT_RANGE, "2,16,1"), "set_chunk_load_radius", "get_chunk_load_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "block_size", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_block_size", "get_block_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sea_level", PROPERTY_HINT_RANGE, "0,191,1"), "set_sea_level", "get_sea_level");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_texture_filter", "get_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "block_registry", PROPERTY_HINT_RESOURCE_TYPE, "VoxelBlockRegistry"), "set_block_registry", "get_block_registry");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "verbose_logging"), "set_verbose_logging", "get_verbose_logging");

	ADD_GROUP("Chunk Borders", "chunk_border_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "chunk_border_show"), "set_show_chunk_borders", "get_show_chunk_borders");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "chunk_border_color"), "set_chunk_border_color", "get_chunk_border_color");

	ADD_GROUP("Day Night Cycle", "day_night_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "day_night_start_time", PROPERTY_HINT_RANGE, "0.0,24.0,0.01"), "set_start_time_of_day", "get_start_time_of_day");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "day_night_time_of_day", PROPERTY_HINT_RANGE, "0.0,24.0,0.01"), "set_time_of_day", "get_time_of_day");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "day_night_day_length", PROPERTY_HINT_RANGE, "10.0,3600.0,1.0,suffix:s"), "set_day_length_seconds", "get_day_length_seconds");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "day_night_auto_advance"), "set_auto_advance_time", "get_auto_advance_time");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "day_night_time_speed", PROPERTY_HINT_RANGE, "0.0,100.0,0.1"), "set_time_speed", "get_time_speed");

	ADD_GROUP("Fog", "fog_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "fog_enabled"), "set_fog_enabled", "get_fog_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fog_distance_ratio", PROPERTY_HINT_RANGE, "0.3,1.0,0.01"), "set_fog_distance_ratio", "get_fog_distance_ratio");

	ADD_GROUP("Environment References", "env_");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "env_sun_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "DirectionalLight3D"), "set_sun_path", "get_sun_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "env_moon_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "DirectionalLight3D"), "set_moon_path", "get_moon_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "env_environment_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "WorldEnvironment"), "set_environment_path", "get_environment_path");

	ADD_SIGNAL(MethodInfo("time_of_day_changed", PropertyInfo(Variant::FLOAT, "time")));

	ADD_SIGNAL(MethodInfo("block_placed", PropertyInfo(Variant::VECTOR3I, "block_pos"), PropertyInfo(Variant::INT, "block_id")));
	ADD_SIGNAL(MethodInfo("block_broken", PropertyInfo(Variant::VECTOR3I, "block_pos"), PropertyInfo(Variant::INT, "old_block_id")));

	// Default block ID constants (convenience for GDScript).
	BIND_CONSTANT(VOXEL_BLOCK_AIR);
	BIND_CONSTANT(VOXEL_BLOCK_GRASS);
	BIND_CONSTANT(VOXEL_BLOCK_DIRT);
	BIND_CONSTANT(VOXEL_BLOCK_STONE);
	BIND_CONSTANT(VOXEL_BLOCK_SAND);
	BIND_CONSTANT(VOXEL_BLOCK_WATER);
	BIND_CONSTANT(VOXEL_BLOCK_SNOW);
	BIND_CONSTANT(VOXEL_BLOCK_WOOD);
	BIND_CONSTANT(VOXEL_BLOCK_LEAVES);
	BIND_CONSTANT(VOXEL_BLOCK_BEDROCK);
	BIND_CONSTANT(VOXEL_BLOCK_TORCH);
}

VoxelWorld::VoxelWorld() {
}

VoxelWorld::~VoxelWorld() {
	_cleanup_world();
}

void VoxelWorld::set_seed(int p_seed) {
	seed = p_seed;
}

void VoxelWorld::set_chunk_load_radius(int p_radius) {
	chunk_load_radius = CLAMP(p_radius, 2, 16);
}

void VoxelWorld::set_block_size(float p_size) {
	block_size = CLAMP(p_size, 0.1f, 10.0f);
}

void VoxelWorld::set_sea_level(int p_level) {
	sea_level = CLAMP(p_level, 0, VoxelTerrainGenerator::CHUNK_SIZE_Y - 1);
	if (generator) {
		generator->set_sea_level(sea_level);
	}
}

void VoxelWorld::set_texture_filter(BaseMaterial3D::TextureFilter p_filter) {
	texture_filter = p_filter;
}

void VoxelWorld::set_block_registry(const Ref<VoxelBlockRegistry> &p_registry) {
	block_registry = p_registry;
}

Ref<VoxelBlockRegistry> VoxelWorld::get_block_registry() const {
	return block_registry;
}

// --- Day/Night Cycle setters ---

void VoxelWorld::set_start_time_of_day(float p_time) {
	start_time_of_day = Math::fmod(p_time, 24.0f);
	if (start_time_of_day < 0.0f) {
		start_time_of_day += 24.0f;
	}
}

void VoxelWorld::set_time_of_day(float p_time) {
	time_of_day = Math::fmod(p_time, 24.0f);
	if (time_of_day < 0.0f) {
		time_of_day += 24.0f;
	}
	// Live-update lights/fog when changed from editor or API.
	if (is_inside_tree()) {
		_update_day_night_cycle(0.0f);
	}
	emit_signal("time_of_day_changed", time_of_day);
}

void VoxelWorld::set_day_length_seconds(float p_seconds) {
	day_length_seconds = CLAMP(p_seconds, 10.0f, 3600.0f);
}

void VoxelWorld::set_fog_distance_ratio(float p_ratio) {
	fog_distance_ratio = CLAMP(p_ratio, 0.3f, 1.0f);
}

// --- Chunk border debug visualization ---

// Build a wireframe-box ArrayMesh for one chunk.
static Ref<ArrayMesh> _make_border_mesh(float sx, float sy, float sz) {
	// 12 edges × 2 vertices = 24 vertices.
	PackedVector3Array verts;
	verts.resize(24);
	Vector3 *v = verts.ptrw();

	// Bottom face.
	v[0] = Vector3(0, 0, 0);  v[1] = Vector3(sx, 0, 0);
	v[2] = Vector3(sx, 0, 0); v[3] = Vector3(sx, 0, sz);
	v[4] = Vector3(sx, 0, sz);v[5] = Vector3(0, 0, sz);
	v[6] = Vector3(0, 0, sz); v[7] = Vector3(0, 0, 0);

	// Top face.
	v[8]  = Vector3(0, sy, 0);  v[9]  = Vector3(sx, sy, 0);
	v[10] = Vector3(sx, sy, 0); v[11] = Vector3(sx, sy, sz);
	v[12] = Vector3(sx, sy, sz);v[13] = Vector3(0, sy, sz);
	v[14] = Vector3(0, sy, sz); v[15] = Vector3(0, sy, 0);

	// Vertical edges.
	v[16] = Vector3(0, 0, 0);   v[17] = Vector3(0, sy, 0);
	v[18] = Vector3(sx, 0, 0);  v[19] = Vector3(sx, sy, 0);
	v[20] = Vector3(sx, 0, sz); v[21] = Vector3(sx, sy, sz);
	v[22] = Vector3(0, 0, sz);  v[23] = Vector3(0, sy, sz);

	Array arr;
	arr.resize(Mesh::ARRAY_MAX);
	arr[Mesh::ARRAY_VERTEX] = verts;

	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_LINES, arr);
	return mesh;
}

void VoxelWorld::_spawn_chunk_border(const Vector2i &p_chunk_pos) {
	if (chunk_border_instances.has(p_chunk_pos)) {
		return; // already exists
	}

	float sx = VoxelChunk::SIZE_X * block_size;
	float sy = VoxelChunk::SIZE_Y * block_size;
	float sz = VoxelChunk::SIZE_Z * block_size;

	Ref<ArrayMesh> mesh = _make_border_mesh(sx, sy, sz);

	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
	mat->set_albedo(chunk_border_color);
	mesh->surface_set_material(0, mat);

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(mesh);
	mi->set_position(Vector3(
			p_chunk_pos.x * sx,
			0.0f,
			p_chunk_pos.y * sz));
	add_child(mi);
	mi->set_owner(nullptr);

	chunk_border_instances[p_chunk_pos] = mi;
}

void VoxelWorld::_despawn_chunk_border(const Vector2i &p_chunk_pos) {
	MeshInstance3D **ptr = chunk_border_instances.getptr(p_chunk_pos);
	if (!ptr) {
		return;
	}
	MeshInstance3D *mi = *ptr;
	if (mi && mi->get_parent() == this) {
		remove_child(mi);
		memdelete(mi);
	}
	chunk_border_instances.erase(p_chunk_pos);
}

void VoxelWorld::_rebuild_all_chunk_borders() {
	_clear_all_chunk_borders();
	for (const KeyValue<Vector2i, VoxelChunk *> &kv : loaded_chunks) {
		_spawn_chunk_border(kv.key);
	}
}

void VoxelWorld::_clear_all_chunk_borders() {
	for (const KeyValue<Vector2i, MeshInstance3D *> &kv : chunk_border_instances) {
		MeshInstance3D *mi = kv.value;
		if (mi && mi->get_parent() == this) {
			remove_child(mi);
			memdelete(mi);
		}
	}
	chunk_border_instances.clear();
}

void VoxelWorld::set_show_chunk_borders(bool p_show) {
	show_chunk_borders = p_show;
	if (!is_inside_tree()) {
		return;
	}
	if (show_chunk_borders) {
		_rebuild_all_chunk_borders();
	} else {
		_clear_all_chunk_borders();
	}
}

void VoxelWorld::set_chunk_border_color(const Color &p_color) {
	chunk_border_color = p_color;
	// Update materials on all existing border instances.
	for (const KeyValue<Vector2i, MeshInstance3D *> &kv : chunk_border_instances) {
		MeshInstance3D *mi = kv.value;
		if (!mi) {
			continue;
		}
		Ref<ArrayMesh> mesh = mi->get_mesh();
		if (mesh.is_valid() && mesh->get_surface_count() > 0) {
			Ref<StandardMaterial3D> mat = mesh->surface_get_material(0);
			if (mat.is_valid()) {
				mat->set_albedo(chunk_border_color);
			}
		}
	}
}

void VoxelWorld::set_time_speed(float p_speed) {
	time_speed = CLAMP(p_speed, 0.0f, 100.0f);
}

// Compute a smooth factor for how "daytime" it is. 1.0 = full day, 0.0 = full night.
static float _compute_day_factor(float p_time) {
	// Sunrise at 5-7, sunset at 17-19. Smooth transitions.
	if (p_time >= 7.0f && p_time <= 17.0f) {
		return 1.0f; // Full day.
	} else if (p_time >= 19.0f || p_time <= 5.0f) {
		return 0.0f; // Full night.
	} else if (p_time > 5.0f && p_time < 7.0f) {
		return (p_time - 5.0f) / 2.0f; // Sunrise.
	} else {
		return 1.0f - (p_time - 17.0f) / 2.0f; // Sunset.
	}
}

// Compute a "sunset/sunrise" factor: peaks at dawn/dusk, zero at noon/midnight.
static float _compute_twilight_factor(float p_time) {
	// Peaks at 6.0 and 18.0.
	float dawn = 1.0f - CLAMP(Math::abs(p_time - 6.0f) / 1.5f, 0.0f, 1.0f);
	float dusk = 1.0f - CLAMP(Math::abs(p_time - 18.0f) / 1.5f, 0.0f, 1.0f);
	return MAX(dawn, dusk);
}

void VoxelWorld::advance_time(float p_hours) {
	set_time_of_day(time_of_day + p_hours);
}

bool VoxelWorld::is_daytime() const {
	return time_of_day >= 6.0f && time_of_day < 18.0f;
}

bool VoxelWorld::is_nighttime() const {
	return !is_daytime();
}

float VoxelWorld::get_day_factor() const {
	return _compute_day_factor(time_of_day);
}

void VoxelWorld::set_sun_path(const NodePath &p_path) {
	sun_path = p_path;
	sun_node = nullptr;
	if (is_inside_tree()) {
		_resolve_environment_nodes();
	}
}

void VoxelWorld::set_moon_path(const NodePath &p_path) {
	moon_path = p_path;
	moon_node = nullptr;
	if (is_inside_tree()) {
		_resolve_environment_nodes();
	}
}

void VoxelWorld::set_environment_path(const NodePath &p_path) {
	environment_path = p_path;
	env_node = nullptr;
	if (is_inside_tree()) {
		_resolve_environment_nodes();
	}
}

void VoxelWorld::_resolve_environment_nodes() {
	sun_node = nullptr;
	moon_node = nullptr;
	env_node = nullptr;

	if (!sun_path.is_empty()) {
		Node *n = get_node_or_null(sun_path);
		if (n) {
			sun_node = Object::cast_to<DirectionalLight3D>(n);
		}
	}
	if (!moon_path.is_empty()) {
		Node *n = get_node_or_null(moon_path);
		if (n) {
			moon_node = Object::cast_to<DirectionalLight3D>(n);
		}
	}
	if (!environment_path.is_empty()) {
		Node *n = get_node_or_null(environment_path);
		if (n) {
			env_node = Object::cast_to<WorldEnvironment>(n);
		}
	}
}

void VoxelWorld::_update_day_night_cycle(float p_delta) {
	// Advance time.
	if (auto_advance_time && day_length_seconds > 0.0f && p_delta > 0.0f) {
		time_of_day += (p_delta / day_length_seconds) * 24.0f * time_speed;
		if (time_of_day >= 24.0f) {
			time_of_day -= 24.0f;
		}
		emit_signal("time_of_day_changed", time_of_day);
	}

	float day_factor = _compute_day_factor(time_of_day);
	float twilight = _compute_twilight_factor(time_of_day);

	// --- Sun rotation & color ---
	if (sun_node) {
		// Sun angle: 6:00 = horizon (0°), 12:00 = zenith (-90°), 18:00 = opposite horizon (-180°).
		float sun_angle = (time_of_day / 24.0f - 0.25f) * (float)Math::TAU;
		sun_node->set_rotation(Vector3(-sun_angle, 0.0f, 0.0f));

		// Sun energy: bright at noon, off at night.
		float sun_energy = CLAMP(day_factor * 1.2f, 0.0f, 1.2f);
		sun_node->set_param(Light3D::PARAM_ENERGY, sun_energy);

		// Sun color: warm white at noon, orange at sunrise/sunset. Lerp smoothly.
		Color noon_color(1.0f, 0.95f, 0.9f);
		Color twilight_color(1.0f, 0.55f, 0.2f);
		Color sun_color = noon_color.lerp(twilight_color, twilight);
		sun_node->set_color(sun_color);
	}

	// --- Moon rotation & color ---
	if (moon_node) {
		float moon_angle = (time_of_day / 24.0f - 0.25f) * (float)Math::TAU + (float)Math::PI;
		moon_node->set_rotation(Vector3(-moon_angle, 0.2f, 0.0f));

		// Moon only visible at night.
		float moon_energy = CLAMP((1.0f - day_factor) * 0.15f, 0.0f, 0.15f);
		moon_node->set_param(Light3D::PARAM_ENERGY, moon_energy);
		moon_node->set_color(Color(0.6f, 0.7f, 0.9f));
	}

	// --- Environment (ambient light + fog) ---
	if (env_node) {
		Ref<Environment> env = env_node->get_environment();
		if (env.is_valid()) {
			// Ambient light.
			Color day_ambient(0.7f, 0.75f, 0.82f);
			Color night_ambient(0.08f, 0.08f, 0.15f);
			Color twilight_ambient(0.6f, 0.45f, 0.3f);
			Color ambient = day_ambient.lerp(night_ambient, 1.0f - day_factor);
			ambient = ambient.lerp(twilight_ambient, twilight * 0.5f);
			env->set_ambient_light_color(ambient);
			env->set_ambient_light_energy(Math::lerp(0.15f, 0.5f, day_factor));

			// Fog.
			if (fog_enabled) {
				env->set_fog_enabled(true);
				env->set_fog_mode(Environment::FOG_MODE_DEPTH);

				float draw_dist = chunk_load_radius * VoxelTerrainGenerator::CHUNK_SIZE_X * block_size;
				env->set_fog_depth_begin(draw_dist * 0.05f);
				env->set_fog_depth_end(draw_dist * fog_distance_ratio);
				env->set_fog_depth_curve(1.5f);

				// Fog color matches sky feeling.
				Color day_fog(0.6f, 0.75f, 0.92f);
				Color night_fog(0.02f, 0.02f, 0.08f);
				Color twilight_fog(0.85f, 0.5f, 0.25f);
				Color fog_color = day_fog.lerp(night_fog, 1.0f - day_factor);
				fog_color = fog_color.lerp(twilight_fog, twilight * 0.6f);
				env->set_fog_light_color(fog_color);

				env->set_fog_light_energy(Math::lerp(0.3f, 1.0f, day_factor));
				env->set_fog_sun_scatter(Math::lerp(0.0f, 0.8f, twilight));
				env->set_fog_density(Math::lerp(0.015f, 0.008f, day_factor));
				env->set_fog_sky_affect(0.85f);
			} else {
				env->set_fog_enabled(false);
			}
		}
	}

	// Update voxel shader sun_intensity so voxel lighting tracks day/night.
	if (voxel_shader_material.is_valid()) {
		voxel_shader_material->set_shader_parameter("sun_intensity", day_factor);

		// Push updated sun_intensity to all live chunk surface materials.
		for (const KeyValue<Vector2i, VoxelChunk *> &E : loaded_chunks) {
			MeshInstance3D *mi = E.value->get_mesh_instance();
			if (!mi) {
				continue;
			}
			Ref<Mesh> mesh = mi->get_mesh();
			if (mesh.is_null()) {
				continue;
			}
			for (int s = 0; s < mesh->get_surface_count(); s++) {
				Ref<ShaderMaterial> sm = mesh->surface_get_material(s);
				if (sm.is_valid() && sm->get_shader() == voxel_shader) {
					sm->set_shader_parameter("sun_intensity", day_factor);
				}
			}
		}
	}
}

void VoxelWorld::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				if (verbose_logging) {
					print_line("[VoxelWorld] Editor mode — skipping generation.");
				}
				_resolve_environment_nodes();
				set_process(true);
				return;
			}
			if (verbose_logging) {
				print_line("[VoxelWorld] NOTIFICATION_READY received, initializing...");
			}
			time_of_day = start_time_of_day;
			set_process(true);
			_resolve_environment_nodes();
			_initialize_world();
		} break;

		case NOTIFICATION_PROCESS: {
			float delta = get_process_delta_time();
			_update_day_night_cycle(delta);

			if (!initialized) {
				return;
			}

			_integrate_finished_chunks();

			Viewport *vp = get_viewport();
			if (!vp) {
				return;
			}
			Camera3D *cam = vp->get_camera_3d();
			if (!cam) {
				return;
			}

			_update_chunks(cam->get_global_position());
		} break;

		case NOTIFICATION_EXIT_TREE: {
			if (verbose_logging) {
				print_line("[VoxelWorld] NOTIFICATION_EXIT_TREE — cleaning up.");
			}
			_cleanup_world();
		} break;
	}
}

void VoxelWorld::_initialize_world() {
	if (initialized) {
		return;
	}

	generator = memnew(VoxelTerrainGenerator);

	int effective_seed = seed;
	if (effective_seed < 0) {
		effective_seed = Math::random(0, 2147483647);
	}
	generator->set_seed(effective_seed);
	generator->set_sea_level(sea_level);

	if (verbose_logging) {
		print_line("[VoxelWorld] Seed: " + itos(effective_seed) + ", sea_level: " + itos(sea_level) + ", block_size: " + rtos(block_size) + ", load_radius: " + itos(chunk_load_radius));
	}

	// If no registry assigned, create one with defaults.
	if (block_registry.is_null()) {
		block_registry.instantiate();
		if (verbose_logging) {
			print_line("[VoxelWorld] No block registry set — created default registry.");
		}
	}

	// Always apply engine defaults for physics/lighting properties.
	// This is safe: create_block() skips existing entries (preserves textures loaded from .tres),
	// and the property assignments below set correct physics/lighting for all built-in types.
	block_registry->setup_defaults();

	// Finalize registry — builds flat cache arrays for thread-safe access.
	block_registry->finalize();

	material.instantiate();
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_PIXEL);
	material->set_texture_filter(texture_filter);

	// Create voxel lighting shader + material.
	{
		voxel_shader.instantiate();
		String shader_code = R"(
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;
uniform sampler2D texture_albedo : source_color, filter_nearest, repeat_enable;
uniform float sun_intensity : hint_range(0.0, 1.0) = 1.0;
uniform bool use_texture = false;
varying vec4 voxel_light;
void vertex() {
	voxel_light = CUSTOM0;
}
void fragment() {
	vec4 base = use_texture ? texture(texture_albedo, UV) : vec4(1.0);
	ALBEDO = base.rgb * COLOR.rgb;
	// Sunlight modulation + AO. Block light is handled by OmniLight3D nodes.
	float sun = voxel_light.r * sun_intensity;
	float ao = voxel_light.b;
	float brightness = sun * ao;
	// Minimum ambient so caves are never pitch black.
	brightness = max(brightness, 0.03);
	ALBEDO *= brightness;
	if (use_texture) {
		ALPHA = base.a * COLOR.a;
		ALPHA_SCISSOR_THRESHOLD = 0.5;
	}
}
)";
		voxel_shader->set_code(shader_code);
		voxel_shader_material.instantiate();
		voxel_shader_material->set_shader(voxel_shader);
		voxel_shader_material->set_shader_parameter("sun_intensity", 1.0f);
		voxel_shader_material->set_shader_parameter("use_texture", false);
	}

	initialized = true;

	if (verbose_logging) {
		print_line("[VoxelWorld] World initialized. Generating initial chunks around origin...");
	}
	_update_chunks(Vector3(0, 0, 0));
}

void VoxelWorld::_cleanup_world() {
	for (const KeyValue<Vector2i, int64_t> &E : pending_chunks) {
		WorkerThreadPool::get_singleton()->wait_for_task_completion(E.value);
	}
	pending_chunks.clear();

	for (const KeyValue<Vector2i, int64_t> &E : pending_remesh) {
		WorkerThreadPool::get_singleton()->wait_for_task_completion(E.value);
	}
	pending_remesh.clear();

	dirty_light.clear();

	{
		MutexLock lock(finished_mutex);
		finished_chunks.clear();
	}

	Vector<Vector2i> keys;
	for (const KeyValue<Vector2i, VoxelChunk *> &E : loaded_chunks) {
		keys.push_back(E.key);
	}
	if (verbose_logging) {
		print_line("[VoxelWorld] Cleanup: unloading " + itos(keys.size()) + " chunks...");
	}
	for (int i = 0; i < keys.size(); i++) {
		_unload_chunk(keys[i].x, keys[i].y);
	}
	loaded_chunks.clear();

	// Safety: remove any block lights not already freed by _unload_chunk.
	for (const KeyValue<Vector3i, OmniLight3D *> &E : block_lights) {
		if (E.value->get_parent() == this) {
			remove_child(E.value);
		}
		memdelete(E.value);
	}
	block_lights.clear();

	if (generator) {
		memdelete(generator);
		generator = nullptr;
	}

	material.unref();
	voxel_shader.unref();
	voxel_shader_material.unref();
	initialized = false;
	last_camera_chunk = Vector2i(INT32_MAX, INT32_MAX);
	if (verbose_logging) {
		print_line("[VoxelWorld] Cleanup complete.");
	}
}

void VoxelWorld::_update_chunks(const Vector3 &p_camera_pos) {
	float chunk_world_size_x = VoxelTerrainGenerator::CHUNK_SIZE_X * block_size;
	float chunk_world_size_z = VoxelTerrainGenerator::CHUNK_SIZE_Z * block_size;

	int cam_cx = (int)Math::floor(p_camera_pos.x / chunk_world_size_x);
	int cam_cz = (int)Math::floor(p_camera_pos.z / chunk_world_size_z);

	Vector2i current_chunk(cam_cx, cam_cz);

	if (current_chunk == last_camera_chunk) {
		return;
	}
	if (verbose_logging) {
		print_line("[VoxelWorld] Camera moved to chunk (" + itos(cam_cx) + ", " + itos(cam_cz) + "), updating chunks...");
	}
	last_camera_chunk = current_chunk;

	HashMap<Vector2i, bool> desired_chunks;
	for (int dx = -chunk_load_radius; dx <= chunk_load_radius; dx++) {
		for (int dz = -chunk_load_radius; dz <= chunk_load_radius; dz++) {
			Vector2i key(cam_cx + dx, cam_cz + dz);
			desired_chunks[key] = true;
		}
	}

	Vector<Vector2i> to_unload;
	for (const KeyValue<Vector2i, VoxelChunk *> &E : loaded_chunks) {
		if (!desired_chunks.has(E.key)) {
			to_unload.push_back(E.key);
		}
	}
	if (to_unload.size() > 0 && verbose_logging) {
		print_line("[VoxelWorld] Unloading " + itos(to_unload.size()) + " chunks.");
	}
	for (int i = 0; i < to_unload.size(); i++) {
		_unload_chunk(to_unload[i].x, to_unload[i].y);
	}

	Vector<Vector2i> pending_to_cancel;
	for (const KeyValue<Vector2i, int64_t> &E : pending_chunks) {
		if (!desired_chunks.has(E.key)) {
			pending_to_cancel.push_back(E.key);
		}
	}
	for (int i = 0; i < pending_to_cancel.size(); i++) {
		pending_chunks.erase(pending_to_cancel[i]);
	}

	int requested_count = 0;
	for (const KeyValue<Vector2i, bool> &E : desired_chunks) {
		if (!loaded_chunks.has(E.key) && !pending_chunks.has(E.key)) {
			_request_chunk(E.key.x, E.key.y);
			requested_count++;
		}
	}
	if (requested_count > 0 && verbose_logging) {
		print_line("[VoxelWorld] Requested " + itos(requested_count) + " chunks for background generation. Pending: " + itos(pending_chunks.size()) + ".");
	}
}

// Background chunk generation
void VoxelWorld::_chunk_generation_task(void *p_userdata) {
	ChunkTaskData *data = static_cast<ChunkTaskData *>(p_userdata);
	VoxelWorld *world = data->world;

	ChunkTaskResult result;
	result.key = data->key;
	result.is_remesh = data->is_remesh;

	if (data->is_remesh) {
		result.blocks = data->pre_blocks;
		result.light_data = data->pre_light;
	} else {
		result.blocks = world->generator->generate_chunk_data(data->chunk_x, data->chunk_z);
	}

	// --- Light propagation ---
	const int total_blocks = VoxelTerrainGenerator::CHUNK_SIZE_X * VoxelTerrainGenerator::CHUNK_SIZE_Y * VoxelTerrainGenerator::CHUNK_SIZE_Z;
	if (result.light_data.size() != total_blocks) {
		result.light_data.resize(total_blocks);
		memset(result.light_data.ptrw(), 0, total_blocks);
	}

	VoxelLightMap::NeighborData light_nb;
	// Block arrays.
	if (data->neighbor_px.size() > 0) { light_nb.blocks_px = data->neighbor_px.ptr(); }
	if (data->neighbor_nx.size() > 0) { light_nb.blocks_nx = data->neighbor_nx.ptr(); }
	if (data->neighbor_pz.size() > 0) { light_nb.blocks_pz = data->neighbor_pz.ptr(); }
	if (data->neighbor_nz.size() > 0) { light_nb.blocks_nz = data->neighbor_nz.ptr(); }
	// Light arrays.
	if (data->neighbor_light_px.size() > 0) { light_nb.light_px = data->neighbor_light_px.ptr(); }
	if (data->neighbor_light_nx.size() > 0) { light_nb.light_nx = data->neighbor_light_nx.ptr(); }
	if (data->neighbor_light_pz.size() > 0) { light_nb.light_pz = data->neighbor_light_pz.ptr(); }
	if (data->neighbor_light_nz.size() > 0) { light_nb.light_nz = data->neighbor_light_nz.ptr(); }

	VoxelLightMap::propagate_sunlight(result.light_data.ptrw(), result.blocks.ptr(), light_nb, world->block_registry->get_cache_light_opacity().ptr());

	// Build NeighborBlocks from snapshots captured on the main thread at queue time.
	VoxelMesher::NeighborBlocks nb;
	if (data->neighbor_px.size() > 0) { nb.px = data->neighbor_px.ptr(); }
	if (data->neighbor_nx.size() > 0) { nb.nx = data->neighbor_nx.ptr(); }
	if (data->neighbor_pz.size() > 0) { nb.pz = data->neighbor_pz.ptr(); }
	if (data->neighbor_nz.size() > 0) { nb.nz = data->neighbor_nz.ptr(); }

	// Build NeighborLight from snapshots.
	VoxelMesher::NeighborLight mesh_nl;
	if (data->neighbor_light_px.size() > 0) { mesh_nl.px = data->neighbor_light_px.ptr(); }
	if (data->neighbor_light_nx.size() > 0) { mesh_nl.nx = data->neighbor_light_nx.ptr(); }
	if (data->neighbor_light_pz.size() > 0) { mesh_nl.pz = data->neighbor_light_pz.ptr(); }
	if (data->neighbor_light_nz.size() > 0) { mesh_nl.nz = data->neighbor_light_nz.ptr(); }

	result.surfaces = VoxelMesher::build_chunk_mesh(result.blocks, world->block_size, world->block_registry, nb, result.light_data.ptr(), mesh_nl);

	{
		MutexLock lock(world->finished_mutex);
		world->finished_chunks.push_back(result);
	}

	memdelete(data);
}

void VoxelWorld::_request_chunk(int p_cx, int p_cz) {
	Vector2i key(p_cx, p_cz);
	if (loaded_chunks.has(key) || pending_chunks.has(key)) {
		return;
	}

	ChunkTaskData *task_data = memnew(ChunkTaskData);
	task_data->world = this;
	task_data->key = key;
	task_data->chunk_x = p_cx;
	task_data->chunk_z = p_cz;

	// Snapshot loaded neighbour block arrays so the background thread can build seam-free meshes.
	auto snapshot_nb = [&](int dx, int dz) -> Vector<uint16_t> {
		VoxelChunk *const *nb = loaded_chunks.getptr(Vector2i(p_cx + dx, p_cz + dz));
		return nb ? (*nb)->get_blocks() : Vector<uint16_t>();
	};
	auto snapshot_light = [&](int dx, int dz) -> Vector<uint8_t> {
		VoxelChunk *const *nb = loaded_chunks.getptr(Vector2i(p_cx + dx, p_cz + dz));
		return nb ? (*nb)->get_light_data() : Vector<uint8_t>();
	};
	task_data->neighbor_px = snapshot_nb(1, 0);
	task_data->neighbor_nx = snapshot_nb(-1, 0);
	task_data->neighbor_pz = snapshot_nb(0, 1);
	task_data->neighbor_nz = snapshot_nb(0, -1);
	task_data->neighbor_light_px = snapshot_light(1, 0);
	task_data->neighbor_light_nx = snapshot_light(-1, 0);
	task_data->neighbor_light_pz = snapshot_light(0, 1);
	task_data->neighbor_light_nz = snapshot_light(0, -1);

	int64_t task_id = WorkerThreadPool::get_singleton()->add_native_task(
			&VoxelWorld::_chunk_generation_task, task_data, false, "VoxelChunkGen");

	pending_chunks[key] = task_id;
}

// ----- Chunk mesh helpers -----

void VoxelWorld::_apply_surfaces_to_chunk(VoxelChunk *p_chunk, const Vector<VoxelMesher::MeshSurface> &p_surfaces) {
	// Remove old mesh instance if any.
	MeshInstance3D *old_mi = p_chunk->get_mesh_instance();
	if (old_mi) {
		if (old_mi->get_parent() == this) {
			remove_child(old_mi);
		}
		memdelete(old_mi);
		p_chunk->set_mesh_instance(nullptr);
	}

	if (p_surfaces.size() == 0) {
		return;
	}

	Ref<ArrayMesh> array_mesh;
	array_mesh.instantiate();

	for (int s = 0; s < p_surfaces.size(); s++) {
		uint64_t fmt_flags = p_surfaces[s].custom_format_flags;
		array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, p_surfaces[s].arrays, Array(), Dictionary(), fmt_flags);
		if (p_surfaces[s].shader_material.is_valid()) {
			Ref<ShaderMaterial> mat = p_surfaces[s].shader_material;
			if (p_surfaces[s].texture.is_valid() && mat->get_shader().is_valid()) {
				mat->set_shader_parameter("texture_albedo", p_surfaces[s].texture);
			}
			array_mesh->surface_set_material(s, mat);
		} else if (fmt_flags != 0 && voxel_shader_material.is_valid()) {
			// Surface has CUSTOM0 light data — use voxel lighting shader.
			Ref<ShaderMaterial> mat;
			mat.instantiate();
			mat->set_shader(voxel_shader);
			mat->set_shader_parameter("sun_intensity", voxel_shader_material->get_shader_parameter("sun_intensity"));
			if (p_surfaces[s].texture.is_valid()) {
				mat->set_shader_parameter("texture_albedo", p_surfaces[s].texture);
				mat->set_shader_parameter("use_texture", true);
			} else {
				mat->set_shader_parameter("use_texture", false);
			}
			array_mesh->surface_set_material(s, mat);
		} else if (p_surfaces[s].texture.is_valid()) {
			Ref<StandardMaterial3D> mat;
			mat.instantiate();
			mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, p_surfaces[s].texture);
			mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			mat->set_texture_filter(texture_filter);

			int btype = p_surfaces[s].block_type;
			if (btype >= 0 && block_registry.is_valid() && block_registry->is_uses_alpha(btype)) {
				mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
				mat->set_alpha_scissor_threshold(0.5f);
			}
			array_mesh->surface_set_material(s, mat);
		} else if (material.is_valid()) {
			array_mesh->surface_set_material(s, material);
		}
	}

	MeshInstance3D *mi = memnew(MeshInstance3D);
	mi->set_mesh(array_mesh);

	Vector2i cpos = p_chunk->get_chunk_pos();
	float world_x = cpos.x * VoxelChunk::SIZE_X * block_size;
	float world_z = cpos.y * VoxelChunk::SIZE_Z * block_size;
	mi->set_position(Vector3(world_x, 0, world_z));

	p_chunk->set_mesh_instance(mi);
	add_child(mi);
	mi->set_owner(nullptr);
}

// ---- Block light (OmniLight3D) management ----

void VoxelWorld::_spawn_block_light(const Vector3i &p_block_pos, int p_type) {
	if (block_lights.has(p_block_pos)) {
		return; // Already spawned.
	}
	uint8_t emission = block_registry->get_emission(p_type);
	if (emission == 0) {
		return;
	}
	OmniLight3D *light = memnew(OmniLight3D);
	light->set_param(Light3D::PARAM_RANGE, (float)emission * block_size);
	light->set_param(Light3D::PARAM_ENERGY, 10.0f);
	light->set_color(block_registry->get_cached_light_color(p_type));
	light->set_position(block_to_world_pos(p_block_pos));
	light->set_shadow(true);
	add_child(light);
	block_lights[p_block_pos] = light;
}

void VoxelWorld::_remove_block_light(const Vector3i &p_block_pos) {
	OmniLight3D **lptr = block_lights.getptr(p_block_pos);
	if (!lptr) {
		return;
	}
	OmniLight3D *light = *lptr;
	if (light->get_parent() == this) {
		remove_child(light);
	}
	memdelete(light);
	block_lights.erase(p_block_pos);
}

void VoxelWorld::_scan_chunk_for_lights(VoxelChunk *p_chunk) {
	const Vector<uint16_t> &blocks = p_chunk->get_blocks();
	Vector2i cpos = p_chunk->get_chunk_pos();
	int base_bx = cpos.x * VoxelChunk::SIZE_X;
	int base_bz = cpos.y * VoxelChunk::SIZE_Z;
	for (int y = 0; y < VoxelChunk::SIZE_Y; y++) {
		for (int z = 0; z < VoxelChunk::SIZE_Z; z++) {
			for (int x = 0; x < VoxelChunk::SIZE_X; x++) {
				int type = (int)blocks[VoxelTerrainGenerator::block_index(x, y, z)];
				if (block_registry->get_emission(type) > 0) {
					_spawn_block_light(Vector3i(base_bx + x, y, base_bz + z), type);
				}
			}
		}
	}
}

// Rebuild a chunk mesh synchronously on the main thread (used in set_block_at for immediate feedback).
void VoxelWorld::_rebuild_chunk_mesh(VoxelChunk *p_chunk) {
	Vector2i cpos = p_chunk->get_chunk_pos();
	VoxelMesher::NeighborBlocks nb;
	auto snap = [&](int dx, int dz) -> const uint16_t * {
		VoxelChunk *const *n = loaded_chunks.getptr(Vector2i(cpos.x + dx, cpos.y + dz));
		return n ? (*n)->get_blocks().ptr() : nullptr;
	};
	nb.px = snap(1, 0);
	nb.nx = snap(-1, 0);
	nb.pz = snap(0, 1);
	nb.nz = snap(0, -1);

	// Re-propagate light for this chunk.
	p_chunk->ensure_light_data();
	memset(p_chunk->get_light_data_rw().ptrw(), 0, p_chunk->get_light_data().size());

	VoxelLightMap::NeighborData light_nb;
	auto snap_blocks_vec = [&](int dx, int dz) -> const uint16_t * {
		VoxelChunk *const *n = loaded_chunks.getptr(Vector2i(cpos.x + dx, cpos.y + dz));
		return n ? (*n)->get_blocks().ptr() : nullptr;
	};
	auto snap_light = [&](int dx, int dz) -> const uint8_t * {
		VoxelChunk *const *n = loaded_chunks.getptr(Vector2i(cpos.x + dx, cpos.y + dz));
		return (n && (*n)->get_light_data().size() > 0) ? (*n)->get_light_data().ptr() : nullptr;
	};
	light_nb.blocks_px = snap_blocks_vec(1, 0);
	light_nb.blocks_nx = snap_blocks_vec(-1, 0);
	light_nb.blocks_pz = snap_blocks_vec(0, 1);
	light_nb.blocks_nz = snap_blocks_vec(0, -1);
	light_nb.light_px = snap_light(1, 0);
	light_nb.light_nx = snap_light(-1, 0);
	light_nb.light_pz = snap_light(0, 1);
	light_nb.light_nz = snap_light(0, -1);

	VoxelLightMap::propagate_sunlight(p_chunk->get_light_data_rw().ptrw(), p_chunk->get_blocks().ptr(), light_nb, block_registry->get_cache_light_opacity().ptr());

	VoxelMesher::NeighborLight mesh_nl;
	mesh_nl.px = snap_light(1, 0);
	mesh_nl.nx = snap_light(-1, 0);
	mesh_nl.pz = snap_light(0, 1);
	mesh_nl.nz = snap_light(0, -1);

	Vector<VoxelMesher::MeshSurface> surfaces = VoxelMesher::build_chunk_mesh(
			p_chunk->get_blocks(), block_size, block_registry, nb,
			p_chunk->get_light_data().ptr(), mesh_nl);
	_apply_surfaces_to_chunk(p_chunk, surfaces);
}

// Queue an async background remesh for an already-loaded chunk.
void VoxelWorld::_request_remesh(const Vector2i &p_key) {
	// If a regular load or remesh is already in flight, defer for next frame.
	if (pending_chunks.has(p_key) || pending_remesh.has(p_key)) {
		dirty_light.insert(p_key);
		return;
	}
	VoxelChunk *const *chunk_ptr = loaded_chunks.getptr(p_key);
	if (!chunk_ptr) {
		return;
	}

	ChunkTaskData *task_data = memnew(ChunkTaskData);
	task_data->world = this;
	task_data->key = p_key;
	task_data->is_remesh = true;
	task_data->pre_blocks = (*chunk_ptr)->get_blocks(); // snapshot
	task_data->pre_light = (*chunk_ptr)->get_light_data(); // light snapshot

	auto snapshot_nb = [&](int dx, int dz) -> Vector<uint16_t> {
		VoxelChunk *const *nb = loaded_chunks.getptr(Vector2i(p_key.x + dx, p_key.y + dz));
		return nb ? (*nb)->get_blocks() : Vector<uint16_t>();
	};
	auto snapshot_light_nb = [&](int dx, int dz) -> Vector<uint8_t> {
		VoxelChunk *const *nb = loaded_chunks.getptr(Vector2i(p_key.x + dx, p_key.y + dz));
		return nb ? (*nb)->get_light_data() : Vector<uint8_t>();
	};
	task_data->neighbor_px = snapshot_nb(1, 0);
	task_data->neighbor_nx = snapshot_nb(-1, 0);
	task_data->neighbor_pz = snapshot_nb(0, 1);
	task_data->neighbor_nz = snapshot_nb(0, -1);
	task_data->neighbor_light_px = snapshot_light_nb(1, 0);
	task_data->neighbor_light_nx = snapshot_light_nb(-1, 0);
	task_data->neighbor_light_pz = snapshot_light_nb(0, 1);
	task_data->neighbor_light_nz = snapshot_light_nb(0, -1);

	int64_t task_id = WorkerThreadPool::get_singleton()->add_native_task(
			&VoxelWorld::_chunk_generation_task, task_data, false, "VoxelChunkRemesh");
	pending_remesh[p_key] = task_id;
}

void VoxelWorld::_integrate_finished_chunks() {
	Vector<ChunkTaskResult> to_integrate;

	{
		MutexLock lock(finished_mutex);
		if (finished_chunks.is_empty()) {
			return;
		}
		to_integrate = finished_chunks;
		finished_chunks.clear();
	}

	int integrated = 0;
	for (int i = 0; i < to_integrate.size(); i++) {
		const ChunkTaskResult &result = to_integrate[i];

		if (result.is_remesh) {
			// Complete the remesh task.
			int64_t *task_id_ptr = pending_remesh.getptr(result.key);
			if (task_id_ptr) {
				WorkerThreadPool::get_singleton()->wait_for_task_completion(*task_id_ptr);
				pending_remesh.erase(result.key);
			}
			// Apply updated surfaces to the existing chunk (if still loaded).
			VoxelChunk *const *chunk_ptr = loaded_chunks.getptr(result.key);
			if (chunk_ptr) {
				if (result.light_data.size() > 0) {
					(*chunk_ptr)->set_light_data(result.light_data);
				}
				_apply_surfaces_to_chunk(*chunk_ptr, result.surfaces);
			}
			// Remesh results don't count toward the per-frame integration limit.
			continue;
		}

		int64_t *task_id_ptr = pending_chunks.getptr(result.key);
		if (task_id_ptr) {
			WorkerThreadPool::get_singleton()->wait_for_task_completion(*task_id_ptr);
			pending_chunks.erase(result.key);
		} else {
			// This chunk was cancelled (camera moved away). Discard it.
			continue;
		}

		if (loaded_chunks.has(result.key)) {
			continue;
		}

		VoxelChunk *chunk = memnew(VoxelChunk);
		chunk->set_chunk_pos(result.key);
		chunk->set_blocks(result.blocks);
		if (result.light_data.size() > 0) {
			chunk->set_light_data(result.light_data);
		}

		// Register the chunk and apply the pre-built surfaces (built off-thread).
		loaded_chunks[result.key] = chunk;
		_apply_surfaces_to_chunk(chunk, result.surfaces);
		_scan_chunk_for_lights(chunk);

		// Spawn chunk border visualizer when enabled.
		if (show_chunk_borders) {
			_spawn_chunk_border(result.key);
		}

		// Mark this chunk and its cardinal neighbours for light recalculation.
		// The newly loaded chunk was generated without some neighbours' light,
		// and neighbours need to incorporate this chunk's light at their borders.
		dirty_light.insert(result.key);
		const Vector2i dirs[4] = {
			Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1)
		};
		for (int d = 0; d < 4; d++) {
			dirty_light.insert(result.key + dirs[d]);
		}

		integrated++;

		if (integrated >= chunks_per_frame) {
			if (i + 1 < to_integrate.size()) {
				MutexLock lock(finished_mutex);
				for (int j = i + 1; j < to_integrate.size(); j++) {
					finished_chunks.push_back(to_integrate[j]);
				}
			}
			break;
		}
	}

	if (integrated > 0 && verbose_logging) {
		print_verbose("[VoxelWorld] Integrated " + itos(integrated) + " chunks this frame. Total loaded: " + itos(loaded_chunks.size()) + ".");
	}

	// Process deferred light-dirty chunks: attempt to queue remesh for each.
	// Chunks that still can't be queued stay dirty for the next frame.
	if (!dirty_light.is_empty()) {
		HashSet<Vector2i> still_dirty;
		for (const Vector2i &key : dirty_light) {
			if (pending_chunks.has(key) || pending_remesh.has(key)) {
				still_dirty.insert(key);
				continue;
			}
			if (!loaded_chunks.has(key)) {
				continue; // Not loaded (yet or anymore) — drop it.
			}
			_request_remesh(key);
		}
		dirty_light = still_dirty;
	}
}

void VoxelWorld::_unload_chunk(int p_cx, int p_cz) {
	Vector2i key(p_cx, p_cz);
	VoxelChunk **chunk_ptr = loaded_chunks.getptr(key);
	if (!chunk_ptr) {
		return;
	}

	VoxelChunk *chunk = *chunk_ptr;
	MeshInstance3D *mi = chunk->get_mesh_instance();
	if (mi && mi->get_parent() == this) {
		remove_child(mi);
		memdelete(mi);
	}

	// Remove OmniLight3D nodes for emissive blocks in this chunk.
	{
		const Vector<uint16_t> &blocks = chunk->get_blocks();
		for (int y = 0; y < VoxelChunk::SIZE_Y; y++) {
			for (int z = 0; z < VoxelChunk::SIZE_Z; z++) {
				for (int x = 0; x < VoxelChunk::SIZE_X; x++) {
					int type = (int)blocks[VoxelTerrainGenerator::block_index(x, y, z)];
					if (block_registry->get_emission(type) > 0) {
						_remove_block_light(Vector3i(p_cx * VoxelChunk::SIZE_X + x, y, p_cz * VoxelChunk::SIZE_Z + z));
					}
				}
			}
		}
	}

	// Remove chunk border visualizer if enabled.
	_despawn_chunk_border(key);

	memdelete(chunk);
	loaded_chunks.erase(key);
	pending_remesh.erase(key); // discard any pending remesh; result will be ignored
}

// --- Coordinate helpers ---

Vector2i VoxelWorld::_world_to_chunk(const Vector3 &p_world_pos) const {
	int cx = (int)Math::floor(p_world_pos.x / (VoxelChunk::SIZE_X * block_size));
	int cz = (int)Math::floor(p_world_pos.z / (VoxelChunk::SIZE_Z * block_size));
	return Vector2i(cx, cz);
}

Vector3i VoxelWorld::_world_to_local_block(const Vector3 &p_world_pos) const {
	float chunk_world_x = Math::floor(p_world_pos.x / (VoxelChunk::SIZE_X * block_size)) * VoxelChunk::SIZE_X * block_size;
	float chunk_world_z = Math::floor(p_world_pos.z / (VoxelChunk::SIZE_Z * block_size)) * VoxelChunk::SIZE_Z * block_size;

	int lx = (int)Math::floor((p_world_pos.x - chunk_world_x) / block_size);
	int ly = (int)Math::floor(p_world_pos.y / block_size);
	int lz = (int)Math::floor((p_world_pos.z - chunk_world_z) / block_size);

	return Vector3i(lx, ly, lz);
}

Vector3i VoxelWorld::world_to_block_pos(const Vector3 &p_world_pos) const {
	int bx = (int)Math::floor(p_world_pos.x / block_size);
	int by = (int)Math::floor(p_world_pos.y / block_size);
	int bz = (int)Math::floor(p_world_pos.z / block_size);
	return Vector3i(bx, by, bz);
}

Vector3 VoxelWorld::block_to_world_pos(const Vector3i &p_block_pos) const {
	return Vector3(p_block_pos.x * block_size + block_size * 0.5f,
			p_block_pos.y * block_size + block_size * 0.5f,
			p_block_pos.z * block_size + block_size * 0.5f);
}

// --- Block interaction API ---

int VoxelWorld::get_block_at(const Vector3 &p_world_pos) const {
	Vector2i chunk_key = _world_to_chunk(p_world_pos);
	VoxelChunk *const *chunk_ptr = loaded_chunks.getptr(chunk_key);
	if (!chunk_ptr) {
		return 0; // AIR if chunk not loaded.
	}
	Vector3i local = _world_to_local_block(p_world_pos);
	return (int)(*chunk_ptr)->get_block(local.x, local.y, local.z);
}

void VoxelWorld::set_block_at(const Vector3 &p_world_pos, int p_block_id) {
	Vector2i chunk_key = _world_to_chunk(p_world_pos);
	VoxelChunk **chunk_ptr = loaded_chunks.getptr(chunk_key);
	if (!chunk_ptr) {
		if (verbose_logging) {
			print_line("[VoxelWorld] set_block_at: chunk not loaded at (" + itos(chunk_key.x) + ", " + itos(chunk_key.y) + ").");
		}
		return;
	}
	Vector3i local = _world_to_local_block(p_world_pos);
	VoxelChunk *chunk = *chunk_ptr;

	int old_id = (int)chunk->get_block(local.x, local.y, local.z);
	chunk->set_block(local.x, local.y, local.z, p_block_id);

	// Manage OmniLight3D for emissive blocks (e.g. torches).
	{
		Vector3i light_pos(
				chunk_key.x * VoxelChunk::SIZE_X + local.x,
				local.y,
				chunk_key.y * VoxelChunk::SIZE_Z + local.z);
		if (block_registry->get_emission(old_id) > 0) {
			_remove_block_light(light_pos);
		}
		if (block_registry->get_emission(p_block_id) > 0) {
			_spawn_block_light(light_pos, p_block_id);
		}
	}

	// Rebuild this chunk synchronously for immediate visual feedback.
	_rebuild_chunk_mesh(chunk);

	// If the changed block is on a border, queue async remesh for the affected neighbour(s).
	if (local.x == 0) {
		_request_remesh(Vector2i(chunk_key.x - 1, chunk_key.y));
	}
	if (local.x == VoxelChunk::SIZE_X - 1) {
		_request_remesh(Vector2i(chunk_key.x + 1, chunk_key.y));
	}
	if (local.z == 0) {
		_request_remesh(Vector2i(chunk_key.x, chunk_key.y - 1));
	}
	if (local.z == VoxelChunk::SIZE_Z - 1) {
		_request_remesh(Vector2i(chunk_key.x, chunk_key.y + 1));
	}

	Vector3i block_pos = world_to_block_pos(p_world_pos);

	if (p_block_id == 0 && old_id != 0) {
		emit_signal("block_broken", block_pos, old_id);
	} else if (p_block_id != 0) {
		emit_signal("block_placed", block_pos, p_block_id);
	}
}

String VoxelWorld::get_block_name_at(const Vector3 &p_world_pos) const {
	int block_id = get_block_at(p_world_pos);
	return block_registry.is_valid() ? block_registry->get_block_name(block_id) : String();
}

int VoxelWorld::get_biome_at(const Vector3 &p_world_pos) const {
	if (!generator) {
		return -1;
	}
	int bx = (int)Math::floor(p_world_pos.x / block_size);
	int bz = (int)Math::floor(p_world_pos.z / block_size);
	return generator->get_biome_index_at(bx, bz);
}

String VoxelWorld::get_biome_name_at(const Vector3 &p_world_pos) const {
	int index = get_biome_at(p_world_pos);
	if (index < 0) {
		return String("Unknown");
	}
	// Fallback names for the four built-in static biomes.
	static const char *fallback_names[] = { "Desert", "Meadow", "Forest", "Mountains" };
	if (index < 4) {
		return String(fallback_names[index]);
	}
	return String("Biome_") + itos(index);
}

// --- Voxel raycast (DDA algorithm) ---

Dictionary VoxelWorld::raycast_block(const Vector3 &p_origin, const Vector3 &p_direction, float p_max_distance) const {
	Dictionary result;

	if (p_direction.is_zero_approx()) {
		return result;
	}

	Vector3 dir = p_direction.normalized();
	float inv_bs = 1.0f / block_size;

	// Current block position.
	int bx = (int)Math::floor(p_origin.x * inv_bs);
	int by = (int)Math::floor(p_origin.y * inv_bs);
	int bz = (int)Math::floor(p_origin.z * inv_bs);

	// Step direction.
	int step_x = (dir.x >= 0) ? 1 : -1;
	int step_y = (dir.y >= 0) ? 1 : -1;
	int step_z = (dir.z >= 0) ? 1 : -1;

	// Distance to next voxel boundary.
	float next_x = ((dir.x >= 0) ? (bx + 1) : bx) * block_size;
	float next_y = ((dir.y >= 0) ? (by + 1) : by) * block_size;
	float next_z = ((dir.z >= 0) ? (bz + 1) : bz) * block_size;

	float t_max_x = (dir.x != 0.0f) ? (next_x - p_origin.x) / dir.x : 1e30f;
	float t_max_y = (dir.y != 0.0f) ? (next_y - p_origin.y) / dir.y : 1e30f;
	float t_max_z = (dir.z != 0.0f) ? (next_z - p_origin.z) / dir.z : 1e30f;

	float t_delta_x = (dir.x != 0.0f) ? (block_size / Math::abs(dir.x)) : 1e30f;
	float t_delta_y = (dir.y != 0.0f) ? (block_size / Math::abs(dir.y)) : 1e30f;
	float t_delta_z = (dir.z != 0.0f) ? (block_size / Math::abs(dir.z)) : 1e30f;

	Vector3i hit_normal(0, 0, 0);
	float t = 0.0f;

	int max_steps = (int)(p_max_distance * inv_bs) + 2;

	for (int i = 0; i < max_steps; i++) {
		// Check current block.
		if (by >= 0 && by < VoxelChunk::SIZE_Y) {
			Vector3 block_center((bx + 0.5f) * block_size, (by + 0.5f) * block_size, (bz + 0.5f) * block_size);
			int block_id = get_block_at(block_center);

			bool is_solid = block_registry.is_valid() && block_registry->is_solid(block_id);

			if (is_solid) {
				result["position"] = p_origin + dir * t;
				result["normal"] = Vector3(hit_normal.x, hit_normal.y, hit_normal.z);
				result["block_id"] = block_id;
				result["block_pos"] = Vector3i(bx, by, bz);
				return result;
			}
		}

		// Advance to next voxel boundary (DDA step).
		if (t_max_x < t_max_y) {
			if (t_max_x < t_max_z) {
				t = t_max_x;
				bx += step_x;
				t_max_x += t_delta_x;
				hit_normal = Vector3i(-step_x, 0, 0);
			} else {
				t = t_max_z;
				bz += step_z;
				t_max_z += t_delta_z;
				hit_normal = Vector3i(0, 0, -step_z);
			}
		} else {
			if (t_max_y < t_max_z) {
				t = t_max_y;
				by += step_y;
				t_max_y += t_delta_y;
				hit_normal = Vector3i(0, -step_y, 0);
			} else {
				t = t_max_z;
				bz += step_z;
				t_max_z += t_delta_z;
				hit_normal = Vector3i(0, 0, -step_z);
			}
		}

		if (t > p_max_distance) {
			break;
		}
	}

	return result; // Empty — nothing hit.
}

// move_body: axis-separated AABB sweep against voxel grid

static _FORCE_INLINE_ bool _is_block_solid(int p_block_id) {
	return p_block_id != VOXEL_BLOCK_AIR && p_block_id != VOXEL_BLOCK_WATER;
}

// move_body: axis-separated AABB sweep against voxel grid
Dictionary VoxelWorld::move_body(const AABB &p_body, const Vector3 &p_velocity, float p_delta) const {
	const float bs = block_size;
	const float inv_bs = 1.0f / bs;

	Vector3 pos = p_body.position;
	Vector3 size = p_body.size;
	Vector3 vel = p_velocity;
	Vector3 move = vel * p_delta;

	bool on_ground = false;

	// Resolve one axis at a time: Y first (gravity), then X, then Z.
	// For each axis, move, then check overlapping blocks and push out.

	auto resolve_axis = [&](int axis) {
		float axis_move = move[axis];
		if (Math::is_zero_approx(axis_move)) {
			return;
		}

		Vector3 new_pos = pos;
		new_pos[axis] += axis_move;

		AABB swept(new_pos, size);

		int min_bx = (int)Math::floor(swept.position.x * inv_bs);
		int min_by = (int)Math::floor(swept.position.y * inv_bs);
		int min_bz = (int)Math::floor(swept.position.z * inv_bs);
		int max_bx = (int)Math::floor((swept.position.x + swept.size.x - 0.001f) * inv_bs);
		int max_by = (int)Math::floor((swept.position.y + swept.size.y - 0.001f) * inv_bs);
		int max_bz = (int)Math::floor((swept.position.z + swept.size.z - 0.001f) * inv_bs);

		for (int by = min_by; by <= max_by; by++) {
			for (int bz = min_bz; bz <= max_bz; bz++) {
				for (int bx = min_bx; bx <= max_bx; bx++) {
					Vector3 bc((bx + 0.5f) * bs, (by + 0.5f) * bs, (bz + 0.5f) * bs);
					int bid = get_block_at(bc);
					if (!_is_block_solid(bid)) {
						continue;
					}

					float col_h = 1.0f;
					if (block_registry.is_valid() && block_registry->has_block(bid)) {
						col_h = block_registry->get_block_collision_height(bid);
					}
					AABB block_aabb(Vector3(bx * bs, by * bs, bz * bs), Vector3(bs, bs * col_h, bs));
					if (!swept.intersects(block_aabb)) {
						continue;
					}

					// Push out along this axis.
					if (axis == 0) { // X
						if (axis_move > 0) {
							new_pos.x = block_aabb.position.x - size.x;
						} else {
							new_pos.x = block_aabb.position.x + bs;
						}
						vel.x = 0;
					} else if (axis == 1) { // Y
						if (axis_move < 0) {
							new_pos.y = block_aabb.position.y + bs;
							on_ground = true;
						} else {
							new_pos.y = block_aabb.position.y - size.y;
						}
						vel.y = 0;
					} else { // Z
						if (axis_move > 0) {
							new_pos.z = block_aabb.position.z - size.z;
						} else {
							new_pos.z = block_aabb.position.z + bs;
						}
						vel.z = 0;
					}

					// Recompute swept AABB after correction.
					swept = AABB(new_pos, size);
				}
			}
		}

		pos = new_pos;
	};

	// Y first, then X, then Z.
	resolve_axis(1);
	resolve_axis(0);
	resolve_axis(2);

	Dictionary result;
	result["position"] = pos;
	result["velocity"] = vel;
	result["on_ground"] = on_ground;
	return result;
}
