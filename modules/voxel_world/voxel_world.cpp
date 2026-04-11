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

	ClassDB::bind_method(D_METHOD("set_alpha_block_flags", "flags"), &VoxelWorld::set_alpha_block_flags);
	ClassDB::bind_method(D_METHOD("get_alpha_block_flags"), &VoxelWorld::get_alpha_block_flags);

	ClassDB::bind_method(D_METHOD("set_block_registry", "registry"), &VoxelWorld::set_block_registry);
	ClassDB::bind_method(D_METHOD("get_block_registry"), &VoxelWorld::get_block_registry);

	ClassDB::bind_method(D_METHOD("set_biome_registry", "registry"), &VoxelWorld::set_biome_registry);
	ClassDB::bind_method(D_METHOD("get_biome_registry"), &VoxelWorld::get_biome_registry);

	ClassDB::bind_method(D_METHOD("set_verbose_logging", "enabled"), &VoxelWorld::set_verbose_logging);
	ClassDB::bind_method(D_METHOD("get_verbose_logging"), &VoxelWorld::get_verbose_logging);

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
	ADD_PROPERTY(PropertyInfo(Variant::INT, "alpha_block_flags", PROPERTY_HINT_FLAGS, "Air,Grass,Dirt,Stone,Sand,Water,Snow,Wood,Leaves,Bedrock"), "set_alpha_block_flags", "get_alpha_block_flags");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "block_registry", PROPERTY_HINT_RESOURCE_TYPE, "VoxelBlockRegistry"), "set_block_registry", "get_block_registry");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "biome_registry", PROPERTY_HINT_RESOURCE_TYPE, "VoxelBiomeRegistry"), "set_biome_registry", "get_biome_registry");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "verbose_logging"), "set_verbose_logging", "get_verbose_logging");

	ADD_SIGNAL(MethodInfo("block_placed", PropertyInfo(Variant::VECTOR3I, "block_pos"), PropertyInfo(Variant::INT, "block_id")));
	ADD_SIGNAL(MethodInfo("block_broken", PropertyInfo(Variant::VECTOR3I, "block_pos"), PropertyInfo(Variant::INT, "old_block_id")));

	BIND_ENUM_CONSTANT(VOXEL_BLOCK_AIR);
	BIND_ENUM_CONSTANT(VOXEL_BLOCK_GRASS);
	BIND_ENUM_CONSTANT(VOXEL_BLOCK_DIRT);
	BIND_ENUM_CONSTANT(VOXEL_BLOCK_STONE);
	BIND_ENUM_CONSTANT(VOXEL_BLOCK_SAND);
	BIND_ENUM_CONSTANT(VOXEL_BLOCK_WATER);
	BIND_ENUM_CONSTANT(VOXEL_BLOCK_SNOW);
	BIND_ENUM_CONSTANT(VOXEL_BLOCK_WOOD);
	BIND_ENUM_CONSTANT(VOXEL_BLOCK_LEAVES);
	BIND_ENUM_CONSTANT(VOXEL_BLOCK_BEDROCK);
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

void VoxelWorld::set_alpha_block_flags(uint32_t p_flags) {
	alpha_block_flags = p_flags;
}

void VoxelWorld::set_block_registry(const Ref<VoxelBlockRegistry> &p_registry) {
	block_registry = p_registry;
}

Ref<VoxelBlockRegistry> VoxelWorld::get_block_registry() const {
	return block_registry;
}

void VoxelWorld::set_biome_registry(const Ref<VoxelBiomeRegistry> &p_registry) {
	biome_registry = p_registry;
	if (generator) {
		generator->set_biome_registry(biome_registry);
	}
}

Ref<VoxelBiomeRegistry> VoxelWorld::get_biome_registry() const {
	return biome_registry;
}

