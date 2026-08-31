#include "voxel_world.h"

#include "core/config/engine.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/string/print_string.h"
#include "core/variant/typed_array.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/main/viewport.h"
#include "servers/rendering/rendering_server.h"

static bool voxel_global_shader_parameter_registered = false;

void VoxelWorld::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_seed", "seed"), &VoxelWorld::set_seed);
	ClassDB::bind_method(D_METHOD("get_seed"), &VoxelWorld::get_seed);

	ClassDB::bind_method(D_METHOD("set_chunk_load_radius", "radius"), &VoxelWorld::set_chunk_load_radius);
	ClassDB::bind_method(D_METHOD("get_chunk_load_radius"), &VoxelWorld::get_chunk_load_radius);
	ClassDB::bind_method(D_METHOD("set_chunks_per_frame", "count"), &VoxelWorld::set_chunks_per_frame);
	ClassDB::bind_method(D_METHOD("get_chunks_per_frame"), &VoxelWorld::get_chunks_per_frame);
	ClassDB::bind_method(D_METHOD("set_max_pending_chunk_tasks", "count"), &VoxelWorld::set_max_pending_chunk_tasks);
	ClassDB::bind_method(D_METHOD("get_max_pending_chunk_tasks"), &VoxelWorld::get_max_pending_chunk_tasks);
	ClassDB::bind_method(D_METHOD("set_chunk_requests_per_frame", "count"), &VoxelWorld::set_chunk_requests_per_frame);
	ClassDB::bind_method(D_METHOD("get_chunk_requests_per_frame"), &VoxelWorld::get_chunk_requests_per_frame);
	ClassDB::bind_method(D_METHOD("set_remesh_requests_per_frame", "count"), &VoxelWorld::set_remesh_requests_per_frame);
	ClassDB::bind_method(D_METHOD("get_remesh_requests_per_frame"), &VoxelWorld::get_remesh_requests_per_frame);
	ClassDB::bind_method(D_METHOD("set_remeshes_per_frame", "count"), &VoxelWorld::set_remeshes_per_frame);
	ClassDB::bind_method(D_METHOD("get_remeshes_per_frame"), &VoxelWorld::get_remeshes_per_frame);
	ClassDB::bind_method(D_METHOD("set_chunk_integration_time_budget_usec", "usec"), &VoxelWorld::set_chunk_integration_time_budget_usec);
	ClassDB::bind_method(D_METHOD("get_chunk_integration_time_budget_usec"), &VoxelWorld::get_chunk_integration_time_budget_usec);

	ClassDB::bind_method(D_METHOD("set_block_size", "size"), &VoxelWorld::set_block_size);
	ClassDB::bind_method(D_METHOD("get_block_size"), &VoxelWorld::get_block_size);

	ClassDB::bind_method(D_METHOD("set_sea_level", "level"), &VoxelWorld::set_sea_level);
	ClassDB::bind_method(D_METHOD("get_sea_level"), &VoxelWorld::get_sea_level);

	ClassDB::bind_method(D_METHOD("set_texture_filter", "filter"), &VoxelWorld::set_texture_filter);
	ClassDB::bind_method(D_METHOD("get_texture_filter"), &VoxelWorld::get_texture_filter);

	ClassDB::bind_method(D_METHOD("set_block_registry", "registry"), &VoxelWorld::set_block_registry);
	ClassDB::bind_method(D_METHOD("get_block_registry"), &VoxelWorld::get_block_registry);

	ClassDB::bind_method(D_METHOD("set_biome_registry", "registry"), &VoxelWorld::set_biome_registry);
	ClassDB::bind_method(D_METHOD("get_biome_registry"), &VoxelWorld::get_biome_registry);

	ClassDB::bind_method(D_METHOD("set_structure_registry", "registry"), &VoxelWorld::set_structure_registry);
	ClassDB::bind_method(D_METHOD("get_structure_registry"), &VoxelWorld::get_structure_registry);

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
	ClassDB::bind_method(D_METHOD("add_world_object", "type", "block_pos", "rotation_y", "state", "blocking"), &VoxelWorld::add_world_object, DEFVAL(0.0f), DEFVAL(Dictionary()), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("remove_world_object", "id"), &VoxelWorld::remove_world_object);
	ClassDB::bind_method(D_METHOD("get_world_object", "id"), &VoxelWorld::get_world_object);
	ClassDB::bind_method(D_METHOD("set_world_object_state", "id", "state"), &VoxelWorld::set_world_object_state);
	ClassDB::bind_method(D_METHOD("set_world_object_blocking", "id", "blocking"), &VoxelWorld::set_world_object_blocking);
	ClassDB::bind_method(D_METHOD("get_world_objects_in_chunk", "chunk_pos"), &VoxelWorld::get_world_objects_in_chunk);
	ClassDB::bind_method(D_METHOD("world_to_block_pos", "world_pos"), &VoxelWorld::world_to_block_pos);
	ClassDB::bind_method(D_METHOD("block_to_world_pos", "block_pos"), &VoxelWorld::block_to_world_pos);
	ClassDB::bind_method(D_METHOD("raycast_block", "origin", "direction", "max_distance"), &VoxelWorld::raycast_block, DEFVAL(10.0f));
	ClassDB::bind_method(D_METHOD("request_chunks_around", "world_position", "radius"), &VoxelWorld::request_chunks_around, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_chunk_region_load_status", "world_position", "radius"), &VoxelWorld::get_chunk_region_load_status, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("is_chunk_region_loaded", "world_position", "radius"), &VoxelWorld::is_chunk_region_loaded, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_debug_metrics"), &VoxelWorld::get_debug_metrics);
	ClassDB::bind_method(D_METHOD("move_body", "body", "velocity", "delta"), &VoxelWorld::move_body);
	ClassDB::bind_method(D_METHOD("create_world_save", "save_dir", "display_name", "save_seed", "player_state", "character_state"), &VoxelWorld::create_world_save, DEFVAL(-1), DEFVAL(Dictionary()), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("load_world_save", "save_dir"), &VoxelWorld::load_world_save);
	ClassDB::bind_method(D_METHOD("save_world_state", "player_state", "character_state", "max_dirty_chunks"), &VoxelWorld::save_world_state, DEFVAL(Dictionary()), DEFVAL(Dictionary()), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("flush_world_save_dirty_chunks", "max_chunks"), &VoxelWorld::flush_world_save_dirty_chunks, DEFVAL(1));
	ClassDB::bind_method(D_METHOD("get_world_save_dirty_chunk_count"), &VoxelWorld::get_world_save_dirty_chunk_count);
	ClassDB::bind_method(D_METHOD("close_world_save"), &VoxelWorld::close_world_save);
	ClassDB::bind_method(D_METHOD("is_world_save_loaded"), &VoxelWorld::is_world_save_loaded);
	ClassDB::bind_method(D_METHOD("get_world_save_metadata"), &VoxelWorld::get_world_save_metadata);
	ClassDB::bind_method(D_METHOD("get_world_save_player_state"), &VoxelWorld::get_world_save_player_state);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed", PROPERTY_HINT_RANGE, "-1,2147483647,1"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_load_radius", PROPERTY_HINT_RANGE, "2,16,1"), "set_chunk_load_radius", "get_chunk_load_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunks_per_frame", PROPERTY_HINT_RANGE, "1,16,1"), "set_chunks_per_frame", "get_chunks_per_frame");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_pending_chunk_tasks", PROPERTY_HINT_RANGE, "1,128,1"), "set_max_pending_chunk_tasks", "get_max_pending_chunk_tasks");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_requests_per_frame", PROPERTY_HINT_RANGE, "1,64,1"), "set_chunk_requests_per_frame", "get_chunk_requests_per_frame");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "remesh_requests_per_frame", PROPERTY_HINT_RANGE, "0,64,1"), "set_remesh_requests_per_frame", "get_remesh_requests_per_frame");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "remeshes_per_frame", PROPERTY_HINT_RANGE, "1,16,1"), "set_remeshes_per_frame", "get_remeshes_per_frame");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_integration_time_budget_usec", PROPERTY_HINT_RANGE, "0,10000,100,suffix:usec"), "set_chunk_integration_time_budget_usec", "get_chunk_integration_time_budget_usec");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "block_size", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_block_size", "get_block_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sea_level", PROPERTY_HINT_RANGE, "0,191,1"), "set_sea_level", "get_sea_level");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_texture_filter", "get_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "block_registry", PROPERTY_HINT_RESOURCE_TYPE, "VoxelBlockRegistry"), "set_block_registry", "get_block_registry");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "biome_registry", PROPERTY_HINT_RESOURCE_TYPE, "VoxelBiomeRegistry"), "set_biome_registry", "get_biome_registry");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "structure_registry", PROPERTY_HINT_RESOURCE_TYPE, "VoxelStructureRegistry"), "set_structure_registry", "get_structure_registry");
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
	ADD_SIGNAL(MethodInfo("world_object_added", PropertyInfo(Variant::INT, "object_id"), PropertyInfo(Variant::DICTIONARY, "object")));
	ADD_SIGNAL(MethodInfo("world_object_removed", PropertyInfo(Variant::INT, "object_id")));
	ADD_SIGNAL(MethodInfo("world_object_state_changed", PropertyInfo(Variant::INT, "object_id"), PropertyInfo(Variant::DICTIONARY, "state")));

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
	BIND_CONSTANT(VOXEL_BLOCK_BIOLUMEN_PLANT);
	BIND_CONSTANT(VOXEL_BLOCK_BIO_RESIN);
	BIND_CONSTANT(VOXEL_BLOCK_RUSTED_PANEL);
	BIND_CONSTANT(VOXEL_BLOCK_BEACON_CORE);
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

void VoxelWorld::set_chunks_per_frame(int p_count) {
	chunks_per_frame = CLAMP(p_count, 1, 16);
}

void VoxelWorld::set_max_pending_chunk_tasks(int p_count) {
	max_pending_chunk_tasks = CLAMP(p_count, 1, 128);
}

void VoxelWorld::set_chunk_requests_per_frame(int p_count) {
	chunk_requests_per_frame = CLAMP(p_count, 1, 64);
}

void VoxelWorld::set_remesh_requests_per_frame(int p_count) {
	remesh_requests_per_frame = CLAMP(p_count, 0, 64);
}

