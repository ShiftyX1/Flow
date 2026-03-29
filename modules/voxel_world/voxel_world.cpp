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

	ADD_PROPERTY(PropertyInfo(Variant::INT, "seed", PROPERTY_HINT_RANGE, "-1,2147483647,1"), "set_seed", "get_seed");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "chunk_load_radius", PROPERTY_HINT_RANGE, "2,16,1"), "set_chunk_load_radius", "get_chunk_load_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "block_size", PROPERTY_HINT_RANGE, "0.1,10.0,0.1"), "set_block_size", "get_block_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sea_level", PROPERTY_HINT_RANGE, "0,63,1"), "set_sea_level", "get_sea_level");
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

	material.instantiate();
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_PIXEL);

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

	MeshInstance3D *mi = chunk->build_mesh(block_size, material);
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