void VoxelWorld::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_editor_hint()) {
				if (verbose_logging) {
					print_line("[VoxelWorld] Editor mode — skipping generation.");
				}
				return;
			}
			if (verbose_logging) {
				print_line("[VoxelWorld] NOTIFICATION_READY received, initializing...");
			}
			set_process(true);
			_initialize_world();
		} break;

		case NOTIFICATION_PROCESS: {
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
	generator->set_biome_registry(biome_registry);

	if (verbose_logging) {
		print_line("[VoxelWorld] Seed: " + itos(effective_seed) + ", sea_level: " + itos(sea_level) + ", block_size: " + rtos(block_size) + ", load_radius: " + itos(chunk_load_radius));
	}

	// If no registry assigned, create one with defaults.
	if (block_registry.is_null()) {
		block_registry.instantiate();
		block_registry->setup_defaults();
		if (verbose_logging) {
			print_line("[VoxelWorld] No block registry set — created default registry with " + itos(block_registry->get_block_count()) + " blocks.");
		}
	}

	material.instantiate();
	material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	material->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_PIXEL);
	material->set_texture_filter(texture_filter);

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

	if (generator) {
		memdelete(generator);
		generator = nullptr;
	}

	material.unref();
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
	} else {
		result.blocks = world->generator->generate_chunk_data(data->chunk_x, data->chunk_z);
	}

	// Build NeighborBlocks from snapshots captured on the main thread at queue time.
	VoxelMesher::NeighborBlocks nb;
	if (data->neighbor_px.size() > 0) { nb.px = data->neighbor_px.ptr(); }
	if (data->neighbor_nx.size() > 0) { nb.nx = data->neighbor_nx.ptr(); }
	if (data->neighbor_pz.size() > 0) { nb.pz = data->neighbor_pz.ptr(); }
	if (data->neighbor_nz.size() > 0) { nb.nz = data->neighbor_nz.ptr(); }

	result.surfaces = VoxelMesher::build_chunk_mesh(result.blocks, world->block_size, world->block_registry, world->alpha_block_flags, nb);

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
	auto snapshot_nb = [&](int dx, int dz) -> Vector<uint8_t> {
		VoxelChunk *const *nb = loaded_chunks.getptr(Vector2i(p_cx + dx, p_cz + dz));
		return nb ? (*nb)->get_blocks() : Vector<uint8_t>();
	};
	task_data->neighbor_px = snapshot_nb(1, 0);
	task_data->neighbor_nx = snapshot_nb(-1, 0);
	task_data->neighbor_pz = snapshot_nb(0, 1);
	task_data->neighbor_nz = snapshot_nb(0, -1);

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
		array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, p_surfaces[s].arrays);
		if (p_surfaces[s].shader_material.is_valid()) {
			Ref<ShaderMaterial> mat = p_surfaces[s].shader_material;
			if (p_surfaces[s].texture.is_valid() && mat->get_shader().is_valid()) {
				mat->set_shader_parameter("texture_albedo", p_surfaces[s].texture);
			}
			array_mesh->surface_set_material(s, mat);
		} else if (p_surfaces[s].texture.is_valid()) {
			Ref<StandardMaterial3D> mat;
			mat.instantiate();
			mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, p_surfaces[s].texture);
			mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
			mat->set_texture_filter(texture_filter);

			int btype = p_surfaces[s].block_type;
			if (btype >= 0 && (alpha_block_flags & (1 << btype))) {
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

// Rebuild a chunk mesh synchronously on the main thread (used in set_block_at for immediate feedback).
void VoxelWorld::_rebuild_chunk_mesh(VoxelChunk *p_chunk) {
	Vector2i cpos = p_chunk->get_chunk_pos();
	VoxelMesher::NeighborBlocks nb;
	auto snap = [&](int dx, int dz) -> const uint8_t * {
		VoxelChunk *const *n = loaded_chunks.getptr(Vector2i(cpos.x + dx, cpos.y + dz));
		return n ? (*n)->get_blocks().ptr() : nullptr;
	};
	nb.px = snap(1, 0);
	nb.nx = snap(-1, 0);
	nb.pz = snap(0, 1);
	nb.nz = snap(0, -1);
	Vector<VoxelMesher::MeshSurface> surfaces = VoxelMesher::build_chunk_mesh(
			p_chunk->get_blocks(), block_size, block_registry, alpha_block_flags, nb);
	_apply_surfaces_to_chunk(p_chunk, surfaces);
}

// Queue an async background remesh for an already-loaded chunk.
void VoxelWorld::_request_remesh(const Vector2i &p_key) {
	// Skip if a regular load or remesh is already in flight for this key.
	if (pending_chunks.has(p_key) || pending_remesh.has(p_key)) {
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

	auto snapshot_nb = [&](int dx, int dz) -> Vector<uint8_t> {
		VoxelChunk *const *nb = loaded_chunks.getptr(Vector2i(p_key.x + dx, p_key.y + dz));
		return nb ? (*nb)->get_blocks() : Vector<uint8_t>();
	};
	task_data->neighbor_px = snapshot_nb(1, 0);
	task_data->neighbor_nx = snapshot_nb(-1, 0);
	task_data->neighbor_pz = snapshot_nb(0, 1);
	task_data->neighbor_nz = snapshot_nb(0, -1);

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

		// Register the chunk and apply the pre-built surfaces (built off-thread).
		loaded_chunks[result.key] = chunk;
		_apply_surfaces_to_chunk(chunk, result.surfaces);

		// Queue async remesh for the four loaded cardinal neighbours so their
		// border faces are updated with awareness of this newly arrived chunk.
		const Vector2i dirs[4] = {
			Vector2i(1, 0), Vector2i(-1, 0), Vector2i(0, 1), Vector2i(0, -1)
		};
		for (int d = 0; d < 4; d++) {
			_request_remesh(result.key + dirs[d]);
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
	chunk->set_block(local.x, local.y, local.z, (VoxelBlockType)p_block_id);

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
	return VoxelBlockData::get_block_name((VoxelBlockType)block_id);
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
		Ref<VoxelBiomeData> bd = biome_registry->get_biome(index);
		if (bd.is_valid()) {
			return bd->get_biome_name();
		}
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

					AABB block_aabb(Vector3(bx * bs, by * bs, bz * bs), Vector3(bs, bs, bs));
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