void VoxelWorld::set_remeshes_per_frame(int p_count) {
	remeshes_per_frame = CLAMP(p_count, 1, 16);
}

void VoxelWorld::set_chunk_integration_time_budget_usec(int p_usec) {
	chunk_integration_time_budget_usec = CLAMP(p_usec, 0, 10000);
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
	standard_texture_material_cache.clear();
	standard_alpha_texture_material_cache.clear();
}

void VoxelWorld::set_block_registry(const Ref<VoxelBlockRegistry> &p_registry) {
	block_registry = p_registry;
}

Ref<VoxelBlockRegistry> VoxelWorld::get_block_registry() const {
	return block_registry;
}

void VoxelWorld::set_biome_registry(const Ref<VoxelBiomeRegistry> &p_registry) {
	biome_registry = p_registry;
}

Ref<VoxelBiomeRegistry> VoxelWorld::get_biome_registry() const {
	return biome_registry;
}

void VoxelWorld::set_structure_registry(const Ref<VoxelStructureRegistry> &p_registry) {
	structure_registry = p_registry;
}

Ref<VoxelStructureRegistry> VoxelWorld::get_structure_registry() const {
	return structure_registry;
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

void VoxelWorld::_ensure_voxel_global_shader_parameter(float p_value) {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		return;
	}
	const StringName param_name("voxel_sun_intensity");
	if (!voxel_global_shader_parameter_registered) {
		rs->global_shader_parameter_add(param_name, RSE::GLOBAL_VAR_TYPE_FLOAT, p_value);
		voxel_global_shader_parameter_registered = true;
	}
	if (!Math::is_equal_approx(last_voxel_sun_intensity, p_value)) {
		rs->global_shader_parameter_set(param_name, p_value);
		last_voxel_sun_intensity = p_value;
	}
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

void VoxelWorld::_update_day_night_cycle(float p_delta, float p_local_light) {
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
				float fog_end = draw_dist * fog_distance_ratio;
				env->set_fog_depth_begin(fog_end * 0.7f);
				env->set_fog_depth_end(fog_end);
				env->set_fog_depth_curve(2.5f);

				// Fog color matches sky feeling.
				Color day_fog(0.6f, 0.75f, 0.92f);
				Color night_fog(0.02f, 0.02f, 0.08f);
				Color twilight_fog(0.85f, 0.5f, 0.25f);
				Color fog_color = day_fog.lerp(night_fog, 1.0f - day_factor);
				fog_color = fog_color.lerp(twilight_fog, twilight * 0.6f);
				// Darken fog in caves/dark areas based on local light level.
				float fog_brightness = CLAMP(p_local_light, 0.04f, 1.0f);
				fog_color = fog_color * fog_brightness;
				env->set_fog_light_color(fog_color);

				env->set_fog_light_energy(Math::lerp(0.3f, 1.0f, day_factor));
				env->set_fog_sun_scatter(Math::lerp(0.0f, 0.8f, twilight) * p_local_light);
				env->set_fog_density(1.0f);
				// Keep distance fog on world geometry without obscuring the shader sky.
				env->set_fog_sky_affect(0.0f);
			} else {
				env->set_fog_enabled(false);
			}
		}
	}

	// Voxel chunk materials read this as a global shader uniform. Updating a
	// single render-server parameter avoids touching every chunk surface.
	_ensure_voxel_global_shader_parameter(day_factor);
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
			if (!world_save_loaded) {
				time_of_day = start_time_of_day;
			}
			set_process(true);
			_resolve_environment_nodes();
			_initialize_world();
		} break;

		case NOTIFICATION_PROCESS: {
			float delta = get_process_delta_time();

			_integrate_finished_chunks();

			Viewport *vp = get_viewport();
			Camera3D *cam = vp ? vp->get_camera_3d() : nullptr;

			// Sample local light level at camera position for fog brightness.
			float target_local_light = 1.0f;
			if (initialized && cam) {
				Vector3 cam_pos = cam->get_global_position();
				int bx = (int)Math::floor(cam_pos.x / block_size);
				int by = CLAMP((int)Math::floor(cam_pos.y / block_size), 0, VoxelChunk::SIZE_Y - 1);
				int bz = (int)Math::floor(cam_pos.z / block_size);
				int cx = (int)Math::floor((float)bx / VoxelChunk::SIZE_X);
				int cz = (int)Math::floor((float)bz / VoxelChunk::SIZE_Z);
				int lx = ((bx % VoxelChunk::SIZE_X) + VoxelChunk::SIZE_X) % VoxelChunk::SIZE_X;
				int lz = ((bz % VoxelChunk::SIZE_Z) + VoxelChunk::SIZE_Z) % VoxelChunk::SIZE_Z;
				VoxelChunk **cp = loaded_chunks.getptr(Vector2i(cx, cz));
				if (cp && *cp) {
					float day_f = _compute_day_factor(time_of_day);
					float sky = ((*cp)->get_sunlight(lx, by, lz) / 15.0f) * day_f;
					float blk = (*cp)->get_block_light(lx, by, lz) / 15.0f;
					target_local_light = MAX(sky, blk);
				}
			}
			// Smooth transition (speed: ~3 units/sec so ~0.3s to go dark).
			current_local_light = Math::lerp(current_local_light, target_local_light, CLAMP(delta * 3.0f, 0.0f, 1.0f));

			_update_day_night_cycle(delta, current_local_light);

			if (!initialized || !cam) {
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
		seed = effective_seed;
	}
	generator->set_seed(effective_seed);
	generator->set_sea_level(sea_level);

	if (biome_registry.is_null()) {
		biome_registry.instantiate();
		biome_registry->setup_defaults();
		if (verbose_logging) {
			print_line("[VoxelWorld] No biome registry set - created default biome registry.");
		}
	}

	// If a biome registry is set, convert its biomes to runtime data for the generator.
	if (biome_registry.is_valid() && biome_registry->get_biome_count() > 0) {
		const Vector<VoxelBiomeRegistry::BiomeEntry> &entries = biome_registry->get_biomes();
		Vector<RuntimeBiomeData> runtime_biomes;
		runtime_biomes.resize(entries.size());

		for (int i = 0; i < entries.size(); i++) {
			const VoxelBiomeRegistry::BiomeEntry &be = entries[i];
			RuntimeBiomeData &bd = runtime_biomes.write[i];
			bd.params.height_base = be.height_base;
			bd.params.height_scale = be.height_scale;
			bd.params.detail_scale = be.detail_scale;
			bd.params.density_3d_weight = be.density_3d_weight;
			bd.params.surface_block = (uint16_t)be.surface_block;
			bd.params.subsurface_block = (uint16_t)be.subsurface_block;
			bd.params.shore_block = (uint16_t)be.shore_block;
			bd.params.snow_block = (uint16_t)be.snow_block;
			bd.params.snow_line = be.snow_line;
			bd.params.features = be.features;
			bd.center = Vector2(be.temperature, be.humidity);
		}

		generator->set_biome_data(runtime_biomes);

		if (verbose_logging) {
			print_line("[VoxelWorld] Loaded " + itos(entries.size()) + " biomes from registry.");
		}
	} else {
		// No registry or empty → use built-in defaults.
		generator->setup_default_biomes();
		if (verbose_logging) {
			print_line("[VoxelWorld] Empty biome registry - using built-in biomes.");
		}
	}

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

	if (structure_registry.is_null()) {
		structure_registry.instantiate();
		structure_registry->setup_defaults();
		if (verbose_logging) {
			print_line("[VoxelWorld] No structure registry set - created default restored forest slice.");
		}
	}

	material.instantiate();
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_PIXEL);
	material->set_texture_filter(texture_filter);

	// Create voxel lighting shader + material.
	{
		_ensure_voxel_global_shader_parameter(1.0f);
		voxel_shader.instantiate();
		String shader_code = R"(
shader_type spatial;
render_mode blend_mix, depth_draw_opaque, cull_back, diffuse_burley, specular_schlick_ggx;
uniform sampler2D texture_albedo : source_color, filter_nearest, repeat_enable;
global uniform float voxel_sun_intensity;
uniform bool use_texture = false;
varying vec4 voxel_light;
void vertex() {
	voxel_light = CUSTOM0;
}
void fragment() {
	vec4 base = use_texture ? texture(texture_albedo, UV) : vec4(1.0);
	ALBEDO = base.rgb * COLOR.rgb;
	// Sunlight modulation + AO. Block light is handled by OmniLight3D nodes.
	float sun = voxel_light.r * voxel_sun_intensity;
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
	world_objects.clear();
	object_ids_by_chunk.clear();

	// Safety: remove any block lights not already freed by _unload_chunk.
	for (const KeyValue<Vector3i, OmniLight3D *> &E : block_lights) {
		if (E.value->get_parent() == this) {
			remove_child(E.value);
		}
		memdelete(E.value);
	}
	block_lights.clear();

	for (const KeyValue<Vector3i, Node3D *> &E : block_model_nodes) {
		if (E.value->get_parent() == this) {
			remove_child(E.value);
		}
		memdelete(E.value);
	}
	block_model_nodes.clear();

	if (generator) {
		memdelete(generator);
		generator = nullptr;
	}

	material.unref();
	voxel_shader.unref();
	voxel_untextured_shader_material.unref();
	voxel_textured_shader_material_cache.clear();
	standard_texture_material_cache.clear();
	standard_alpha_texture_material_cache.clear();
	last_voxel_sun_intensity = -1.0f;
	initialized = false;
	last_camera_chunk = Vector2i(INT32_MAX, INT32_MAX);
	if (verbose_logging) {
		print_line("[VoxelWorld] Cleanup complete.");
	}
}

Vector<Vector2i> VoxelWorld::_sorted_chunk_keys_by_distance(const Vector2i &p_center, int p_radius) const {
	Vector<Vector2i> keys;
	keys.reserve((p_radius * 2 + 1) * (p_radius * 2 + 1));
	for (int dx = -p_radius; dx <= p_radius; dx++) {
		for (int dz = -p_radius; dz <= p_radius; dz++) {
			keys.push_back(Vector2i(p_center.x + dx, p_center.y + dz));
		}
	}

	for (int i = 1; i < keys.size(); i++) {
		const Vector2i key = keys[i];
		const int dx = key.x - p_center.x;
		const int dz = key.y - p_center.y;
		const int distance_sq = dx * dx + dz * dz;
		int j = i - 1;
		while (j >= 0) {
			const Vector2i other = keys[j];
			const int other_dx = other.x - p_center.x;
			const int other_dz = other.y - p_center.y;
			const int other_distance_sq = other_dx * other_dx + other_dz * other_dz;
			if (other_distance_sq < distance_sq || (other_distance_sq == distance_sq && (other.x < key.x || (other.x == key.x && other.y <= key.y)))) {
				break;
			}
			keys.write[j + 1] = other;
			j--;
		}
		keys.write[j + 1] = key;
	}
	return keys;
}

int VoxelWorld::_request_missing_chunks_around(const Vector2i &p_center, int p_radius) {
	const int capacity = MAX(0, max_pending_chunk_tasks - pending_chunks.size());
	const int request_budget = MIN(chunk_requests_per_frame, capacity);
	if (request_budget <= 0) {
		return 0;
	}

	int requested_count = 0;
	const Vector<Vector2i> desired_keys = _sorted_chunk_keys_by_distance(p_center, p_radius);
	for (int i = 0; i < desired_keys.size() && requested_count < request_budget; i++) {
		const Vector2i key = desired_keys[i];
		if (!loaded_chunks.has(key) && !pending_chunks.has(key)) {
			_request_chunk(key.x, key.y);
			requested_count++;
		}
	}
	return requested_count;
}

void VoxelWorld::_update_chunks(const Vector3 &p_camera_pos) {
	float chunk_world_size_x = VoxelTerrainGenerator::CHUNK_SIZE_X * block_size;
	float chunk_world_size_z = VoxelTerrainGenerator::CHUNK_SIZE_Z * block_size;

	int cam_cx = (int)Math::floor(p_camera_pos.x / chunk_world_size_x);
	int cam_cz = (int)Math::floor(p_camera_pos.z / chunk_world_size_z);

	Vector2i current_chunk(cam_cx, cam_cz);

	if (current_chunk == last_camera_chunk) {
		_request_missing_chunks_around(current_chunk, chunk_load_radius);
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
		// Erasing the TaskID only makes the job invisible; it does not stop a worker from publishing into this world.
		pending_chunks.erase(pending_to_cancel[i]);
	}

	int requested_count = _request_missing_chunks_around(current_chunk, chunk_load_radius);
	if (requested_count > 0 && verbose_logging) {
		print_line("[VoxelWorld] Requested " + itos(requested_count) + " chunks for background generation. Pending: " + itos(pending_chunks.size()) + ".");
	}
}

int VoxelWorld::_effective_chunk_region_radius(int p_radius) const {
	if (p_radius < 0) {
		return MAX(chunk_load_radius, 0);
	}
	return MAX(p_radius, 0);
}

void VoxelWorld::request_chunks_around(const Vector3 &p_world_position, int p_radius) {
	if (!initialized) {
		_initialize_world();
	}
	if (!initialized) {
		return;
	}

	const int radius = _effective_chunk_region_radius(p_radius);
	const Vector2i center = _world_to_chunk(p_world_position);
	_request_missing_chunks_around(center, radius);
}

Dictionary VoxelWorld::get_chunk_region_load_status(const Vector3 &p_world_position, int p_radius) {
	const int radius = _effective_chunk_region_radius(p_radius);
	const Vector2i center = _world_to_chunk(p_world_position);
	const int total = (radius * 2 + 1) * (radius * 2 + 1);

	HashSet<Vector2i> finished_keys;
	{
		MutexLock lock(finished_mutex);
		for (int i = 0; i < finished_chunks.size(); i++) {
			if (!finished_chunks[i].is_remesh) {
				finished_keys.insert(finished_chunks[i].key);
			}
		}
	}

	int loaded = 0;
	int pending = 0;
	for (int dx = -radius; dx <= radius; dx++) {
		for (int dz = -radius; dz <= radius; dz++) {
			const Vector2i key(center.x + dx, center.y + dz);
			if (loaded_chunks.has(key)) {
				loaded++;
			} else if (pending_chunks.has(key) || finished_keys.has(key)) {
				pending++;
			}
		}
	}

	const int remaining = total - loaded;
	Dictionary status;
	status["total"] = total;
	status["loaded"] = loaded;
	status["pending"] = pending;
	status["remaining"] = remaining;
	status["progress"] = total > 0 ? (float)loaded / (float)total : 1.0f;
	status["ready"] = remaining == 0;
	return status;
}

bool VoxelWorld::is_chunk_region_loaded(const Vector3 &p_world_position, int p_radius) {
	const Dictionary status = get_chunk_region_load_status(p_world_position, p_radius);
	return (bool)status["ready"];
}

Dictionary VoxelWorld::get_debug_metrics() {
	int finished_load_count = 0;
	int finished_remesh_count = 0;
	{
		MutexLock lock(finished_mutex);
		for (int i = 0; i < finished_chunks.size(); i++) {
			if (finished_chunks[i].is_remesh) {
				finished_remesh_count++;
			} else {
				finished_load_count++;
			}
		}
	}

	Dictionary metrics;
	metrics["loaded_chunks"] = loaded_chunks.size();
	metrics["pending_chunk_tasks"] = pending_chunks.size();
	metrics["pending_remesh_tasks"] = pending_remesh.size();
	metrics["finished_chunk_results"] = finished_load_count;
	metrics["finished_remesh_results"] = finished_remesh_count;
	metrics["dirty_light_chunks"] = dirty_light.size();
	metrics["world_objects"] = world_objects.size();
	metrics["dirty_object_chunks"] = dirty_saved_object_chunks.size();
	metrics["max_pending_chunk_tasks"] = max_pending_chunk_tasks;
	metrics["chunk_requests_per_frame"] = chunk_requests_per_frame;
	metrics["chunks_per_frame"] = chunks_per_frame;
	metrics["remesh_requests_per_frame"] = remesh_requests_per_frame;
	metrics["remeshes_per_frame"] = remeshes_per_frame;
	metrics["integration_time_budget_usec"] = chunk_integration_time_budget_usec;
	metrics["last_integrated_loads"] = last_integrated_load_count;
	metrics["last_integrated_remeshes"] = last_integrated_remesh_count;
	metrics["last_remesh_requests"] = last_remesh_request_count;
	metrics["last_integration_time_usec"] = last_integration_time_usec;
	metrics["material_cache_size"] = voxel_textured_shader_material_cache.size() + standard_texture_material_cache.size() + standard_alpha_texture_material_cache.size() + (voxel_untextured_shader_material.is_valid() ? 1 : 0);
	return metrics;
}

// Background chunk generation
void VoxelWorld::_chunk_generation_task(void *p_userdata) {
	ChunkTaskData *data = static_cast<ChunkTaskData *>(p_userdata);
	// This still reads live world state while shutdown can dismantle it; snapshot the inputs or make the lifetime explicit.
	VoxelWorld *world = data->world;

	ChunkTaskResult result;
	result.key = data->key;
	result.is_remesh = data->is_remesh;

	if (data->is_remesh) {
		result.blocks = data->pre_blocks;
		result.light_data = data->pre_light;
	} else {
		result.blocks = world->generator->generate_chunk_data(data->chunk_x, data->chunk_z);
		if (world->world_save_loaded && !world->world_save_dir.is_empty()) {
			Vector<uint16_t> saved_blocks;
			const String chunk_path = VoxelWorldSave::get_chunk_file_path(world->world_save_dir, data->key);
			if (FileAccess::exists(chunk_path) && VoxelWorldSave::load_chunk_file(chunk_path, &saved_blocks) == OK) {
				result.blocks = saved_blocks;
				result.blocks_from_save = true;
			}
		}
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
	if (pending_chunks.size() >= max_pending_chunk_tasks) {
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

Ref<ShaderMaterial> VoxelWorld::_get_voxel_shader_material(const Ref<Texture2D> &p_texture) {
	if (voxel_shader.is_null()) {
		return Ref<ShaderMaterial>();
	}

	if (p_texture.is_valid()) {
		Ref<ShaderMaterial> *cached = voxel_textured_shader_material_cache.getptr(p_texture);
		if (cached != nullptr && cached->is_valid()) {
			return *cached;
		}

		Ref<ShaderMaterial> mat;
		mat.instantiate();
		if (mat.is_null()) {
			return Ref<ShaderMaterial>();
		}
		mat->set_shader(voxel_shader);
		if (mat->get_shader().is_null()) {
			return Ref<ShaderMaterial>();
		}
		mat->set_shader_parameter("texture_albedo", p_texture);
		mat->set_shader_parameter("use_texture", true);
		voxel_textured_shader_material_cache[p_texture] = mat;
		return mat;
	}

	if (voxel_untextured_shader_material.is_null()) {
		voxel_untextured_shader_material.instantiate();
		if (voxel_untextured_shader_material.is_null()) {
			return Ref<ShaderMaterial>();
		}
		voxel_untextured_shader_material->set_shader(voxel_shader);
		if (voxel_untextured_shader_material->get_shader().is_null()) {
			voxel_untextured_shader_material.unref();
			return Ref<ShaderMaterial>();
		}
		voxel_untextured_shader_material->set_shader_parameter("use_texture", false);
	}
	return voxel_untextured_shader_material;
}

Ref<StandardMaterial3D> VoxelWorld::_get_standard_texture_material(const Ref<Texture2D> &p_texture, bool p_alpha_scissor) {
	if (p_texture.is_null()) {
		return Ref<StandardMaterial3D>();
	}

	HashMap<Ref<Texture2D>, Ref<StandardMaterial3D>> &cache = p_alpha_scissor ? standard_alpha_texture_material_cache : standard_texture_material_cache;
	Ref<StandardMaterial3D> *cached = cache.getptr(p_texture);
	if (cached != nullptr && cached->is_valid()) {
		return *cached;
	}

	Ref<StandardMaterial3D> mat;
	mat.instantiate();
	mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, p_texture);
	mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	mat->set_texture_filter(texture_filter);
	if (p_alpha_scissor) {
		mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_SCISSOR);
		mat->set_alpha_scissor_threshold(0.5f);
	}
	cache[p_texture] = mat;
	return mat;
}

void VoxelWorld::_apply_surfaces_to_chunk(VoxelChunk *p_chunk, const Vector<VoxelMesher::MeshSurface> &p_surfaces) {
	if (p_surfaces.size() == 0) {
		MeshInstance3D *mi = p_chunk->get_mesh_instance();
		if (mi) {
			mi->set_mesh(Ref<Mesh>());
		}
		return;
	}

	Ref<ArrayMesh> array_mesh;
	array_mesh.instantiate();

	for (int s = 0; s < p_surfaces.size(); s++) {
		uint64_t fmt_flags = p_surfaces[s].custom_format_flags;
		array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, p_surfaces[s].arrays, Array(), Dictionary(), fmt_flags);
		Ref<Material> surface_material;
		if (p_surfaces[s].shader_material.is_valid()) {
			Ref<ShaderMaterial> mat = p_surfaces[s].shader_material;
			if (p_surfaces[s].texture.is_valid() && mat.is_valid() && mat->get_shader().is_valid()) {
				mat->set_shader_parameter("texture_albedo", p_surfaces[s].texture);
			}
			surface_material = mat;
		} else if (fmt_flags != 0 && voxel_shader.is_valid()) {
			// Surface has CUSTOM0 light data — use voxel lighting shader.
			surface_material = _get_voxel_shader_material(p_surfaces[s].texture);
		}
		if (surface_material.is_null() && p_surfaces[s].texture.is_valid()) {
			int btype = p_surfaces[s].block_type;
			bool alpha_scissor = btype >= 0 && block_registry.is_valid() && block_registry->is_uses_alpha(btype);
			surface_material = _get_standard_texture_material(p_surfaces[s].texture, alpha_scissor);
		}
		if (surface_material.is_null() && material.is_valid()) {
			surface_material = material;
		}
		if (surface_material.is_valid()) {
			array_mesh->surface_set_material(s, surface_material);
		}
	}

	MeshInstance3D *mi = p_chunk->get_mesh_instance();
	if (mi == nullptr) {
		mi = memnew(MeshInstance3D);
		Vector2i cpos = p_chunk->get_chunk_pos();
		float world_x = cpos.x * VoxelChunk::SIZE_X * block_size;
		float world_z = cpos.y * VoxelChunk::SIZE_Z * block_size;
		mi->set_position(Vector3(world_x, 0, world_z));
		p_chunk->set_mesh_instance(mi);
		add_child(mi);
		mi->set_owner(nullptr);
	}
	mi->set_mesh(array_mesh);
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

Node3D *VoxelWorld::_spawn_block_model(const Vector3i &p_block_pos, int p_type) {
	if (block_model_nodes.has(p_block_pos) || block_registry.is_null()) {
		return nullptr;
	}

	const VoxelBlockRegistry::VisualMode mode = block_registry->get_block_visual_mode(p_type);
	if (mode == VoxelBlockRegistry::VISUAL_MODE_VOXEL) {
		return nullptr;
	}

	Node3D *node = nullptr;
	if (mode == VoxelBlockRegistry::VISUAL_MODE_MODEL_MESH) {
		Ref<Mesh> mesh = block_registry->get_block_model_mesh(p_type);
		if (mesh.is_null()) {
			return nullptr;
		}
		MeshInstance3D *mi = memnew(MeshInstance3D);
		mi->set_mesh(mesh);
		node = mi;
	} else if (mode == VoxelBlockRegistry::VISUAL_MODE_MODEL_SCENE) {
		Ref<PackedScene> scene = block_registry->get_block_model_scene(p_type);
		if (scene.is_null() || !scene->can_instantiate()) {
			return nullptr;
		}
		Node *instanced = scene->instantiate();
		node = Object::cast_to<Node3D>(instanced);
		if (node == nullptr) {
			memdelete(instanced);
			return nullptr;
		}
	}

	if (node == nullptr) {
		return nullptr;
	}

	node->set_position(block_to_world_pos(p_block_pos) + block_registry->get_block_model_offset(p_type));
	node->set_rotation_degrees(Vector3(0, block_registry->get_block_model_rotation_y(p_type), 0));
	node->set_scale(block_registry->get_block_model_scale(p_type));
	add_child(node);
	node->set_owner(nullptr);
	block_model_nodes[p_block_pos] = node;
	_play_block_model_animation(p_block_pos, p_type);
	return node;
}

void VoxelWorld::_remove_block_model(const Vector3i &p_block_pos) {
	Node3D **node_ptr = block_model_nodes.getptr(p_block_pos);
	if (node_ptr == nullptr) {
		return;
	}
	Node3D *node = *node_ptr;
	if (node->get_parent() == this) {
		remove_child(node);
	}
	memdelete(node);
	block_model_nodes.erase(p_block_pos);
}

void VoxelWorld::_scan_chunk_for_block_models(VoxelChunk *p_chunk) {
	if (p_chunk == nullptr || block_registry.is_null()) {
		return;
	}
	const Vector<uint16_t> &blocks = p_chunk->get_blocks();
	Vector2i cpos = p_chunk->get_chunk_pos();
	const int base_bx = cpos.x * VoxelChunk::SIZE_X;
	const int base_bz = cpos.y * VoxelChunk::SIZE_Z;
	for (int y = 0; y < VoxelChunk::SIZE_Y; y++) {
		for (int z = 0; z < VoxelChunk::SIZE_Z; z++) {
			for (int x = 0; x < VoxelChunk::SIZE_X; x++) {
				const int type = (int)blocks[VoxelTerrainGenerator::block_index(x, y, z)];
				if (type != VOXEL_BLOCK_AIR && block_registry->get_block_visual_mode(type) != VoxelBlockRegistry::VISUAL_MODE_VOXEL) {
					_spawn_block_model(Vector3i(base_bx + x, y, base_bz + z), type);
				}
			}
		}
	}
}

void VoxelWorld::_remove_block_models_in_chunk(const Vector2i &p_key) {
	Vector<Vector3i> to_remove;
	for (const KeyValue<Vector3i, Node3D *> &E : block_model_nodes) {
		if (_block_to_chunk(E.key) == p_key) {
			to_remove.push_back(E.key);
		}
	}
	for (int i = 0; i < to_remove.size(); i++) {
		_remove_block_model(to_remove[i]);
	}
}

AnimationPlayer *VoxelWorld::_find_block_model_animation_player(Node *p_root) const {
	if (p_root == nullptr) {
		return nullptr;
	}
	AnimationPlayer *root_player = Object::cast_to<AnimationPlayer>(p_root);
	if (root_player != nullptr) {
		return root_player;
	}
	TypedArray<Node> players = p_root->find_children("*", "AnimationPlayer", true, false);
	if (!players.is_empty()) {
		return Object::cast_to<AnimationPlayer>(players[0]);
	}
	return nullptr;
}

StringName VoxelWorld::_select_block_model_animation(const Vector3i &p_block_pos, int p_type) const {
	StringName key("idle");
	const WorldObjectEntry *best_object = nullptr;
	const Vector<int64_t> *same_chunk_ids = object_ids_by_chunk.getptr(_block_to_chunk(p_block_pos));
	if (same_chunk_ids != nullptr) {
		for (int i = 0; i < same_chunk_ids->size(); i++) {
			const WorldObjectEntry *object = world_objects.getptr((*same_chunk_ids)[i]);
			if (object != nullptr && object->block_pos == p_block_pos) {
				best_object = object;
				break;
			}
		}
	}
	if (best_object == nullptr) {
		for (int dz = -1; dz <= 1 && best_object == nullptr; dz++) {
			for (int dx = -1; dx <= 1 && best_object == nullptr; dx++) {
				const Vector<int64_t> *ids = object_ids_by_chunk.getptr(_block_to_chunk(p_block_pos + Vector3i(dx, 0, dz)));
				if (ids == nullptr) {
					continue;
				}
				for (int i = 0; i < ids->size(); i++) {
					const WorldObjectEntry *object = world_objects.getptr((*ids)[i]);
					if (object == nullptr) {
						continue;
					}
					const Vector3i delta = object->block_pos - p_block_pos;
					if (Math::abs(delta.x) <= 1 && Math::abs(delta.y) <= 1 && Math::abs(delta.z) <= 1) {
						best_object = object;
						break;
					}
				}
			}
		}
	}

	if (best_object != nullptr) {
		const Dictionary &state = best_object->state;
		if (state.has("animation")) {
			key = StringName(String(state["animation"]));
		} else if (state.has("open")) {
			key = (bool)state["open"] ? StringName("open") : StringName("idle");
		} else if (state.has("active")) {
			key = (bool)state["active"] ? StringName("active") : StringName("idle");
		}
	}

	Dictionary map = block_registry->get_block_animation_state_map(p_type);
	if (map.has(key)) {
		return StringName(String(map[key]));
	}
	return key;
}

void VoxelWorld::_play_block_model_animation(const Vector3i &p_block_pos, int p_type) {
	Node3D *const *node_ptr = block_model_nodes.getptr(p_block_pos);
	if (node_ptr == nullptr) {
		return;
	}
	AnimationPlayer *player = _find_block_model_animation_player(*node_ptr);
	if (player == nullptr) {
		return;
	}
	const StringName animation = _select_block_model_animation(p_block_pos, p_type);
	if (animation != StringName() && player->has_animation(animation)) {
		player->play(animation);
	}
}

void VoxelWorld::_refresh_block_model_animation_at(const Vector3i &p_block_pos) {
	Node3D *const *node_ptr = block_model_nodes.getptr(p_block_pos);
	if (node_ptr == nullptr) {
		return;
	}
	const int type = get_block_at(block_to_world_pos(p_block_pos));
	if (type != VOXEL_BLOCK_AIR) {
		_play_block_model_animation(p_block_pos, type);
	}
}

void VoxelWorld::_refresh_block_model_animations_near_object(const WorldObjectEntry &p_object) {
	for (int dz = -1; dz <= 1; dz++) {
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				_refresh_block_model_animation_at(p_object.block_pos + Vector3i(dx, dy, dz));
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
	last_integrated_load_count = 0;
	last_integrated_remesh_count = 0;
	last_remesh_request_count = 0;
	const uint64_t start_usec = OS::get_singleton()->get_ticks_usec();

	{
		MutexLock lock(finished_mutex);
		if (finished_chunks.is_empty()) {
			last_integration_time_usec = 0;
		} else {
			to_integrate = finished_chunks;
			finished_chunks.clear();
		}
	}

	int integrated = 0;
	int integrated_remesh = 0;
	HashSet<Vector2i> integrated_keys_this_frame;
	auto defer_remaining = [&](int p_from_index) {
		if (p_from_index >= to_integrate.size()) {
			return;
		}
		MutexLock lock(finished_mutex);
		// This reshuffles the whole queue while workers wait on the same lock. Very democratic, not very timely.
		Vector<ChunkTaskResult> newly_finished = finished_chunks;
		finished_chunks.clear();
		for (int j = p_from_index; j < to_integrate.size(); j++) {
			finished_chunks.push_back(to_integrate[j]);
		}
		for (int j = 0; j < newly_finished.size(); j++) {
			finished_chunks.push_back(newly_finished[j]);
		}
	};
	for (int i = 0; i < to_integrate.size(); i++) {
		if (chunk_integration_time_budget_usec > 0 &&
				(integrated + integrated_remesh) > 0 &&
				(int)(OS::get_singleton()->get_ticks_usec() - start_usec) >= chunk_integration_time_budget_usec) {
			defer_remaining(i);
			break;
		}
		const ChunkTaskResult &result = to_integrate[i];

		if (result.is_remesh) {
			if (integrated_remesh >= remeshes_per_frame) {
				defer_remaining(i);
				break;
			}
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
				integrated_remesh++;
				integrated_keys_this_frame.insert(result.key);
			}
			if (chunk_integration_time_budget_usec > 0 &&
					(int)(OS::get_singleton()->get_ticks_usec() - start_usec) >= chunk_integration_time_budget_usec) {
				defer_remaining(i + 1);
				break;
			}
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
		const bool structures_modified = _apply_structures_to_chunk(result.key, chunk, result.blocks_from_save);
		if (structures_modified) {
			_rebuild_chunk_mesh(chunk);
		} else {
			_apply_surfaces_to_chunk(chunk, result.surfaces);
		}
		_scan_chunk_for_lights(chunk);

		const bool objects_loaded = _load_world_objects_for_chunk(result.key);
		if (!objects_loaded) {
			_generate_structure_objects_for_chunk(result.key);
		}
		_scan_chunk_for_block_models(chunk);

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
		integrated_keys_this_frame.insert(result.key);

		if (integrated >= chunks_per_frame) {
			defer_remaining(i + 1);
			break;
		}
		if (chunk_integration_time_budget_usec > 0 &&
				(int)(OS::get_singleton()->get_ticks_usec() - start_usec) >= chunk_integration_time_budget_usec) {
			defer_remaining(i + 1);
			break;
		}
	}

	last_integrated_load_count = integrated;
	last_integrated_remesh_count = integrated_remesh;
	last_integration_time_usec = (int)(OS::get_singleton()->get_ticks_usec() - start_usec);

	if ((integrated + integrated_remesh) > 0 && verbose_logging) {
		print_verbose("[VoxelWorld] Integrated loads=" + itos(integrated) + " remeshes=" + itos(integrated_remesh) + " this frame. Total loaded: " + itos(loaded_chunks.size()) + ".");
	}

	// Process deferred light-dirty chunks: attempt to queue remesh for each.
	// Chunks that still can't be queued stay dirty for the next frame.
	if (!dirty_light.is_empty()) {
		HashSet<Vector2i> still_dirty;
		for (const Vector2i &key : dirty_light) {
			if (integrated_keys_this_frame.has(key)) {
				still_dirty.insert(key);
				continue;
			}
			if (pending_chunks.has(key) || pending_remesh.has(key)) {
				still_dirty.insert(key);
				continue;
			}
			if (!loaded_chunks.has(key)) {
				continue; // Not loaded (yet or anymore) — drop it.
			}
			if (last_remesh_request_count >= remesh_requests_per_frame) {
				still_dirty.insert(key);
				continue;
			}
			_request_remesh(key);
			last_remesh_request_count++;
		}
		dirty_light = still_dirty;
	}
}

Error VoxelWorld::_save_chunk_blocks(const Vector2i &p_key, const Vector<uint16_t> &p_blocks) {
	if (!world_save_loaded || world_save_dir.is_empty()) {
		return OK;
	}
	const String chunk_path = VoxelWorldSave::get_chunk_file_path(world_save_dir, p_key);
	return VoxelWorldSave::save_chunk_file(chunk_path, p_blocks);
}

Error VoxelWorld::_save_chunk_if_dirty(const Vector2i &p_key, VoxelChunk *p_chunk, bool p_force) {
	if (!world_save_loaded || world_save_dir.is_empty() || p_chunk == nullptr) {
		return OK;
	}
	if (!p_force && !dirty_saved_chunks.has(p_key)) {
		return OK;
	}
	Error err = _save_chunk_blocks(p_key, p_chunk->get_blocks());
	if (err == OK) {
		dirty_saved_chunks.erase(p_key);
	}
	return err;
}

void VoxelWorld::_queue_chunk_save_snapshot(const Vector2i &p_key, VoxelChunk *p_chunk) {
	if (!world_save_loaded || world_save_dir.is_empty() || p_chunk == nullptr) {
		return;
	}
	if (!dirty_saved_chunks.has(p_key)) {
		return;
	}
	pending_saved_chunk_blocks[p_key] = p_chunk->get_blocks();
	dirty_saved_chunks.erase(p_key);
}

Error VoxelWorld::_flush_dirty_saved_chunks(int p_max_chunks) {
	if (!world_save_loaded || world_save_dir.is_empty()) {
		return OK;
	}
	if (p_max_chunks == 0) {
		return OK;
	}

	Error result = OK;
	int flushed_count = 0;
	const bool unlimited = p_max_chunks < 0;
	auto can_flush_more = [&]() -> bool {
		return unlimited || flushed_count < p_max_chunks;
	};

	Vector<Vector2i> pending_keys;
	for (const KeyValue<Vector2i, Vector<uint16_t>> &E : pending_saved_chunk_blocks) {
		pending_keys.push_back(E.key);
	}
	for (int i = 0; i < pending_keys.size() && can_flush_more(); i++) {
		const Vector2i key = pending_keys[i];
		Vector<uint16_t> *blocks = pending_saved_chunk_blocks.getptr(key);
		if (blocks == nullptr) {
			continue;
		}
		Error err = _save_chunk_blocks(key, *blocks);
		if (err == OK) {
			pending_saved_chunk_blocks.erase(key);
		} else if (result == OK) {
			result = err;
		}
		flushed_count++;
	}
	if (!can_flush_more()) {
		return result;
	}

	Vector<Vector2i> dirty_keys;
	for (const Vector2i &key : dirty_saved_chunks) {
		dirty_keys.push_back(key);
	}
	for (int i = 0; i < dirty_keys.size() && can_flush_more(); i++) {
		VoxelChunk *const *chunk_ptr = loaded_chunks.getptr(dirty_keys[i]);
		if (!chunk_ptr) {
			dirty_saved_chunks.erase(dirty_keys[i]);
			continue;
		}
		Error err = _save_chunk_if_dirty(dirty_keys[i], *chunk_ptr, true);
		if (err != OK && result == OK) {
			result = err;
		}
		flushed_count++;
	}
	return result;
}

void VoxelWorld::_unload_chunk(int p_cx, int p_cz) {
	Vector2i key(p_cx, p_cz);
	VoxelChunk **chunk_ptr = loaded_chunks.getptr(key);
	if (!chunk_ptr) {
		return;
	}

	VoxelChunk *chunk = *chunk_ptr;
	_queue_chunk_save_snapshot(key, chunk);
	_queue_object_save_snapshot(key);
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

	_remove_block_models_in_chunk(key);

	// Remove chunk border visualizer if enabled.
	_despawn_chunk_border(key);
	_remove_world_objects_in_chunk(key, false);

	memdelete(chunk);
	loaded_chunks.erase(key);
	pending_remesh.erase(key); // discard any pending remesh; result will be ignored
	dirty_light.erase(key);
}

Dictionary VoxelWorld::_build_world_save_metadata(const String &p_display_name, int p_resolved_seed, const Dictionary &p_player_state, const Dictionary &p_character_state) const {
	const double now = Time::get_singleton()->get_unix_time_from_system();
	Dictionary metadata;
	metadata["schema_version"] = 1;
	metadata["display_name"] = p_display_name.is_empty() ? String("New World") : p_display_name;
	metadata["seed"] = p_resolved_seed;
	metadata["world_time"] = time_of_day;
	metadata["time_of_day"] = time_of_day;
	metadata["created_unix"] = now;
	metadata["updated_unix"] = now;
	metadata["player_state"] = p_player_state;
	metadata["character_state"] = p_character_state;
	return metadata;
}

Error VoxelWorld::_load_world_save_metadata(const String &p_save_dir) {
	const String metadata_path = VoxelWorldSave::get_metadata_path(p_save_dir);
	ERR_FAIL_COND_V_MSG(!FileAccess::exists(metadata_path), ERR_FILE_NOT_FOUND, vformat("VoxelWorld: save metadata not found: %s", metadata_path));

	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(metadata_path);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("VoxelWorld: failed to load save metadata: %s", metadata_path));

	const int schema_version = (int)config->get_value("world", "schema_version", 0);
	ERR_FAIL_COND_V_MSG(schema_version != 1, ERR_FILE_UNRECOGNIZED, vformat("VoxelWorld: unsupported save schema version %d.", schema_version));

	world_save_metadata.clear();
	world_save_metadata["schema_version"] = schema_version;
	String slot_id = config->get_value("world", "slot_id", p_save_dir.get_file());
	if (slot_id.is_empty()) {
		slot_id = p_save_dir.get_file();
	}
	world_save_metadata["slot_id"] = slot_id;
	world_save_metadata["save_dir"] = p_save_dir;
	world_save_metadata["display_name"] = config->get_value("world", "display_name", String("New World"));
	world_save_metadata["seed"] = (int)config->get_value("world", "seed", -1);
	const double world_time = (double)config->get_value("world", "world_time", config->get_value("world", "time_of_day", (double)start_time_of_day));
	world_save_metadata["world_time"] = world_time;
	world_save_metadata["time_of_day"] = world_time;
	world_save_metadata["created_unix"] = (double)config->get_value("world", "created_unix", 0.0);
	world_save_metadata["updated_unix"] = (double)config->get_value("world", "updated_unix", 0.0);

	world_save_player_state = (Dictionary)config->get_value("player", "state", Dictionary());
	world_save_character_state = (Dictionary)config->get_value("character", "state", Dictionary());
	world_save_metadata["player_state"] = world_save_player_state;
	world_save_metadata["character_state"] = world_save_character_state;
	return OK;
}

Error VoxelWorld::_write_world_save_metadata() {
	if (!world_save_loaded || world_save_dir.is_empty()) {
		return ERR_UNCONFIGURED;
	}
	Error err = VoxelWorldSave::ensure_save_dirs(world_save_dir);
	ERR_FAIL_COND_V(err != OK, err);

	const String metadata_path = VoxelWorldSave::get_metadata_path(world_save_dir);
	const String tmp_path = metadata_path + ".tmp";
	Ref<ConfigFile> config;
	config.instantiate();
	config->set_value("world", "schema_version", (int)world_save_metadata.get("schema_version", 1));
	config->set_value("world", "slot_id", world_save_metadata.get("slot_id", String()));
	config->set_value("world", "display_name", world_save_metadata.get("display_name", String("New World")));
	config->set_value("world", "seed", (int)world_save_metadata.get("seed", seed));
	const double world_time = (double)world_save_metadata.get("world_time", world_save_metadata.get("time_of_day", (double)time_of_day));
	config->set_value("world", "world_time", world_time);
	config->set_value("world", "time_of_day", world_time);
	config->set_value("world", "created_unix", (double)world_save_metadata.get("created_unix", Time::get_singleton()->get_unix_time_from_system()));
	config->set_value("world", "updated_unix", (double)world_save_metadata.get("updated_unix", Time::get_singleton()->get_unix_time_from_system()));
	config->set_value("player", "state", world_save_player_state);
	config->set_value("character", "state", world_save_character_state);
	err = config->save(tmp_path);
	ERR_FAIL_COND_V(err != OK, err);
	if (FileAccess::exists(metadata_path)) {
		err = DirAccess::remove_absolute(metadata_path);
		ERR_FAIL_COND_V(err != OK, err);
	}
	return DirAccess::rename_absolute(tmp_path, metadata_path);
}

Error VoxelWorld::create_world_save(const String &p_save_dir, const String &p_display_name, int p_save_seed, const Dictionary &p_player_state, const Dictionary &p_character_state) {
	ERR_FAIL_COND_V_MSG(p_save_dir.is_empty(), ERR_INVALID_PARAMETER, "VoxelWorld::create_world_save requires a save directory.");

	if (world_save_loaded) {
		save_world_state();
	}
	if (initialized) {
		_cleanup_world();
	}

	Error err = VoxelWorldSave::ensure_save_dirs(p_save_dir);
	ERR_FAIL_COND_V(err != OK, err);

	int resolved_seed = p_save_seed;
	if (resolved_seed < 0) {
		resolved_seed = Math::random(0, 2147483647);
	}
	seed = resolved_seed;
	start_time_of_day = time_of_day;
	world_save_dir = p_save_dir;
	world_save_player_state = p_player_state;
	world_save_character_state = p_character_state;
	world_save_metadata = _build_world_save_metadata(p_display_name, resolved_seed, world_save_player_state, world_save_character_state);
	world_save_metadata["slot_id"] = p_save_dir.get_file();
	world_save_metadata["save_dir"] = p_save_dir;
	world_save_loaded = true;
	dirty_saved_chunks.clear();
	pending_saved_chunk_blocks.clear();
	dirty_saved_object_chunks.clear();
	pending_saved_chunk_objects.clear();
	world_objects.clear();
	object_ids_by_chunk.clear();
	next_world_object_id = 1;

	err = _write_world_save_metadata();
	if (err != OK) {
		world_save_loaded = false;
		return err;
	}
	_initialize_world();
	return OK;
}

Error VoxelWorld::load_world_save(const String &p_save_dir) {
	ERR_FAIL_COND_V_MSG(p_save_dir.is_empty(), ERR_INVALID_PARAMETER, "VoxelWorld::load_world_save requires a save directory.");

	if (world_save_loaded) {
		save_world_state();
	}
	if (initialized) {
		_cleanup_world();
	}

	Error err = _load_world_save_metadata(p_save_dir);
	ERR_FAIL_COND_V(err != OK, err);

	world_save_dir = p_save_dir;
	world_save_loaded = true;
	dirty_saved_chunks.clear();
	pending_saved_chunk_blocks.clear();
	dirty_saved_object_chunks.clear();
	pending_saved_chunk_objects.clear();
	world_objects.clear();
	object_ids_by_chunk.clear();
	next_world_object_id = 1;
	seed = (int)world_save_metadata.get("seed", seed);
	const float loaded_time = (float)(double)world_save_metadata.get("world_time", world_save_metadata.get("time_of_day", (double)start_time_of_day));
	set_start_time_of_day(loaded_time);
	set_time_of_day(loaded_time);
	_initialize_world();
	return OK;
}

Error VoxelWorld::save_world_state(const Dictionary &p_player_state, const Dictionary &p_character_state, int p_max_dirty_chunks) {
	if (!world_save_loaded || world_save_dir.is_empty()) {
		return ERR_UNCONFIGURED;
	}
	if (!p_player_state.is_empty()) {
		world_save_player_state = p_player_state;
	}
	if (!p_character_state.is_empty()) {
		world_save_character_state = p_character_state;
	}
	const double now = Time::get_singleton()->get_unix_time_from_system();
	world_save_metadata["seed"] = seed;
	world_save_metadata["world_time"] = time_of_day;
	world_save_metadata["time_of_day"] = time_of_day;
	world_save_metadata["updated_unix"] = now;
	world_save_metadata["player_state"] = world_save_player_state;
	world_save_metadata["character_state"] = world_save_character_state;

	Error err = _flush_dirty_saved_chunks(p_max_dirty_chunks);
	if (err != OK) {
		return err;
	}
	err = _flush_dirty_saved_object_chunks(p_max_dirty_chunks);
	if (err != OK) {
		return err;
	}
	return _write_world_save_metadata();
}

Error VoxelWorld::flush_world_save_dirty_chunks(int p_max_chunks) {
	Error err = _flush_dirty_saved_chunks(p_max_chunks);
	if (err != OK) {
		return err;
	}
	return _flush_dirty_saved_object_chunks(p_max_chunks);
}

Error VoxelWorld::close_world_save() {
	if (!world_save_loaded) {
		return OK;
	}
	Error err = save_world_state();
	world_save_loaded = false;
	world_save_dir.clear();
	world_save_metadata.clear();
	world_save_player_state.clear();
	world_save_character_state.clear();
	dirty_saved_chunks.clear();
	pending_saved_chunk_blocks.clear();
	dirty_saved_object_chunks.clear();
	pending_saved_chunk_objects.clear();
	world_objects.clear();
	object_ids_by_chunk.clear();
	next_world_object_id = 1;
	return err;
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

Vector2i VoxelWorld::_block_to_chunk(const Vector3i &p_block_pos) const {
	int cx = (int)Math::floor((float)p_block_pos.x / (float)VoxelChunk::SIZE_X);
	int cz = (int)Math::floor((float)p_block_pos.z / (float)VoxelChunk::SIZE_Z);
	return Vector2i(cx, cz);
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

Dictionary VoxelWorld::_world_object_to_dictionary(const WorldObjectEntry &p_object) const {
	Dictionary d;
	d["id"] = p_object.id;
	d["type"] = String(p_object.type);
	d["block_pos"] = p_object.block_pos;
	d["rotation_y"] = p_object.rotation_y;
	d["state"] = p_object.state;
	d["blocking"] = p_object.blocking;
	return d;
}

VoxelWorld::WorldObjectEntry VoxelWorld::_world_object_from_dictionary(const Dictionary &p_dict) const {
	WorldObjectEntry object;
	object.id = (int64_t)p_dict.get("id", (int64_t)0);
	object.type = StringName(String(p_dict.get("type", String())));
	object.block_pos = p_dict.get("block_pos", Vector3i());
	object.rotation_y = (float)p_dict.get("rotation_y", 0.0f);
	object.state = p_dict.get("state", Dictionary());
	object.blocking = (bool)p_dict.get("blocking", false);
	return object;
}

void VoxelWorld::_index_world_object(const WorldObjectEntry &p_object) {
	Vector2i key = _block_to_chunk(p_object.block_pos);
	Vector<int64_t> *ids = object_ids_by_chunk.getptr(key);
	if (ids == nullptr) {
		object_ids_by_chunk[key] = Vector<int64_t>();
		ids = object_ids_by_chunk.getptr(key);
	}
	for (int i = 0; i < ids->size(); i++) {
		if ((*ids)[i] == p_object.id) {
			return;
		}
	}
	ids->push_back(p_object.id);
}

void VoxelWorld::_unindex_world_object(const WorldObjectEntry &p_object) {
	Vector2i key = _block_to_chunk(p_object.block_pos);
	Vector<int64_t> *ids = object_ids_by_chunk.getptr(key);
	if (ids == nullptr) {
		return;
	}
	for (int i = 0; i < ids->size(); i++) {
		if ((*ids)[i] == p_object.id) {
			ids->remove_at(i);
			break;
		}
	}
	if (ids->is_empty()) {
		object_ids_by_chunk.erase(key);
	}
}

int64_t VoxelWorld::_add_world_object_internal(const WorldObjectEntry &p_object, bool p_mark_dirty, bool p_emit_signal) {
	ERR_FAIL_COND_V_MSG(p_object.id == 0, 0, "World object id cannot be 0.");
	if (world_objects.has(p_object.id)) {
		return p_object.id;
	}
	world_objects[p_object.id] = p_object;
	_index_world_object(p_object);
	if (p_object.id >= next_world_object_id) {
		next_world_object_id = p_object.id + 1;
	}
	if (p_mark_dirty && world_save_loaded) {
		dirty_saved_object_chunks.insert(_block_to_chunk(p_object.block_pos));
	}
	if (p_emit_signal) {
		emit_signal("world_object_added", p_object.id, _world_object_to_dictionary(p_object));
	}
	_refresh_block_model_animations_near_object(p_object);
	return p_object.id;
}

bool VoxelWorld::_remove_world_object_internal(int64_t p_id, bool p_mark_dirty, bool p_emit_signal) {
	WorldObjectEntry *object = world_objects.getptr(p_id);
	if (object == nullptr) {
		return false;
	}
	Vector2i key = _block_to_chunk(object->block_pos);
	WorldObjectEntry removed = *object;
	_unindex_world_object(removed);
	world_objects.erase(p_id);
	if (p_mark_dirty && world_save_loaded) {
		dirty_saved_object_chunks.insert(key);
	}
	if (p_emit_signal) {
		emit_signal("world_object_removed", p_id);
	}
	return true;
}

Array VoxelWorld::_get_world_objects_array_for_chunk(const Vector2i &p_key) const {
	Array objects;
	const Vector<int64_t> *ids = object_ids_by_chunk.getptr(p_key);
	if (ids == nullptr) {
		return objects;
	}
	for (int i = 0; i < ids->size(); i++) {
		const WorldObjectEntry *object = world_objects.getptr((*ids)[i]);
		if (object != nullptr) {
			objects.push_back(_world_object_to_dictionary(*object));
		}
	}
	return objects;
}

Error VoxelWorld::_save_chunk_objects(const Vector2i &p_key, const Array &p_objects) {
	if (!world_save_loaded || world_save_dir.is_empty()) {
		return OK;
	}
	const String object_path = VoxelWorldSave::get_chunk_objects_file_path(world_save_dir, p_key);
	return VoxelWorldSave::save_chunk_objects_file(object_path, p_objects);
}

Error VoxelWorld::_save_chunk_objects_if_dirty(const Vector2i &p_key, bool p_force) {
	if (!world_save_loaded || world_save_dir.is_empty()) {
		return OK;
	}
	if (!p_force && !dirty_saved_object_chunks.has(p_key)) {
		return OK;
	}
	Error err = _save_chunk_objects(p_key, _get_world_objects_array_for_chunk(p_key));
	if (err == OK) {
		dirty_saved_object_chunks.erase(p_key);
	}
	return err;
}

void VoxelWorld::_queue_object_save_snapshot(const Vector2i &p_key) {
	if (!world_save_loaded || world_save_dir.is_empty()) {
		return;
	}
	if (!dirty_saved_object_chunks.has(p_key)) {
		return;
	}
	pending_saved_chunk_objects[p_key] = _get_world_objects_array_for_chunk(p_key);
	dirty_saved_object_chunks.erase(p_key);
}

Error VoxelWorld::_flush_dirty_saved_object_chunks(int p_max_chunks) {
	if (!world_save_loaded || world_save_dir.is_empty()) {
		return OK;
	}
	if (p_max_chunks == 0) {
		return OK;
	}

	Error result = OK;
	int flushed_count = 0;
	const bool unlimited = p_max_chunks < 0;
	auto can_flush_more = [&]() -> bool {
		return unlimited || flushed_count < p_max_chunks;
	};

	Vector<Vector2i> pending_keys;
	for (const KeyValue<Vector2i, Array> &E : pending_saved_chunk_objects) {
		pending_keys.push_back(E.key);
	}
	for (int i = 0; i < pending_keys.size() && can_flush_more(); i++) {
		const Vector2i key = pending_keys[i];
		Array *objects = pending_saved_chunk_objects.getptr(key);
		if (objects == nullptr) {
			continue;
		}
		Error err = _save_chunk_objects(key, *objects);
		if (err == OK) {
			pending_saved_chunk_objects.erase(key);
		} else if (result == OK) {
			result = err;
		}
		flushed_count++;
	}
	if (!can_flush_more()) {
		return result;
	}

	Vector<Vector2i> dirty_keys;
	for (const Vector2i &key : dirty_saved_object_chunks) {
		dirty_keys.push_back(key);
	}
	for (int i = 0; i < dirty_keys.size() && can_flush_more(); i++) {
		Error err = _save_chunk_objects_if_dirty(dirty_keys[i], true);
		if (err != OK && result == OK) {
			result = err;
		}
		flushed_count++;
	}
	return result;
}

bool VoxelWorld::_load_world_objects_for_chunk(const Vector2i &p_key) {
	if (!world_save_loaded || world_save_dir.is_empty()) {
		return false;
	}
	const String object_path = VoxelWorldSave::get_chunk_objects_file_path(world_save_dir, p_key);
	if (!FileAccess::exists(object_path)) {
		return false;
	}
	Array loaded_objects;
	if (VoxelWorldSave::load_chunk_objects_file(object_path, &loaded_objects) != OK) {
		return false;
	}
	for (int i = 0; i < loaded_objects.size(); i++) {
		if (loaded_objects[i].get_type() != Variant::DICTIONARY) {
			continue;
		}
		WorldObjectEntry object = _world_object_from_dictionary(loaded_objects[i]);
		if (object.id == 0) {
			object.id = next_world_object_id++;
		}
		_add_world_object_internal(object, false, true);
	}
	return true;
}

void VoxelWorld::_remove_world_objects_in_chunk(const Vector2i &p_key, bool p_emit_signal) {
	Vector<int64_t> ids;
	const Vector<int64_t> *indexed = object_ids_by_chunk.getptr(p_key);
	if (indexed != nullptr) {
		ids = *indexed;
	}
	for (int i = 0; i < ids.size(); i++) {
		_remove_world_object_internal(ids[i], false, p_emit_signal);
	}
}

int64_t VoxelWorld::add_world_object(const StringName &p_type, const Vector3i &p_block_pos, float p_rotation_y, const Dictionary &p_state, bool p_blocking) {
	WorldObjectEntry object;
	object.id = next_world_object_id++;
	object.type = p_type;
	object.block_pos = p_block_pos;
	object.rotation_y = p_rotation_y;
	object.state = p_state;
	object.blocking = p_blocking;
	return _add_world_object_internal(object, true, true);
}

bool VoxelWorld::remove_world_object(int64_t p_id) {
	return _remove_world_object_internal(p_id, true, true);
}

Dictionary VoxelWorld::get_world_object(int64_t p_id) const {
	const WorldObjectEntry *object = world_objects.getptr(p_id);
	if (object == nullptr) {
		return Dictionary();
	}
	return _world_object_to_dictionary(*object);
}

bool VoxelWorld::set_world_object_state(int64_t p_id, const Dictionary &p_state) {
	WorldObjectEntry *object = world_objects.getptr(p_id);
	if (object == nullptr) {
		return false;
	}
	object->state = p_state;
	if (world_save_loaded) {
		dirty_saved_object_chunks.insert(_block_to_chunk(object->block_pos));
	}
	emit_signal("world_object_state_changed", p_id, p_state);
	_refresh_block_model_animations_near_object(*object);
	return true;
}

bool VoxelWorld::set_world_object_blocking(int64_t p_id, bool p_blocking) {
	WorldObjectEntry *object = world_objects.getptr(p_id);
	if (object == nullptr) {
		return false;
	}
	if (object->blocking == p_blocking) {
		return true;
	}
	object->blocking = p_blocking;
	if (world_save_loaded) {
		dirty_saved_object_chunks.insert(_block_to_chunk(object->block_pos));
	}
	return true;
}

Array VoxelWorld::get_world_objects_in_chunk(const Vector2i &p_chunk_pos) const {
	return _get_world_objects_array_for_chunk(p_chunk_pos);
}

static _FORCE_INLINE_ uint32_t _voxel_world_mix_u32(uint32_t v) {
	v ^= v >> 16;
	v *= 0x7feb352dU;
	v ^= v >> 15;
	v *= 0x846ca68bU;
	v ^= v >> 16;
	return v;
}

static bool _packed_string_array_has(const PackedStringArray &p_array, const String &p_value) {
	for (int i = 0; i < p_array.size(); i++) {
		if (p_array[i] == p_value) {
			return true;
		}
	}
	return false;
}

uint32_t VoxelWorld::_hash_structure_anchor(int p_structure_id, const Vector2i &p_anchor_key, uint32_t p_salt) const {
	uint32_t h = (uint32_t)seed;
	h ^= (uint32_t)p_structure_id * 0x9e3779b9U;
	h ^= (uint32_t)p_anchor_key.x * 0x85ebca6bU;
	h ^= (uint32_t)p_anchor_key.y * 0xc2b2ae35U;
	h ^= p_salt * 0x27d4eb2dU;
	return _voxel_world_mix_u32(h);
}

bool VoxelWorld::_structure_should_place(int p_structure_id, const Vector2i &p_anchor_key) const {
	if (structure_registry.is_null() || !structure_registry->has_structure(p_structure_id)) {
		return false;
	}
	const VoxelStructureRegistry::StructureEntry &entry = structure_registry->get_structures()[p_structure_id];
	if (entry.rarity <= 0) {
		return false;
	}
	if (!entry.biome_tags.is_empty() && generator != nullptr) {
		const int bx = p_anchor_key.x * VoxelChunk::SIZE_X + (VoxelChunk::SIZE_X / 2);
		const int bz = p_anchor_key.y * VoxelChunk::SIZE_Z + (VoxelChunk::SIZE_Z / 2);
		const int biome_id = generator->get_biome_index_at(bx, bz);
		String biome_name;
		if (biome_registry.is_valid() && biome_id >= 0 && biome_id < biome_registry->get_biome_count()) {
			biome_name = biome_registry->get_biome_name(biome_id);
		}
		if (biome_name.is_empty() || !_packed_string_array_has(entry.biome_tags, biome_name)) {
			return false;
		}
	}
	return (_hash_structure_anchor(p_structure_id, p_anchor_key) % (uint32_t)entry.rarity) == 0;
}

int VoxelWorld::_select_structure_rotation(const VoxelStructureRegistry::StructureEntry &p_entry, int p_structure_id, const Vector2i &p_anchor_key) const {
	const PackedInt32Array &rotations = p_entry.allowed_rotations;
	if (rotations.is_empty()) {
		return 0;
	}
	const uint32_t h = _hash_structure_anchor(p_structure_id, p_anchor_key, 1);
	int rotation = rotations[h % (uint32_t)rotations.size()];
	rotation %= 360;
	if (rotation < 0) {
		rotation += 360;
	}
	if (rotation == 90 || rotation == 180 || rotation == 270) {
		return rotation;
	}
	return 0;
}

Vector3i VoxelWorld::_rotate_structure_local(const Vector3i &p_local, const Vector3i &p_size, int p_rotation) const {
	switch (p_rotation) {
		case 90:
			return Vector3i(p_size.z - 1 - p_local.z, p_local.y, p_local.x);
		case 180:
			return Vector3i(p_size.x - 1 - p_local.x, p_local.y, p_size.z - 1 - p_local.z);
		case 270:
			return Vector3i(p_local.z, p_local.y, p_size.x - 1 - p_local.x);
		default:
			return p_local;
	}
}

bool VoxelWorld::_apply_structures_to_chunk(const Vector2i &p_key, VoxelChunk *p_chunk, bool p_blocks_from_save) {
	if (p_blocks_from_save || p_chunk == nullptr || structure_registry.is_null() || structure_registry->get_structure_count() == 0 || generator == nullptr) {
		return false;
	}

	bool modified = false;
	const Vector<VoxelStructureRegistry::StructureEntry> &entries = structure_registry->get_structures();
	for (int si = 0; si < entries.size(); si++) {
		const VoxelStructureRegistry::StructureEntry &entry = entries[si];
		if (entry.voxel_data.is_null() || entry.rarity <= 0) {
			continue;
		}
		const Vector3i size = entry.voxel_data->get_size();
		if (size.x <= 0 || size.y <= 0 || size.z <= 0) {
			continue;
		}
		const int candidate_radius = MAX(size.x, size.z) / VoxelChunk::SIZE_X + 2;
		for (int ax = p_key.x - candidate_radius; ax <= p_key.x + candidate_radius; ax++) {
			for (int az = p_key.y - candidate_radius; az <= p_key.y + candidate_radius; az++) {
				const Vector2i anchor_key(ax, az);
				if (!_structure_should_place(si, anchor_key)) {
					continue;
				}

				const uint32_t h = _hash_structure_anchor(si, anchor_key, 2);
				const int local_x = (int)(h % VoxelChunk::SIZE_X);
				const int local_z = (int)((h >> 8) % VoxelChunk::SIZE_Z);
				const int anchor_x = anchor_key.x * VoxelChunk::SIZE_X + local_x;
				const int anchor_z = anchor_key.y * VoxelChunk::SIZE_Z + local_z;
				const int anchor_y = CLAMP(generator->get_surface_y_at(anchor_x, anchor_z) + 1, 1, VoxelChunk::SIZE_Y - 1);
				const int rotation = _select_structure_rotation(entry, si, anchor_key);
				const Vector3i rotated_anchor = _rotate_structure_local(entry.anchor, size, rotation);
				const Vector3i world_origin(anchor_x - rotated_anchor.x, anchor_y - rotated_anchor.y, anchor_z - rotated_anchor.z);

				const Vector<uint16_t> &template_blocks = entry.voxel_data->get_blocks_array();
				for (int y = 0; y < size.y; y++) {
					for (int z = 0; z < size.z; z++) {
						for (int x = 0; x < size.x; x++) {
							const int template_index = entry.voxel_data->index_of(x, y, z);
							const uint16_t block_id = template_blocks[template_index];
							if (block_id == VOXEL_BLOCK_AIR) {
								continue;
							}
							const Vector3i rotated = _rotate_structure_local(Vector3i(x, y, z), size, rotation);
							const Vector3i world_pos = world_origin + rotated;
							if (_block_to_chunk(world_pos) != p_key || world_pos.y < 0 || world_pos.y >= VoxelChunk::SIZE_Y) {
								continue;
							}
							const int lx = world_pos.x - p_key.x * VoxelChunk::SIZE_X;
							const int lz = world_pos.z - p_key.y * VoxelChunk::SIZE_Z;
							if (lx < 0 || lx >= VoxelChunk::SIZE_X || lz < 0 || lz >= VoxelChunk::SIZE_Z) {
								continue;
							}
							p_chunk->set_block(lx, world_pos.y, lz, block_id);
							modified = true;
						}
					}
				}
			}
		}
	}
	return modified;
}

void VoxelWorld::_generate_structure_objects_for_chunk(const Vector2i &p_key) {
	if (structure_registry.is_null() || structure_registry->get_structure_count() == 0 || generator == nullptr) {
		return;
	}
	const Vector<VoxelStructureRegistry::StructureEntry> &entries = structure_registry->get_structures();
	for (int si = 0; si < entries.size(); si++) {
		const VoxelStructureRegistry::StructureEntry &entry = entries[si];
		if (entry.world_object_type == StringName() || entry.rarity <= 0) {
			continue;
		}
		const Vector3i size = entry.voxel_data.is_valid() ? entry.voxel_data->get_size() : entry.size;
		const int candidate_radius = MAX(MAX(size.x, size.z), 1) / VoxelChunk::SIZE_X + 2;
		for (int ax = p_key.x - candidate_radius; ax <= p_key.x + candidate_radius; ax++) {
			for (int az = p_key.y - candidate_radius; az <= p_key.y + candidate_radius; az++) {
				const Vector2i anchor_key(ax, az);
				if (!_structure_should_place(si, anchor_key)) {
					continue;
				}
				const uint32_t h = _hash_structure_anchor(si, anchor_key, 2);
				const int local_x = (int)(h % VoxelChunk::SIZE_X);
				const int local_z = (int)((h >> 8) % VoxelChunk::SIZE_Z);
				const int anchor_x = anchor_key.x * VoxelChunk::SIZE_X + local_x;
				const int anchor_z = anchor_key.y * VoxelChunk::SIZE_Z + local_z;
				const int anchor_y = CLAMP(generator->get_surface_y_at(anchor_x, anchor_z) + 1, 1, VoxelChunk::SIZE_Y - 1);
				const Vector3i object_pos(anchor_x, anchor_y, anchor_z);
				if (_block_to_chunk(object_pos) != p_key) {
					continue;
				}

				const uint32_t id_hi = _hash_structure_anchor(si, anchor_key, 100);
				const uint32_t id_lo = _hash_structure_anchor(si, anchor_key, 101);
				int64_t object_id = -((int64_t)((((uint64_t)id_hi) << 32) | id_lo) & 0x7FFFFFFFFFFFFFFFLL);
				if (object_id == 0) {
					object_id = -1;
				}
				Dictionary state = entry.world_object_state;
				state["structure_name"] = entry.name;
				state["generated"] = true;

				WorldObjectEntry object;
				object.id = object_id;
				object.type = entry.world_object_type;
				object.block_pos = object_pos;
				object.rotation_y = (float)_select_structure_rotation(entry, si, anchor_key);
				object.state = state;
				object.blocking = entry.world_object_blocking;
				_add_world_object_internal(object, false, true);
			}
		}
	}
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
	Vector3i block_pos(
			chunk_key.x * VoxelChunk::SIZE_X + local.x,
			local.y,
			chunk_key.y * VoxelChunk::SIZE_Z + local.z);
	if (world_save_loaded && old_id != p_block_id) {
		dirty_saved_chunks.insert(chunk_key);
	}

	// Manage OmniLight3D for emissive blocks (e.g. torches).
	{
		if (block_registry->get_emission(old_id) > 0) {
			_remove_block_light(block_pos);
		}
		if (block_registry->get_emission(p_block_id) > 0) {
			_spawn_block_light(block_pos, p_block_id);
		}
	}

	if (old_id != p_block_id) {
		_remove_block_model(block_pos);
		if (p_block_id != VOXEL_BLOCK_AIR && block_registry->get_block_visual_mode(p_block_id) != VoxelBlockRegistry::VISUAL_MODE_VOXEL) {
			_spawn_block_model(block_pos, p_block_id);
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
	if (biome_registry.is_valid() && index < biome_registry->get_biome_count()) {
		return biome_registry->get_biome_name(index);
	}
	// Fallback names for the built-in default biome.
	static const char *fallback_names[] = { "Meadow" };
	if (index < 1) {
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

		const int min_cx = (int)Math::floor((float)min_bx / (float)VoxelChunk::SIZE_X);
		const int max_cx = (int)Math::floor((float)max_bx / (float)VoxelChunk::SIZE_X);
		const int min_cz = (int)Math::floor((float)min_bz / (float)VoxelChunk::SIZE_Z);
		const int max_cz = (int)Math::floor((float)max_bz / (float)VoxelChunk::SIZE_Z);
		for (int cz = min_cz; cz <= max_cz; cz++) {
			for (int cx = min_cx; cx <= max_cx; cx++) {
				const Vector<int64_t> *ids = object_ids_by_chunk.getptr(Vector2i(cx, cz));
				if (ids == nullptr) {
					continue;
				}
				for (int i = 0; i < ids->size(); i++) {
					const WorldObjectEntry *object = world_objects.getptr((*ids)[i]);
					if (object == nullptr || !object->blocking) {
						continue;
					}
					AABB object_aabb(Vector3(object->block_pos.x * bs, object->block_pos.y * bs, object->block_pos.z * bs), Vector3(bs, bs, bs));
					if (!swept.intersects(object_aabb)) {
						continue;
					}
					if (axis == 0) {
						if (axis_move > 0) {
							new_pos.x = object_aabb.position.x - size.x;
						} else {
							new_pos.x = object_aabb.position.x + bs;
						}
						vel.x = 0;
					} else if (axis == 1) {
						if (axis_move < 0) {
							new_pos.y = object_aabb.position.y + bs;
							on_ground = true;
						} else {
							new_pos.y = object_aabb.position.y - size.y;
						}
						vel.y = 0;
					} else {
						if (axis_move > 0) {
							new_pos.z = object_aabb.position.z - size.z;
						} else {
							new_pos.z = object_aabb.position.z + bs;
						}
						vel.z = 0;
					}
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
