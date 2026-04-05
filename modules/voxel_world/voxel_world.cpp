#include "voxel_world.h"

#include "core/config/engine.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
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

	ClassDB::bind_method(D_METHOD("get_block_at", "world_pos"), &VoxelWorld::get_block_at);
	ClassDB::bind_method(D_METHOD("set_block_at", "world_pos", "block_id"), &VoxelWorld::set_block_at);
	ClassDB::bind_method(D_METHOD("get_block_name_at", "world_pos"), &VoxelWorld::get_block_name_at);
	ClassDB::bind_method(D_METHOD("world_to_block_pos", "world_pos"), &VoxelWorld::world_to_block_pos);
	ClassDB::bind_method(D_METHOD("block_to_world_pos", "block_pos"), &VoxelWorld::block_to_world_pos);
	ClassDB::bind_method(D_METHOD("raycast_block", "origin", "direction", "max_distance"), &VoxelWorld::raycast_block, DEFVAL(10.0f));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed", PROPERTY_HINT_RANGE, "-1,2147483647,1"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_load_radius", PROPERTY_HINT_RANGE, "2,16,1"), "set_chunk_load_radius", "get_chunk_load_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "block_size", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_block_size", "get_block_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sea_level", PROPERTY_HINT_RANGE, "0,63,1"), "set_sea_level", "get_sea_level");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_filter", PROPERTY_HINT_ENUM, "Nearest,Linear,Nearest Mipmap,Linear Mipmap,Nearest Mipmap Anisotropic,Linear Mipmap Anisotropic"), "set_texture_filter", "get_texture_filter");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "block_registry", PROPERTY_HINT_RESOURCE_TYPE, "VoxelBlockRegistry"), "set_block_registry", "get_block_registry");

	ADD_SIGNAL(MethodInfo("block_placed", PropertyInfo(Variant::VECTOR3I, "block_pos"), PropertyInfo(Variant::INT, "block_id")));
	ADD_SIGNAL(MethodInfo("block_broken", PropertyInfo(Variant::VECTOR3I, "block_pos"), PropertyInfo(Variant::INT, "old_block_id")));
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

void VoxelWorld::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				print_line("[VoxelWorld] Editor mode — skipping generation.");
				return;
			}
			print_line("[VoxelWorld] NOTIFICATION_READY received, initializing...");
			set_process(true);
			_initialize_world();
		} break;

		case NOTIFICATION_PROCESS: {
			if (!initialized) {
				return;
			}

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
			print_line("[VoxelWorld] NOTIFICATION_EXIT_TREE — cleaning up.");
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

	print_line("[VoxelWorld] Seed: " + itos(effective_seed) + ", sea_level: " + itos(sea_level) + ", block_size: " + rtos(block_size) + ", load_radius: " + itos(chunk_load_radius));

	// If no registry assigned, create one with defaults.
	if (block_registry.is_null()) {
		block_registry.instantiate();
		block_registry->setup_defaults();
		print_line("[VoxelWorld] No block registry set — created default registry with " + itos(block_registry->get_block_count()) + " blocks.");
	}

	material.instantiate();
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_PIXEL);
	material->set_texture_filter(texture_filter);

	initialized = true;

	print_line("[VoxelWorld] World initialized. Generating initial chunks around origin...");
	_update_chunks(Vector3(0, 0, 0));
}

void VoxelWorld::_cleanup_world() {
	Vector<Vector2i> keys;
	for (const KeyValue<Vector2i, VoxelChunk *> &E : loaded_chunks) {
		keys.push_back(E.key);
	}
	print_line("[VoxelWorld] Cleanup: unloading " + itos(keys.size()) + " chunks...");
	for (int i = 0; i < keys.size(); i++) {
		_unload_chunk(keys[i].x, keys[i].y);
	}
	loaded_chunks.clear();

	if (generator) {
		memdelete(generator);
		generator = nullptr;
	}

	material.unref();
	initialized = false;
	last_camera_chunk = Vector2i(INT32_MAX, INT32_MAX);
	print_line("[VoxelWorld] Cleanup complete.");
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
	print_line("[VoxelWorld] Camera moved to chunk (" + itos(cam_cx) + ", " + itos(cam_cz) + "), updating chunks...");
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
	if (to_unload.size() > 0) {
		print_line("[VoxelWorld] Unloading " + itos(to_unload.size()) + " chunks.");
	}
	for (int i = 0; i < to_unload.size(); i++) {
		_unload_chunk(to_unload[i].x, to_unload[i].y);
	}

	int loaded_count = 0;
	for (const KeyValue<Vector2i, bool> &E : desired_chunks) {
		if (!loaded_chunks.has(E.key)) {
			_load_chunk(E.key.x, E.key.y);
			loaded_count++;
		}
	}
	if (loaded_count > 0) {
		print_line("[VoxelWorld] Loaded " + itos(loaded_count) + " new chunks. Total loaded: " + itos(loaded_chunks.size()) + ".");
	}
}

void VoxelWorld::_load_chunk(int p_cx, int p_cz) {
	Vector2i key(p_cx, p_cz);
	if (loaded_chunks.has(key)) {
		return;
	}

	VoxelChunk *chunk = memnew(VoxelChunk);
	chunk->set_chunk_pos(key);

	Vector<uint8_t> blocks = generator->generate_chunk_data(p_cx, p_cz);
	chunk->set_blocks(blocks);

	MeshInstance3D *mi = chunk->build_mesh(block_size, material, block_registry, texture_filter);
	if (mi) {
		add_child(mi);
		mi->set_owner(nullptr);
		print_verbose("[VoxelWorld] Chunk (" + itos(p_cx) + ", " + itos(p_cz) + ") mesh built.");
	} else {
		print_verbose("[VoxelWorld] Chunk (" + itos(p_cx) + ", " + itos(p_cz) + ") — empty, no mesh.");
	}

	loaded_chunks[key] = chunk;
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

	memdelete(chunk);
	loaded_chunks.erase(key);
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
		print_line("[VoxelWorld] set_block_at: chunk not loaded at (" + itos(chunk_key.x) + ", " + itos(chunk_key.y) + ").");
		return;
	}
	Vector3i local = _world_to_local_block(p_world_pos);
	VoxelChunk *chunk = *chunk_ptr;

	int old_id = (int)chunk->get_block(local.x, local.y, local.z);
	chunk->set_block(local.x, local.y, local.z, (VoxelBlockType)p_block_id);

	// Rebuild the chunk mesh.
	chunk->rebuild_mesh(block_size, material, block_registry, texture_filter);

	Vector3i block_pos = world_to_block_pos(p_world_pos);

	if (p_block_id == 0 && old_id != 0) {
		emit_signal("block_broken", block_pos, old_id);
	} else if (p_block_id != 0) {
		emit_signal("block_placed", block_pos, p_block_id);
	}
}

String VoxelWorld::get_block_name_at(const Vector3 &p_world_pos) const {
	int block_id = get_block_at(p_world_pos);
	return VoxelBlockData::get_block_name((VoxelBlockType)block_id);
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

			bool is_solid = VoxelBlockData::is_solid((VoxelBlockType)block_id);

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
